#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#ifdef _WIN32
#include <process.h>
#include <windows.h>
#define getpid _getpid
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#define MAX_LINE 4096
#define MAX_TOKEN 512
#define VERSION "3.2"

// Acha o caminho absoluto do proprio executavel (nclang/nuclearcloud),
// independente de onde o usuario chamou ele. Usado pra achar o nc_setup
// ao lado, e pra se auto-chamar de forma confiavel no modo -e/terminal.
static int self_exe_path(char *buf, size_t bufsize) {
#if defined(_WIN32)
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)bufsize);
    return (n > 0 && n < bufsize) ? 0 : -1;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)bufsize;
    return _NSGetExecutablePath(buf, &size) == 0 ? 0 : -1;
#else
    ssize_t n = readlink("/proc/self/exe", buf, bufsize - 1);
    if (n <= 0) return -1;
    buf[n] = 0;
    return 0;
#endif
}

// Pasta onde o executavel atual mora (pra achar irmaos como nc_setup)
static void self_exe_dir(char *out, size_t outsize) {
    char self[2200];
    if (self_exe_path(self, sizeof(self)) != 0) {
        strncpy(out, ".", outsize); out[outsize-1] = 0; return;
    }
    char *slash = strrchr(self, '/');
    #ifdef _WIN32
    char *bslash = strrchr(self, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
    #endif
    if (slash) { *slash = 0; strncpy(out, self, outsize); out[outsize-1] = 0; }
    else { strncpy(out, ".", outsize); out[outsize-1] = 0; }
}

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

int compile(FILE *in, FILE *out, int is_lib) {
    char line[MAX_LINE], trimmed[MAX_LINE];
    int in_class = 0, main_open = 1, indent = 1;
    int line_no = 0, had_error = 0;

    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "#include <math.h>\n");
    fprintf(out, "#ifndef _WIN32\n#include <unistd.h>\n#endif\n");
    fprintf(out, "\n// NC Language v%s - NWL-Systems\n\n", VERSION);

    // Helpers para os comandos de sistema de arquivos (!mkdir! !write! !ls!)
    fprintf(out, "#if defined(_WIN32)\n");
    fprintf(out, "#include <direct.h>\n");
    fprintf(out, "static void _nc_mkdir(const char *p){ _mkdir(p); }\n");
    fprintf(out, "#else\n");
    fprintf(out, "#include <sys/stat.h>\n");
    fprintf(out, "#include <sys/types.h>\n");
    fprintf(out, "static void _nc_mkdir(const char *p){ mkdir(p, 0755); }\n");
    fprintf(out, "#endif\n");
    fprintf(out, "static void _nc_write_file(const char *path, const char *content){\n");
    fprintf(out, "    FILE *_f = fopen(path, \"w\");\n");
    fprintf(out, "    if (_f) { fprintf(_f, \"%%s\", content); fclose(_f); }\n");
    fprintf(out, "    else { fprintf(stderr, \"Erro ao escrever %%s\\n\", path); }\n");
    fprintf(out, "}\n");
    fprintf(out, "static void _nc_list_dir(const char *path){\n");
    fprintf(out, "    char _cmd[1200];\n");
    fprintf(out, "#if defined(_WIN32)\n");
    fprintf(out, "    snprintf(_cmd, sizeof(_cmd), \"dir \\\"%%s\\\"\", path);\n");
    fprintf(out, "#else\n");
    fprintf(out, "    snprintf(_cmd, sizeof(_cmd), \"ls -la \\\"%%s\\\"\", path);\n");
    fprintf(out, "#endif\n");
    fprintf(out, "    system(_cmd);\n");
    fprintf(out, "}\n\n");

    // Helper pra instalar pacote, detectando a plataforma certa:
    // Windows -> winget | macOS -> brew | Android/Termux -> pkg
    // Linux -> detecta apt-get/dnf/pacman/zypper em tempo de execucao
    fprintf(out, "static void _nc_pkg_install(const char *pkgname){\n");
    fprintf(out, "    char _cmd[700];\n");
    fprintf(out, "#if defined(_WIN32)\n");
    fprintf(out, "    snprintf(_cmd, sizeof(_cmd), \"winget install --accept-source-agreements --accept-package-agreements -e --id %%s\", pkgname);\n");
    fprintf(out, "    system(_cmd);\n");
    fprintf(out, "#elif defined(__APPLE__)\n");
    fprintf(out, "    snprintf(_cmd, sizeof(_cmd), \"brew install %%s\", pkgname);\n");
    fprintf(out, "    system(_cmd);\n");
    fprintf(out, "#else\n");
    fprintf(out, "    const char *pfx = getenv(\"PREFIX\");\n");
    fprintf(out, "    const char *sudo_pfx = (geteuid()==0) ? \"\" : \"sudo \";\n");
    fprintf(out, "    if (pfx && strstr(pfx, \"com.termux\")) {\n");
    fprintf(out, "        snprintf(_cmd, sizeof(_cmd), \"pkg install -y %%s\", pkgname);\n");
    fprintf(out, "    } else if (system(\"command -v apt-get >/dev/null 2>&1\") == 0) {\n");
    fprintf(out, "        snprintf(_cmd, sizeof(_cmd), \"%%sapt-get install -y %%s\", sudo_pfx, pkgname);\n");
    fprintf(out, "    } else if (system(\"command -v dnf >/dev/null 2>&1\") == 0) {\n");
    fprintf(out, "        snprintf(_cmd, sizeof(_cmd), \"%%sdnf install -y %%s\", sudo_pfx, pkgname);\n");
    fprintf(out, "    } else if (system(\"command -v pacman >/dev/null 2>&1\") == 0) {\n");
    fprintf(out, "        snprintf(_cmd, sizeof(_cmd), \"%%spacman -S --noconfirm %%s\", sudo_pfx, pkgname);\n");
    fprintf(out, "    } else if (system(\"command -v zypper >/dev/null 2>&1\") == 0) {\n");
    fprintf(out, "        snprintf(_cmd, sizeof(_cmd), \"%%szypper install -y %%s\", sudo_pfx, pkgname);\n");
    fprintf(out, "    } else {\n");
    fprintf(out, "        fprintf(stderr, \"Nenhum gerenciador de pacotes conhecido encontrado (pkg/apt/dnf/pacman/zypper).\\n\");\n");
    fprintf(out, "        return;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    system(_cmd);\n");
    fprintf(out, "#endif\n");
    fprintf(out, "}\n\n");

    if (is_lib) {
        fprintf(out, "// NCLib\n");
        main_open = 0;
    } else {
        fprintf(out, "int main() {\n");
        fprintf(out, "    char _nc_buf[4096];\n\n");
    }

    while (fgets(line, MAX_LINE, in)) {
        line_no++;
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
            char close_pad[256] = "    ";
            for (int i = 1; i < indent; i++) strcat(close_pad, "    ");
            fprintf(out, "%s}\n", close_pad);
            if(in_class) in_class=0;
            continue;
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
                if(strcmp(vtype,"!fra!")==0) {
                    // Le a linha inteira (aceita espacos) e remove o \n final
                    fprintf(out,"%sif (fgets(%s, sizeof(%s), stdin)) { %s[strcspn(%s, \"\\n\")] = 0; }\n",
                        pad,varname,varname,varname,varname);
                } else {
                    fprintf(out,"%sscanf(\"%s\",&%s);\n",pad,
                        strcmp(vtype,"!numD!")==0?"%lf":strcmp(vtype,"!numF!")==0?"%f":"%d",varname);
                    fprintf(out,"%swhile(getchar()!='\\n');\n",pad); // limpa o buffer pro proximo !ask! de texto
                }
            } continue;
        }

        // !jun!
        if (starts_with(trimmed,"!jun!")) {
            char *t2=trim(trimmed+5); char fname[MAX_TOKEN]; strcpy(fname,t2);
            char *dot=strstr(fname,".nc"); if(dot)*dot=0;
            fprintf(out,"%s%s();\n",pad,fname); continue;
        }

        // Controle de fluxo
        // !if! / !elif! / !else! seguem o padrao documentado no README: cada
        // bloco fecha com "]" antes do proximo !elif!/!else!. Por isso
        // !elif!/!else! NAO fecham chave por conta propria (isso ja foi
        // feito pelo "]" anterior) - so abrem o proximo bloco.
        if (starts_with(trimmed,"!if!"))   { char _fc[4096]; nc_fix_cond(trim(trimmed+4),_fc); fprintf(out,"%sif(%s){\n",pad,_fc); indent++; continue; }
        if (starts_with(trimmed,"!elif!")) { char _fc[4096]; nc_fix_cond(trim(trimmed+6),_fc); fprintf(out,"%selse if(%s){\n",pad,_fc); indent++; continue; }
        if (starts_with(trimmed,"!else!")) {
            char *body=trim(trimmed+6);
            if(strlen(body)>0) fprintf(out,"%selse{\n%s    printf(\"%%s\\n\",\"%s\");\n%s}\n",pad,pad,body,pad);
            else { fprintf(out,"%selse{\n",pad); indent++; }
            continue;
        }
        if (starts_with(trimmed,"!loop!"))  { char *r=trim(trimmed+6);char *b=strstr(r,"[");if(b)*b=0; fprintf(out,"%sfor(int _i=0;_i<%s;_i++){\n",pad,trim(r));indent++;continue; }
        if (starts_with(trimmed,"!while!")) { char *r=trim(trimmed+7);char *b=strstr(r,"[");if(b)*b=0; fprintf(out,"%swhile(%s){\n",pad,trim(r));indent++;continue; }
        if (starts_with(trimmed,"!ret!"))   { fprintf(out,"%sreturn %s;\n",pad,trim(trimmed+5)); continue; }
        if (starts_with(trimmed,"!stop!"))  { fprintf(out,"%sbreak;\n",pad); continue; }
        if (starts_with(trimmed,"!skip!"))  { fprintf(out,"%scontinue;\n",pad); continue; }

        // !mkdir! nome_pasta
        if (starts_with(trimmed,"!mkdir!")) {
            char *arg = trim(trimmed+7);
            if (strlen(arg) == 0) {
                fprintf(stderr,"Erro de sintaxe NC na linha %d: '!mkdir!' precisa de um caminho. Uso: !mkdir! nome_pasta\n",line_no);
                had_error = 1; continue;
            }
            fprintf(out,"%s_nc_mkdir(%s);\n",pad,arg); continue;
        }

        // !ls! path
        if (starts_with(trimmed,"!ls!")) {
            char *arg = trim(trimmed+4);
            if (strlen(arg) == 0) {
                fprintf(stderr,"Erro de sintaxe NC na linha %d: '!ls!' precisa de um caminho. Uso: !ls! caminho\n",line_no);
                had_error = 1; continue;
            }
            fprintf(out,"%s_nc_list_dir(%s);\n",pad,arg); continue;
        }

        // !pkg! nome_pacote - instala pacote detectando a plataforma
        if (starts_with(trimmed,"!pkg!")) {
            char *arg = trim(trimmed+5);
            if (strlen(arg) == 0) {
                fprintf(stderr,"Erro de sintaxe NC na linha %d: '!pkg!' precisa de um nome de pacote. Uso: !pkg! nome_pacote\n",line_no);
                had_error = 1; continue;
            }
            fprintf(out,"%s_nc_pkg_install(%s);\n",pad,arg); continue;
        }

        // !write! arquivo, conteudo
        if (starts_with(trimmed,"!write!")) {
            char *rest = trim(trimmed+7);
            char *comma = strchr(rest,',');
            if (!comma) {
                fprintf(stderr,"Erro de sintaxe NC na linha %d: '!write!' precisa de \"arquivo, conteudo\". Uso: !write! arquivo, conteudo\n",line_no);
                had_error = 1; continue;
            }
            char fname[MAX_TOKEN], content[MAX_TOKEN];
            strncpy(fname, rest, comma-rest); fname[comma-rest]=0; strcpy(fname,trim(fname));
            strcpy(content, trim(comma+1));
            if (strlen(fname) == 0 || strlen(content) == 0) {
                fprintf(stderr,"Erro de sintaxe NC na linha %d: '!write!' precisa de nome de arquivo E conteudo, ambos preenchidos.\n",line_no);
                had_error = 1; continue;
            }
            fprintf(out,"%s_nc_write_file(%s,%s);\n",pad,fname,content);
            continue;
        }

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
    return !had_error; // 1 = ok, 0 = erro de sintaxe NC encontrado
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("nclang - NC Language v%s - NWL-Systems\n", VERSION);
        printf("Uso: nclang arquivo.nc [saida]\n");
        printf("     nclang arquivo.ncli [saida]\n");
        printf("     nclang -e \"codigo NC\"      (executa uma linha direto, sem gerar app)\n");
        printf("     nclang --open terminal    (abre o shell interativo)\n");
        return 1;
    }

    // Modo --open terminal: abre o shell interativo (mesma coisa que o
    // nc-cmd, so que embutido em nclang/nuclearcloud - nao precisa de
    // binario separado). Se o NuclearCloud OS ainda nao foi inicializado
    // nessa maquina, roda o nc_setup sozinho antes de abrir.
    if (argc >= 3 && strcmp(argv[1], "--open") == 0 && strcmp(argv[2], "terminal") == 0) {
        char self_dir[2200];
        self_exe_dir(self_dir, sizeof(self_dir));

        // Base do NuclearCloud OS: mesma logica do nc_setup (Termux -> /sdcard, senao $HOME)
        const char *pfx = getenv("PREFIX");
        const char *base = (pfx && strstr(pfx, "com.termux")) ? "/sdcard"
                          : (getenv("HOME") ? getenv("HOME") : ".");

        char nc_check[2200];
        snprintf(nc_check, sizeof(nc_check), "%s/NuclearCloud", base);

        struct stat st;
        int exists = (stat(nc_check, &st) == 0
            #ifndef _WIN32
                && S_ISDIR(st.st_mode)
            #endif
        );

        if (!exists) {
            printf("[NuclearCloud OS ainda nao foi inicializado nessa maquina - rodando o setup...]\n");
            fflush(stdout);
            char setup_path[2400];
            #ifdef _WIN32
                snprintf(setup_path, sizeof(setup_path), "%s\\nc_setup.exe", self_dir);
            #else
                snprintf(setup_path, sizeof(setup_path), "%s/nc_setup", self_dir);
            #endif
            if (access(setup_path, 0) == 0) {
                char setup_cmd[2500];
                snprintf(setup_cmd, sizeof(setup_cmd), "\"%s\"", setup_path);
                int sret = system(setup_cmd);
                (void)sret;
            } else {
                fprintf(stderr, "Aviso: nc_setup nao encontrado ao lado de %s - pulando a inicializacao automatica.\n",
                        strcmp(argv[0], "nuclearcloud") == 0 ? "nuclearcloud" : "nclang");
            }
            printf("\n");
        }

        // REPL: mesma logica do nc-cmd (sessao acumulada, reexecutada a
        // cada linha, pra manter variaveis vivas entre comandos)
        char self_bin[2400];
        #ifdef _WIN32
            snprintf(self_bin, sizeof(self_bin), "%s\\nclang.exe", self_dir);
        #else
            snprintf(self_bin, sizeof(self_bin), "%s/nclang", self_dir);
        #endif
        if (access(self_bin, X_OK) != 0) {
            // fallback: usa o proprio caminho pelo qual fomos chamados
            strncpy(self_bin, argv[0], sizeof(self_bin)-1);
            self_bin[sizeof(self_bin)-1] = 0;
        }

        const char *tmpdir_t = getenv("PREFIX") ? "/data/data/com.termux/files/usr/tmp"
                              : getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
        char tmp_nc[1024], tmp_out[1024];
        snprintf(tmp_nc, sizeof(tmp_nc), "%s/_nc_cmd_session_%d.nc", tmpdir_t, (int)getpid());
        snprintf(tmp_out, sizeof(tmp_out), "%s/_nc_cmd_session_%d", tmpdir_t, (int)getpid());

        static char sessao[65536] = "";
        char entrada[1024];

        printf("==========================================\n");
        printf("         NC Interative Shell (CMD)        \n");
        printf("   Digite 'exit' para fechar o terminal   \n");
        printf("   Digite 'reset' para limpar as variaveis \n");
        printf("==========================================\n\n");
        printf("Obs: cada linha reexecuta a sessao inteira do zero (variaveis\n");
        printf("sao mantidas, mas as saidas de linhas anteriores reaparecem).\n\n");

        while (1) {
            printf("nc> ");
            fflush(stdout);
            if (!fgets(entrada, sizeof(entrada), stdin)) break;
            entrada[strcspn(entrada, "\r\n")] = 0;

            if (strcmp(entrada, "exit") == 0 || strcmp(entrada, "quit") == 0) {
                printf("Saindo do NC Shell...\n");
                break;
            }
            if (strcmp(entrada, "cls") == 0 || strcmp(entrada, "clear") == 0) {
                int r = system(
                    #ifdef _WIN32
                        "cls"
                    #else
                        "clear"
                    #endif
                );
                (void)r;
                continue;
            }
            if (strcmp(entrada, "reset") == 0) {
                sessao[0] = 0;
                printf("Sessao limpa.\n");
                continue;
            }
            if (strlen(entrada) == 0) continue;

            strncat(sessao, entrada, sizeof(sessao) - strlen(sessao) - 2);
            strncat(sessao, "\n", sizeof(sessao) - strlen(sessao) - 1);

            FILE *sf = fopen(tmp_nc, "w");
            if (!sf) { fprintf(stderr, "Erro: nao consegui escrever %s\n", tmp_nc); continue; }
            fputs(sessao, sf);
            fclose(sf);

            char run_cmd[2600];
            snprintf(run_cmd, sizeof(run_cmd), "\"%s\" \"%s\" \"%s\"", self_bin, tmp_nc, tmp_out);
            int rret = system(run_cmd);
            (void)rret;
        }

        remove(tmp_nc);
        remove(tmp_out);
        return 0;
    }

    // Modo -e: executa codigo NC inline, sem precisar de um arquivo .nc
    // Usado pelo nc-cmd (shell interativo). Varios comandos podem ser
    // separados por ";;" na mesma linha, viram linhas separadas no .nc.
    if (strcmp(argv[1], "-e") == 0) {
        if (argc < 3) { fprintf(stderr, "Uso: nclang -e \"codigo NC\"\n"); return 1; }

        const char *tmpdir_e = getenv("PREFIX") ? "/data/data/com.termux/files/usr/tmp"
                              : getenv("TMPDIR") ? getenv("TMPDIR")
                              : "/tmp";
        char tmp_nc[1024], tmp_bin[1024];
        snprintf(tmp_nc, sizeof(tmp_nc), "%s/_nc_eval_%d.nc", tmpdir_e, (int)getpid());
        snprintf(tmp_bin, sizeof(tmp_bin), "%s/_nc_eval_%d", tmpdir_e, (int)getpid());

        FILE *ef = fopen(tmp_nc, "w");
        if (!ef) {
            // fallback: escreve no diretorio atual se o tmp nao for gravavel
            snprintf(tmp_nc, sizeof(tmp_nc), "_nc_eval_%d.nc", (int)getpid());
            snprintf(tmp_bin, sizeof(tmp_bin), "_nc_eval_%d", (int)getpid());
            ef = fopen(tmp_nc, "w");
            if (!ef) { fprintf(stderr, "Erro: nao consegui criar arquivo temporario\n"); return 1; }
        }
        // ";;" vira quebra de linha, permitindo varios comandos NC na mesma entrada
        for (const char *p = argv[2]; *p; p++) {
            if (p[0] == ';' && p[1] == ';') { fputc('\n', ef); p++; }
            else fputc(*p, ef);
        }
        fputc('\n', ef);
        fclose(ef);

        char self_cmd[2200];
        snprintf(self_cmd, sizeof(self_cmd), "\"%s\" \"%s\" \"%s\"", argv[0], tmp_nc, tmp_bin);
        int status = system(self_cmd);
        remove(tmp_nc);
        remove(tmp_bin);
        if (status == -1) return 1;
        #ifdef _WIN32
            return status; // no Windows, system() ja retorna o exit code direto
        #else
            return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        #endif
    }

    // Modo --install-pkg: instala um pacote .ncpkg (zip com .h/.ncli/manifesto)
    if (strcmp(argv[1], "--install-pkg") == 0) {
        if (argc < 3) { fprintf(stderr, "Uso: nclang --install-pkg pacote.ncpkg [destino]\n"); return 1; }
        const char *pkg = argv[2];
        const char *dest = argc >= 4 ? argv[3] : "./nc_libs";

        char mkdir_cmd[1300];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", dest);
        if (system(mkdir_cmd) != 0) { fprintf(stderr, "Erro ao criar %s\n", dest); return 1; }

        char unzip_cmd[2200];
        snprintf(unzip_cmd, sizeof(unzip_cmd), "unzip -oq \"%s\" -d \"%s\" 2>&1", pkg, dest);
        if (system(unzip_cmd) != 0) {
            fprintf(stderr, "✗ Erro ao extrair %s (precisa do comando 'unzip' instalado)\n", pkg);
            return 1;
        }

        char manifest[1300];
        snprintf(manifest, sizeof(manifest), "%s/package.ncpkg.json", dest);
        FILE *mf = fopen(manifest, "r");
        if (mf) {
            printf("✓ Pacote instalado em %s/\n\nManifesto (package.ncpkg.json):\n", dest);
            char mline[512];
            while (fgets(mline, sizeof(mline), mf)) printf("%s", mline);
            fclose(mf);
            printf("\nAdicione %s ao include path do seu compilador C (-I%s) e use\n", dest, dest);
            printf("!use! <nome_da_lib> no seu codigo .nc pra importar o header.\n");
        } else {
            printf("✓ Pacote extraido em %s/ (sem package.ncpkg.json - conteudo bruto)\n", dest);
        }
        return 0;
    }

    // Se for arquivo NC ja compilado — executa direto
    const char *in_ext = strrchr(argv[1], '.');
    if (in_ext && (
        !strcmp(in_ext, ".ncapp") ||
        !strcmp(in_ext, ".ncdevapp") ||
        !strcmp(in_ext, ".ncprivapp")
    )) {
        // Executaveis NC - roda direto
        char run_cmd[2048];
        snprintf(run_cmd, 2048, "chmod +x ./%s && ./%s", argv[1], argv[1]);
        return system(run_cmd);
    }
    if (in_ext && (
        !strcmp(in_ext, ".ncimg") ||
        !strcmp(in_ext, ".ncdocs") ||
        !strcmp(in_ext, ".ncgif") ||
        !strcmp(in_ext, ".ncvideo") ||
        !strcmp(in_ext, ".ncfile")
    )) {
        // Arquivos de dados - mostra info
        printf("Arquivo NC: %s\n", argv[1]);
        printf("Tipo: %s\n", in_ext);
        printf("Use o visualizador do NuclearCloud OS para abrir.\n");
        return 0;
    }

    // Detecta tipo de entrada e empacota se nao for .nc
    const char *input = argv[1];
    const char *ext = strrchr(input, '.');
    if (ext && strcmp(ext, ".nc") != 0 && strcmp(ext, ".ncli") != 0) {
        // Nao e NC Language — empacota como arquivo NC
        char output[MAX_TOKEN];
        if (argc >= 3) strcpy(output, argv[2]);
        else {
            strcpy(output, input);
            // Detecta extensao correta pelo tipo
            if (!strcmp(ext,".jpg")||!strcmp(ext,".png")||!strcmp(ext,".bmp"))
                strcat(output, ".ncimg");
            else if (!strcmp(ext,".mp4")||!strcmp(ext,".mkv")||!strcmp(ext,".avi"))
                strcat(output, ".ncvideo");
            else if (!strcmp(ext,".gif"))
                strcat(output, ".ncgif");
            else if (!strcmp(ext,".pdf")||!strcmp(ext,".txt")||!strcmp(ext,".json")||!strcmp(ext,".md"))
                strcat(output, ".ncdocs");
            else if (!strcmp(ext,".apk")||!strcmp(ext,".exe")||!strcmp(ext,".sh"))
                strcat(output, ".ncapp");
            else
                strcat(output, ".ncfile");
        }
        // Copia o arquivo
        FILE *fin = fopen(input, "rb");
        FILE *fout = fopen(output, "wb");
        if (!fin || !fout) {
            fprintf(stderr, "Erro ao empacotar %s\n", input);
            return 1;
        }
        char buf[4096]; int n;
        while ((n = fread(buf, 1, 4096, fin)) > 0)
            fwrite(buf, 1, n, fout);
        fclose(fin); fclose(fout);
        // Da permissao de execucao
        char chmod_cmd[MAX_TOKEN + 32];
        snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", output);
        if (system(chmod_cmd) != 0)
            fprintf(stderr, "Aviso: nao foi possivel marcar %s como executavel\n", output);
        printf("✓ %s -> %s\n", input, output);
        return 0;
    }

    char outname[MAX_TOKEN];
    if (argc >= 3) {
        strcpy(outname, argv[2]); // usa nome exato do usuario (.ncapp, .ncdevapp etc)
    } else {
        strcpy(outname, argv[1]);
        // Remove so .nc mas preserva .ncapp .ncli etc
        char *dot = strstr(outname, ".nc");
        if (dot && strcmp(dot, ".nc") == 0) *dot = 0;
    }

    int is_lib = strstr(argv[1], ".ncli") != NULL;

    char tmpfile[1024];
    // Se o nome de saida ja contem um caminho (ex: build/app, /tmp/app),
    // o arquivo .c intermediario fica ao lado dele - concatenar com tmpdir
    // geraria um path invalido tipo "/tmp/_nc_/tmp/app.c".
    if (strchr(outname, '/') || strchr(outname, '\\')) {
        snprintf(tmpfile, sizeof(tmpfile), "%s.tmp.c", outname);
    } else {
        #ifdef _WIN32
            snprintf(tmpfile,1024,"%s\\_nc_%s.c",getenv("TEMP")?getenv("TEMP"):".",outname);
        #else
            // Termux (com.termux) primeiro, senao TMPDIR, senao /tmp, senao diretorio atual
            const char *tmpdir = getenv("PREFIX") ? "/data/data/com.termux/files/usr/tmp"
                                : getenv("TMPDIR") ? getenv("TMPDIR")
                                : "/tmp";
            snprintf(tmpfile,1024,"%s/_nc_%s.c",tmpdir,outname);
        #endif
    }

    FILE *in=fopen(argv[1],"r");
    if(!in){fprintf(stderr,"Erro: nao abriu %s\n",argv[1]);return 1;}
    FILE *out_f=fopen(tmpfile,"w");
    if(!out_f){
        // fallback: mesmo esquema usado acima, mas peganado so o nome-base
        // (sem diretorios) pra nao gerar path invalido de novo
        const char *base = strrchr(outname,'/'); base = base? base+1 : outname;
        #ifdef _WIN32
        const char *base2 = strrchr(base,'\\'); base = base2? base2+1 : base;
        #endif
        snprintf(tmpfile,1024,"_nc_%s.c",base);
        out_f=fopen(tmpfile,"w");
        if(!out_f){fprintf(stderr,"Erro temporario\n");fclose(in);return 1;}
    }

    int nc_ok = compile(in,out_f,is_lib);
    fclose(in);fclose(out_f);

    if (!nc_ok) {
        fprintf(stderr,"✗ Erro ao compilar %s (erro de sintaxe NC - veja acima)\n",argv[1]);
        remove(tmpfile);
        return 1;
    }

    // Compilador C: usa $CC se definido, senao "cc" (aponta pra clang no
    // Termux/Mac e gcc na maioria das distros Linux via update-alternatives)
    const char *cc = getenv("CC") ? getenv("CC") : "cc";
    char cmd[2048];
    if(is_lib)
        snprintf(cmd,2048,"%s -c %s -o %s.o -lm 2>&1",cc,tmpfile,outname);
    else
        #ifdef _WIN32
            snprintf(cmd,2048,"%s -o %s.exe %s -lm 2>&1",cc,outname,tmpfile);
        #else
            snprintf(cmd,2048,"%s -o %s %s -lm 2>&1",cc,outname,tmpfile);
        #endif

    int ret=system(cmd);
    remove(tmpfile);

    if(ret==0){
        if(is_lib) {
            printf("✓ Biblioteca: %s.o\n",outname);
        } else {
            // Executa automaticamente igual Python/Java
            char run_cmd[2048];
            int has_path = strchr(outname,'/') != NULL;
            #ifdef _WIN32
                has_path = has_path || strchr(outname,'\\') != NULL;
                snprintf(run_cmd, 2048, "%s.exe", outname);
            #else
                snprintf(run_cmd, 2048, has_path ? "%s" : "./%s", outname);
            #endif
            int run_ret = system(run_cmd);
            // So apaga se nao tiver extensao nc especial no outname
            if (!strstr(outname, ".ncapp") && !strstr(outname, ".ncdevapp") &&
                !strstr(outname, ".ncprivapp") && !strstr(outname, ".ncfile")) {
                remove(outname);
            }
            if (run_ret == -1) return 1;
            #ifdef _WIN32
                return run_ret;
            #else
                return WIFEXITED(run_ret) ? WEXITSTATUS(run_ret) : 1;
            #endif
        }
    } else { fprintf(stderr,"✗ Erro ao compilar\n"); return 1; }
    return 0;
}
