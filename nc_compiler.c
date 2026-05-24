#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 4096
#define MAX_TOKEN 512

typedef struct { char name[MAX_TOKEN]; char type[32]; } VarInfo;
static VarInfo vars[512];
static int var_count = 0;

void reg_var(const char *name, const char *type) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(vars[i].name, name) == 0) {
            strcpy(vars[i].type, type); return;
        }
    strcpy(vars[var_count].name, name);
    strcpy(vars[var_count].type, type);
    var_count++;
}

const char* get_var_type(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(vars[i].name, name) == 0)
            return vars[i].type;
    return "!num!";
}

char* trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;
    return s;
}

int starts_with(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

const char* nc_type_to_c(const char *type) {
    if (strncmp(type, "!sintax!", 8) == 0) return "int";
    if (strncmp(type, "!numD!", 6) == 0)  return "double";
    if (strncmp(type, "!numF!", 6) == 0)  return "float";
    if (strncmp(type, "!num!", 5) == 0)   return "int";
    if (strncmp(type, "!fra!", 5) == 0)   return "char*";
    return "int";
}

const char* fmt_for(const char *expr) {
    if (expr[0] == '"') return "%s";
    const char *t = get_var_type(expr);
    if (strcmp(t, "!fra!") == 0)  return "%s";
    if (strcmp(t, "!numD!") == 0) return "%lf";
    if (strcmp(t, "!numF!") == 0) return "%f";
    return "%d";
}

const char* scanf_fmt(const char *type) {
    if (strcmp(type, "!fra!") == 0)  return "%s";
    if (strcmp(type, "!numD!") == 0) return "%lf";
    if (strcmp(type, "!numF!") == 0) return "%f";
    return "%d";
}

void compile(FILE *in, FILE *out) {
    char line[MAX_LINE], trimmed[MAX_LINE];
    int in_class = 0, main_open = 1;
    int indent = 1;

    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <math.h>\n\n");
    fprintf(out, "// NuclearCloud Language (NC) - v2.0\n\n");
    fprintf(out, "int main() {\n");
    fprintf(out, "    char _nc_buf[4096];\n\n");

    while (fgets(line, MAX_LINE, in)) {
        char tmp[MAX_LINE];
        strcpy(tmp, line);
        strcpy(trimmed, trim(tmp));
        if (!strlen(trimmed)) { fprintf(out, "\n"); continue; }

        // Indentacao
        char pad[64] = "    ";
        for (int i = 1; i < indent; i++) strcat(pad, "    ");

        // Comentario !# ... 
        if (starts_with(trimmed, "!#")) {
            fprintf(out, "%s//%s\n", pad, trimmed + 2);
            continue;
        }

        // Classe: Nome nClass()[
        if (strstr(trimmed, "nClass()[")) {
            if (main_open) { fprintf(out, "    return 0;\n}\n\n"); main_open = 0; }
            char name[MAX_TOKEN];
            char *b = strstr(trimmed, "nClass()[");
            strncpy(name, trimmed, b - trimmed);
            name[b - trimmed] = 0;
            strcpy(name, trim(name));
            fprintf(out, "void %s() {\n", name);
            in_class = 1; indent = 1;
            continue;
        }

        // Funcao: !func! nome(params)[
        if (starts_with(trimmed, "!func!")) {
            if (main_open) { fprintf(out, "    return 0;\n}\n\n"); main_open = 0; }
            char *rest = trim(trimmed + 6);
            char *bracket = strstr(rest, "[");
            if (bracket) *bracket = 0;
            fprintf(out, "void %s {\n", trim(rest));
            in_class = 1; indent = 1;
            continue;
        }

        // Fecha bloco ]
        if (strcmp(trimmed, "]") == 0) {
            indent--;
            if (indent < 1) indent = 1;
            fprintf(out, "%s}\n", pad);
            if (in_class) in_class = 0;
            continue;
        }

        // say = "texto"
        if (starts_with(trimmed, "say")) {
            char *eq = strchr(trimmed, '=');
            if (eq) {
                char *val = trim(eq + 1);
                if (val[0] == '"')
                    fprintf(out, "%sprintf(\"%s\\n\", %s);\n", pad, "%s", val);
                else
                    fprintf(out, "%sprintf(\"%s\\n\", %s);\n", pad, fmt_for(val), val);
            }
            continue;
        }

        // !ask! "pergunta" -> variavel
        if (starts_with(trimmed, "!ask!")) {
            char *rest = trim(trimmed + 5);
            char *arrow = strstr(rest, "->");
            if (arrow) {
                char question[MAX_LINE], varname[MAX_TOKEN];
                strncpy(question, rest, arrow - rest);
                question[arrow - rest] = 0;
                strcpy(question, trim(question));
                strcpy(varname, trim(arrow + 2));
                const char *vtype = get_var_type(varname);
                fprintf(out, "%sprintf(\"%s\\n\", %s);\n", pad, "%s", question);
                if (strcmp(vtype, "!fra!") == 0)
                    fprintf(out, "%sscanf(\"%s\", %s);\n", pad, "%s", varname);
                else
                    fprintf(out, "%sscanf(\"%s\", &%s);\n", pad, scanf_fmt(vtype), varname);
            }
            continue;
        }

        // !jun! arquivo/classe
        if (starts_with(trimmed, "!jun!")) {
            char *t2 = trim(trimmed + 5);
            char fname[MAX_TOKEN]; strcpy(fname, t2);
            char *dot = strstr(fname, ".nc");
            if (dot) *dot = 0;
            fprintf(out, "%s%s();\n", pad, fname);
            continue;
        }

        // !if!
        if (starts_with(trimmed, "!if!")) {
            fprintf(out, "%sif (%s) {\n", pad, trim(trimmed + 4));
            indent++;
            continue;
        }

        // !else!
        if (starts_with(trimmed, "!else!")) {
            char *body = trim(trimmed + 6);
            indent--;
            if (strlen(body) > 0) {
                fprintf(out, "%s} else {\n", pad);
                fprintf(out, "%s    printf(\"%s\\n\", \"%s\");\n", pad, "%s", body);
                fprintf(out, "%s}\n", pad);
            } else {
                fprintf(out, "%s} else {\n", pad);
                indent++;
            }
            continue;
        }

        // !loop! N [  ->  repete N vezes
        if (starts_with(trimmed, "!loop!")) {
            char *rest = trim(trimmed + 6);
            char *bracket = strstr(rest, "[");
            if (bracket) *bracket = 0;
            char *times = trim(rest);
            fprintf(out, "%sfor(int _i=0; _i<%s; _i++) {\n", pad, times);
            indent++;
            continue;
        }

        // !while! condicao [
        if (starts_with(trimmed, "!while!")) {
            char *rest = trim(trimmed + 7);
            char *bracket = strstr(rest, "[");
            if (bracket) *bracket = 0;
            fprintf(out, "%swhile(%s) {\n", pad, trim(rest));
            indent++;
            continue;
        }

        // !ret! valor  ->  return
        if (starts_with(trimmed, "!ret!")) {
            fprintf(out, "%sreturn %s;\n", pad, trim(trimmed + 5));
            continue;
        }

        // !stop!  ->  break
        if (starts_with(trimmed, "!stop!")) {
            fprintf(out, "%sbreak;\n", pad);
            continue;
        }

        // !skip!  ->  continue
        if (starts_with(trimmed, "!skip!")) {
            fprintf(out, "%scontinue;\n", pad);
            continue;
        }

        // Tipos de variaveis
        if (starts_with(trimmed, "!num!") || starts_with(trimmed, "!numD!") ||
            starts_with(trimmed, "!numF!") || starts_with(trimmed, "!fra!") ||
            starts_with(trimmed, "!sintax!")) {
            int tlen = starts_with(trimmed, "!sintax!") ? 8 :
                      (starts_with(trimmed, "!numD!") || starts_with(trimmed, "!numF!")) ? 6 : 5;
            char type[32]; strncpy(type, trimmed, tlen); type[tlen] = 0;
            char rest[MAX_LINE]; strcpy(rest, trim(trimmed + tlen));
            char *eq = strchr(rest, '=');
            if (eq) {
                char name[MAX_TOKEN], val[MAX_TOKEN];
                strncpy(name, rest, eq - rest); name[eq - rest] = 0;
                strcpy(name, trim(name));
                char *rv = trim(eq + 1);
                if (strcmp(rv, "!A") == 0 || strcmp(rv, "true") == 0)  strcpy(val, "1");
                else if (strcmp(rv, "!2") == 0 || strcmp(rv, "false") == 0) strcpy(val, "0");
                else strcpy(val, rv);
                reg_var(name, type);
                fprintf(out, "%s%s %s = %s;\n", pad, nc_type_to_c(type), name, val);
            } else {
                reg_var(rest, type);
                if (strcmp(type, "!fra!") == 0) fprintf(out, "%schar %s[256];\n", pad, rest); else fprintf(out, "%s%s %s;\n", pad, nc_type_to_c(type), rest);
            }
            continue;
        }

        // !! expr  ->  print com tipo automatico
        if (starts_with(trimmed, "!!")) {
            char *expr = trim(trimmed + 2);
            fprintf(out, "%sprintf(\"%s\\n\", %s);\n", pad, fmt_for(expr), expr);
            continue;
        }

        // Atribuicao simples: var = valor
        if (strchr(trimmed, '=') && trimmed[0] != '!' && trimmed[0] != '"') {
            char *eq = strchr(trimmed, '=');
            // Nao e comparacao
            if (*(eq-1) != '!' && *(eq-1) != '<' && *(eq-1) != '>' && *(eq+1) != '=') {
                char name[MAX_TOKEN], val[MAX_TOKEN];
                strncpy(name, trimmed, eq - trimmed); name[eq - trimmed] = 0;
                strcpy(name, trim(name));
                strcpy(val, trim(eq + 1));
                fprintf(out, "%s%s = %s;\n", pad, name, val);
                continue;
            }
        }

        fprintf(out, "%s// NC: %s\n", pad, trimmed);
    }

    if (main_open) fprintf(out, "    return 0;\n}\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "NC Compiler v2.0 - NuclearCloud OS\n");
        fprintf(stderr, "Uso: nc_compiler arquivo.nc [saida]\n");
        return 1;
    }
    char outname[MAX_TOKEN] = "programa";
    if (argc >= 3) strcpy(outname, argv[2]);
    else {
        strcpy(outname, argv[1]);
        char *dot = strstr(outname, ".nc");
        if (dot) *dot = 0;
    }
    char tmpfile[1024];
    snprintf(tmpfile, 1024, "/data/data/com.termux/files/usr/tmp/_nc_%s.c", outname);

    FILE *in = fopen(argv[1], "r");
    if (!in) { fprintf(stderr, "Erro: nao abriu %s\n", argv[1]); return 1; }
    FILE *out_f = fopen(tmpfile, "w");
    if (!out_f) {
        // Fallback pra /data/data/com.termux/files/usr/tmp
        snprintf(tmpfile, 1024, "/data/data/com.termux/files/usr/tmp/_nc_%s.c", outname);
        out_f = fopen(tmpfile, "w");
        if (!out_f) { fprintf(stderr, "Erro: nao criou temporario\n"); fclose(in); return 1; }
    }
    compile(in, out_f);
    fclose(in); fclose(out_f);

    char cmd[2048];
    snprintf(cmd, 2048, "clang -o %s %s -lm 2>&1", outname, tmpfile);
    int ret = system(cmd);
    remove(tmpfile);

    if (ret == 0) printf("✓ Compilado: %s\n", outname);
    else { fprintf(stderr, "✗ Erro ao compilar\n"); return 1; }
    return 0;
}
