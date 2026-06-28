#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 4096
#define MAX_TOKEN 512
#define VERSION "3.0"

typedef struct { char name[MAX_TOKEN]; char type[32]; } VarInfo;
static VarInfo vars[1024];
static int var_count = 0;

void reg_var(const char *name, const char *type) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(vars[i].name, name) == 0) { strcpy(vars[i].type, type); return; }
    strcpy(vars[var_count].name, name);
    strcpy(vars[var_count].type, type);
    var_count++;
}

const char* get_var_type(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(vars[i].name, name) == 0) return vars[i].type;
    return "!num!";
}

char* trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;
    return s;
}

int starts_with(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

const char* nc_type_to_c(const char *t) {
    if (strncmp(t, "!sintax!", 8) == 0) return "int";
    if (strncmp(t, "!numD!", 6) == 0)   return "double";
    if (strncmp(t, "!numF!", 6) == 0)   return "float";
    if (strncmp(t, "!num!", 5) == 0)    return "int";
    if (strncmp(t, "!fra!", 5) == 0)    return "char*";
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


// Converte condicao NC pra C (string comparison fix)
void nc_fix_cond(const char *cond, char *out) {
    // Detecta: var == "string" -> strcmp(var,"string")==0
    // Detecta: var != "string" -> strcmp(var,"string")!=0
    char tmp[4096]; strcpy(tmp, cond);
    char *eq = strstr(tmp, "== \"");
    char *neq = strstr(tmp, "!= \"");
    if (!eq) eq = strstr(tmp, "==\"");
    if (!neq) neq = strstr(tmp, "!=\"");
    
    if (eq || neq) {
        char *op_pos = eq ? eq : neq;
        char is_neq = (neq && (!eq || neq < eq)) ? 1 : 0;
        op_pos = is_neq ? neq : eq;
        
        char varname[512];
        strncpy(varname, tmp, op_pos - tmp);
        varname[op_pos - tmp] = 0;
        // trim varname
        char *v = varname;
        while(*v==' ') v++;
        char *ve = v + strlen(v) - 1;
        while(ve > v && *ve==' ') *ve-- = 0;
        
        char *qstart = strchr(op_pos, '"');
        if (qstart) {
            if (is_neq)
                snprintf(out, 4096, "strcmp(%s,%s)!=0", v, qstart);
            else
                snprintf(out, 4096, "strcmp(%s,%s)==0", v, qstart);
            return;
        }
    }
    strcpy(out, cond);
}

void compile(FILE *in, FILE *out, int is_lib) {
    char line[MAX_LINE], trimmed[MAX_LINE];
    int in_class = 0, main_open = 1, indent = 1;

    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "#include <math.h>\n");
    fprintf(out, "\n// NC Language v%s - NWL-Systems\n\n", VERSION);

    if (is_lib) {
        fprintf(out, "// NCLib\n");
        main_open = 0;
    } else {
        fprintf(out, "int main() {\n");
        fprintf(out, "    char _nc_buf[4096];\n\n");
    }

    while (fgets(line, MAX_LINE, in)) {
        char tmp[MAX_LINE]; strcpy(tmp, line); strcpy(trimmed, trim(tmp));
        if (!strlen(trimmed)) { fprintf(out, "\n"); continue; }
        char pad[256] = "    ";
        for (int i = 1; i < indent; i++) strcat(pad, "    ");

        // Comentario
        if (starts_with(trimmed, "!#")) {
            fprintf(out, "%s//%s\n", pad, trimmed+2); continue;
        }

        // !use! — importa classe, web, NCD etc
        if (starts_with(trimmed, "!use!")) {
            char *lib = trim(trimmed+5);
            char libname[MAX_TOKEN]; strcpy(libname, lib);
            // Remove extensao se tiver
            char *dot = strchr(libname, '.'); if (dot) *dot = 0;
            // NCD.connection — biblioteca oficial
            if (strstr(lib, "NCD.connection")) {
                fprintf(out, "#include \"ncd_connection.h\" // NCD Official\n");
            } else {
                fprintf(out, "#include \"%s.h\" // !use! %s\n", libname, lib);
            }
            continue;
        }

        // Classe
        if (strstr(trimmed, "nClass()[")) {
            if (main_open) { fprintf(out, "    return 0;\n}\n\n"); main_open = 0; }
            char name[MAX_TOKEN]; char *b = strstr(trimmed, "nClass()[");
            strncpy(name, trimmed, b-trimmed); name[b-trimmed]=0; strcpy(name, trim(name));
            fprintf(out, "void %s() {\n", name); in_class=1; indent=1; continue;
        }

        // Funcao
        if (starts_with(trimmed, "!func!")) {
            if (main_open) { fprintf(out, "    return 0;\n}\n\n"); main_open=0; }
            char *rest = trim(trimmed+6); char *bracket = strstr(rest,"["); if(bracket)*bracket=0;
            fprintf(out, "void %s {\n", trim(rest)); in_class=1; indent=1; continue;
        }

        // Funcao com retorno
        if (starts_with(trimmed, "!funcret!")) {
            if (main_open) { fprintf(out, "    return 0;\n}\n\n"); main_open=0; }
            char *rest = trim(trimmed+9); char *bracket = strstr(rest,"["); if(bracket)*bracket=0;
            fprintf(out, "%s {\n", trim(rest)); in_class=1; indent=1; continue;
        }

        // Fecha bloco
        if (strcmp(trimmed,"]")==0) {
            indent--; if(indent<1)indent=1;
            fprintf(out, "%s}\n", pad);
            if(in_class) in_class=0; continue;
        }

        // say
        if (starts_with(trimmed, "say")) {
            char *eq = strchr(trimmed,'=');
            if (eq) { char *val=trim(eq+1);
                if(val[0]=='"') fprintf(out,"%sprintf(\"%%s\\n\",%s);\n",pad,val);
                else fprintf(out,"%sprintf(\"%s\\n\",%s);\n",pad,fmt_for(val),val);
            } continue;
        }

        // !ask!
        if (starts_with(trimmed, "!ask!")) {
            char *rest=trim(trimmed+5); char *arrow=strstr(rest,"->");
            if (arrow) {
                char question[MAX_LINE], varname[MAX_TOKEN];
                strncpy(question,rest,arrow-rest); question[arrow-rest]=0; strcpy(question,trim(question));
                strcpy(varname,trim(arrow+2));
                const char *vtype=get_var_type(varname);
                fprintf(out,"%sprintf(\"%%s\\n\",%s);\n",pad,question);
                if(strcmp(vtype,"!fra!")==0) fprintf(out,"%sscanf(\"%%s\",%s);\n",pad,varname);
                else fprintf(out,"%sscanf(\"%s\",&%s);\n",pad,
                    strcmp(vtype,"!numD!")==0?"%lf":strcmp(vtype,"!numF!")==0?"%f":"%d",varname);
            } continue;
        }

        // !jun!
        if (starts_with(trimmed,"!jun!")) {
            char *t2=trim(trimmed+5); char fname[MAX_TOKEN]; strcpy(fname,t2);
            char *dot=strstr(fname,".nc"); if(dot)*dot=0;
            fprintf(out,"%s%s();\n",pad,fname); continue;
        }

        // Controle de fluxo
        if (starts_with(trimmed,"!if!"))   { char _fc[4096]; nc_fix_cond(trim(trimmed+4),_fc); fprintf(out,"%sif(%s){\n",pad,_fc); indent++; continue; }
        if (starts_with(trimmed,"!elif!")) { char _fc[4096]; nc_fix_cond(trim(trimmed+6),_fc); indent--; fprintf(out,"%s}else if(%s){\n",pad,_fc); indent++; continue; }
        if (starts_with(trimmed,"!else!")) {
            char *body=trim(trimmed+6); indent--;
            if(strlen(body)>0) fprintf(out,"%s}else{\n%s    printf(\"%%s\\n\",\"%s\");\n%s}\n",pad,pad,body,pad);
            else { fprintf(out,"%s}else{\n",pad); indent++; }
            continue;
        }
        if (starts_with(trimmed,"!loop!"))  { char *r=trim(trimmed+6);char *b=strstr(r,"[");if(b)*b=0; fprintf(out,"%sfor(int _i=0;_i<%s;_i++){\n",pad,trim(r));indent++;continue; }
        if (starts_with(trimmed,"!while!")) { char *r=trim(trimmed+7);char *b=strstr(r,"[");if(b)*b=0; fprintf(out,"%swhile(%s){\n",pad,trim(r));indent++;continue; }
        if (starts_with(trimmed,"!ret!"))   { fprintf(out,"%sreturn %s;\n",pad,trim(trimmed+5)); continue; }
        if (starts_with(trimmed,"!stop!"))  { fprintf(out,"%sbreak;\n",pad); continue; }
        if (starts_with(trimmed,"!skip!"))  { fprintf(out,"%scontinue;\n",pad); continue; }

        // Tipos
        if (starts_with(trimmed,"!num!")||starts_with(trimmed,"!numD!")||
            starts_with(trimmed,"!numF!")||starts_with(trimmed,"!fra!")||
            starts_with(trimmed,"!sintax!")) {
            int tlen=starts_with(trimmed,"!sintax!")?8:
                    (starts_with(trimmed,"!numD!")||starts_with(trimmed,"!numF!"))?6:5;
            char type[32]; strncpy(type,trimmed,tlen); type[tlen]=0;
            char rest[MAX_LINE]; strcpy(rest,trim(trimmed+tlen));
            char *eq=strchr(rest,'=');
            if(eq){
                char name[MAX_TOKEN],val[MAX_TOKEN];
                strncpy(name,rest,eq-rest);name[eq-rest]=0;strcpy(name,trim(name));
                char *rv=trim(eq+1);
                if(strcmp(rv,"!A")==0||strcmp(rv,"true")==0) strcpy(val,"1");
                else if(strcmp(rv,"!2")==0||strcmp(rv,"false")==0) strcpy(val,"0");
                else strcpy(val,rv);
                reg_var(name,type);
                fprintf(out,"%s%s %s=%s;\n",pad,nc_type_to_c(type),name,val);
            } else {
                reg_var(rest,type);
                if(strcmp(type,"!fra!")==0) fprintf(out,"%schar %s[1024];\n",pad,rest);
                else fprintf(out,"%s%s %s;\n",pad,nc_type_to_c(type),rest);
            } continue;
        }

        // !!
        if (starts_with(trimmed,"!!")) {
            char *expr=trim(trimmed+2);
            fprintf(out,"%sprintf(\"%s\\n\",%s);\n",pad,fmt_for(expr),expr); continue;
        }

        // Atribuicao simples
        if (strchr(trimmed,'=')&&trimmed[0]!='!'&&trimmed[0]!='"') {
            char *eq=strchr(trimmed,'=');
            if(*(eq-1)!='!'&&*(eq-1)!='<'&&*(eq-1)!='>'&&*(eq+1)!='='){
                char name[MAX_TOKEN],val[MAX_TOKEN];
                strncpy(name,trimmed,eq-trimmed);name[eq-trimmed]=0;strcpy(name,trim(name));
                strcpy(val,trim(eq+1));
                fprintf(out,"%s%s=%s;\n",pad,name,val); continue;
            }
        }

        fprintf(out,"%s// NC: %s\n",pad,trimmed);
    }

    if(main_open) fprintf(out,"    return 0;\n}\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("NC Language v%s - NWL-Systems\n", VERSION);
        printf("Uso: nc arquivo.nc [saida]\n");
        printf("     nc arquivo.ncli [saida]\n");
        return 1;
    }

    char outname[MAX_TOKEN];
    if (argc >= 3) strcpy(outname, argv[2]);
    else { strcpy(outname,argv[1]); char *dot=strstr(outname,".nc");if(dot)*dot=0; }

    int is_lib = strstr(argv[1], ".ncli") != NULL;

    char tmpfile[1024];
    #ifdef _WIN32
        snprintf(tmpfile,1024,"%s\\_nc_%s.c",getenv("TEMP")?getenv("TEMP"):".",outname);
    #elif defined(__ANDROID__)
        snprintf(tmpfile,1024,"/data/data/com.termux/files/usr/tmp/_nc_%s.c",outname);
    #else
        snprintf(tmpfile,1024,"/data/data/com.termux/files/usr/tmp/_nc_%s.c",outname);
    #endif

    FILE *in=fopen(argv[1],"r");
    if(!in){fprintf(stderr,"Erro: nao abriu %s\n",argv[1]);return 1;}
    FILE *out_f=fopen(tmpfile,"w");
    if(!out_f){snprintf(tmpfile,1024,"_nc_%s.c",outname);out_f=fopen(tmpfile,"w");
    if(!out_f){fprintf(stderr,"Erro temporario\n");fclose(in);return 1;}}

    compile(in,out_f,is_lib);
    fclose(in);fclose(out_f);

    char cmd[2048];
    if(is_lib)
        snprintf(cmd,2048,"clang -c %s -o %s.o -lm 2>&1",tmpfile,outname);
    else
        #ifdef _WIN32
            snprintf(cmd,2048,"clang -o %s.exe %s -lm 2>&1",outname,tmpfile);
        #else
            snprintf(cmd,2048,"clang -o %s %s -lm 2>&1",outname,tmpfile);
        #endif

    int ret=system(cmd);
    remove(tmpfile);

    if(ret==0){
        if(is_lib) {
            printf("✓ Biblioteca: %s.o\n",outname);
        } else {
            // Executa automaticamente igual Python/Java
            char run_cmd[2048];
            #ifdef _WIN32
                snprintf(run_cmd, 2048, "%s.exe", outname);
            #else
                snprintf(run_cmd, 2048, "./%s", outname);
            #endif
            int run_ret = system(run_cmd);
            remove(outname); // apaga o binario apos rodar
            return run_ret;
        }
    } else { fprintf(stderr,"✗ Erro ao compilar\n"); return 1; }
    return 0;
}
