#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_CMD 1024
#define MAX_SESSION 65536

void limpar_tela() {
#ifdef _WIN32
    int _r1 = system("cls"); (void)_r1;
#else
    int _r2 = system("clear"); (void)_r2;
#endif
}

// Acha o caminho absoluto do proprio executavel do nc-cmd, independente
// de onde o usuario chamou ele (cwd, PATH, instalado em outro lugar).
static int self_exe_path(char *buf, size_t bufsize) {
#if defined(_WIN32)
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)bufsize);
    return (n > 0 && n < bufsize) ? 0 : -1;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)bufsize;
    return _NSGetExecutablePath(buf, &size) == 0 ? 0 : -1;
#else
    // Linux e Android/Termux
    ssize_t n = readlink("/proc/self/exe", buf, bufsize - 1);
    if (n <= 0) return -1;
    buf[n] = 0;
    return 0;
#endif
}

// Descobre o comando certo pra chamar o nclang, nessa ordem:
// 1) na mesma pasta do EXECUTAVEL do nc-cmd (funciona mesmo chamado de
//    outra pasta, ou instalado em /usr/local/bin, etc)
// 2) ./nclang na pasta atual
// 3) nclang no PATH
// Se nao achar em lugar nenhum, avisa bem claro e sai - nada de erro
// escondido no meio do system().
static char g_nclang_path[2200];
const char* nclang_bin() {
    char self[2200];
    if (self_exe_path(self, sizeof(self)) == 0) {
        char *slash = strrchr(self, '/');
        #ifdef _WIN32
        char *bslash = strrchr(self, '\\');
        if (!slash || (bslash && bslash > slash)) slash = bslash;
        #endif
        if (slash) {
            *slash = 0;
            #ifdef _WIN32
                snprintf(g_nclang_path, sizeof(g_nclang_path), "%s\\nclang.exe", self);
            #else
                snprintf(g_nclang_path, sizeof(g_nclang_path), "%s/nclang", self);
            #endif
            if (access(g_nclang_path, X_OK) == 0) return g_nclang_path;
        }
    }
#ifdef _WIN32
    if (access("./nclang.exe", 0) == 0) return "./nclang.exe";
    if (access("nclang.exe", 0) == 0) return "nclang.exe";
    fprintf(stderr, "✗ nclang.exe nao encontrado (nem ao lado do nc-cmd, nem na pasta atual, nem no PATH).\n");
    fprintf(stderr, "  Coloque nclang.exe na mesma pasta do nc-cmd.exe.\n");
    exit(1);
#else
    if (access("./nclang", X_OK) == 0) return "./nclang";
    if (access("nclang", X_OK) == 0) return "nclang"; // ./nclang sem o "./" (raro)
    if (system("command -v nclang >/dev/null 2>&1") == 0) return "nclang"; // achou no PATH
    fprintf(stderr, "✗ nclang nao encontrado (nem ao lado do nc-cmd, nem na pasta atual, nem no PATH).\n");
    fprintf(stderr, "  Coloque o binario nclang na mesma pasta do nc-cmd, ou instale ele no PATH.\n");
    exit(1);
#endif
}

int main() {
    char entrada[MAX_CMD];
    static char sessao[MAX_SESSION] = "";   // historico de comandos da sessao (uma linha NC por linha)
    char comando_final[2200];

    printf("==========================================\n");
    printf("         NC Interative Shell (CMD)        \n");
    printf("   Digite 'exit' para fechar o terminal   \n");
    printf("   Digite 'reset' para limpar as variaveis \n");
    printf("==========================================\n\n");
    printf("Obs: cada linha reexecuta a sessao inteira do zero (variaveis\n");
    printf("sao mantidas, mas as saidas de linhas anteriores reaparecem).\n\n");

    const char *bin = nclang_bin();

    // Arquivos temporarios da sessao. Passar o codigo por arquivo (em vez de
    // por linha de comando) evita qualquer problema de aspas do shell -
    // codigo NC quase sempre tem aspas duplas (ex: say = "texto").
    char tmp_nc[300], tmp_out[300];
    #ifdef _WIN32
        const char *tdir = getenv("TEMP") ? getenv("TEMP") : ".";
    #else
        const char *tdir = getenv("PREFIX") ? "/data/data/com.termux/files/usr/tmp"
                          : getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
    #endif
    snprintf(tmp_nc, sizeof(tmp_nc), "%s/_nc_cmd_session_%d.nc", tdir, (int)getpid());
    snprintf(tmp_out, sizeof(tmp_out), "%s/_nc_cmd_session_%d", tdir, (int)getpid());

    while (1) {
        printf("nc> ");
        if (!fgets(entrada, sizeof(entrada), stdin)) break;

        // Remove a quebra de linha (\n)
        entrada[strcspn(entrada, "\r\n")] = 0;

        if (strcmp(entrada, "exit") == 0 || strcmp(entrada, "quit") == 0) {
            printf("Saindo do NC Shell...\n");
            break;
        }

        if (strcmp(entrada, "cls") == 0 || strcmp(entrada, "clear") == 0) {
            limpar_tela();
            continue;
        }

        if (strcmp(entrada, "reset") == 0) {
            sessao[0] = 0;
            printf("Sessao limpa.\n");
            continue;
        }

        if (strlen(entrada) == 0) continue;

        // Adiciona a linha ao historico da sessao
        strncat(sessao, entrada, sizeof(sessao) - strlen(sessao) - 2);
        strncat(sessao, "\n", sizeof(sessao) - strlen(sessao) - 1);

        // Escreve a sessao inteira num .nc temporario e compila/roda esse
        // arquivo diretamente - sem passar codigo por linha de comando.
        FILE *sf = fopen(tmp_nc, "w");
        if (!sf) { fprintf(stderr, "Erro: nao consegui escrever %s\n", tmp_nc); continue; }
        fputs(sessao, sf);
        fclose(sf);

        snprintf(comando_final, sizeof(comando_final), "%s \"%s\" \"%s\"", bin, tmp_nc, tmp_out);
        int _r3 = system(comando_final); (void)_r3;
    }

    remove(tmp_nc);
    remove(tmp_out);
    return 0;
}