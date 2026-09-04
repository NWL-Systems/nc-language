#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nc_fs.h"

int main(int argc, char **argv) {
    // Base: sdcard no Android/Termux, home no desktop.
    // Detecta em tempo de execucao via $PREFIX (com.termux) - __ANDROID__
    // so e definido quando compilado com o NDK, nao no build nativo do
    // Termux, entao nao da pra confiar nele aqui.
    const char *pfx = getenv("PREFIX");
    const char *base = (pfx && strstr(pfx, "com.termux")) ? "/sdcard"
                      : (getenv("HOME") ? getenv("HOME") : ".");

    if (argc > 1) base = argv[1];

    printf("=== NuclearCloud OS - Setup de Pastas ===\n");
    printf("© 2026 NWL-Systems\n\n");

    NCUserDirs dirs;
    nc_init_dirs(&dirs, base);
    if (nc_create_dirs(&dirs) != 0) {
        fprintf(stderr, "\n✗ Falha ao criar a estrutura de pastas.\n");
        return 1;
    }

    printf("\nEstrutura criada:\n");
    printf("  %s\n", dirs.root);
    printf("  %s\n", dirs.config);
    printf("  %s\n", dirs.user);
    printf("  %s\n", dirs.sdcard);
    printf("  %s\n", dirs.tmp);
    printf("  %s\n", dirs.apps);
    printf("  %s\n", dirs.docs);
    printf("  %s\n", dirs.imgs);
    printf("  %s\n", dirs.devapps);
    printf("  %s\n", dirs.privapps);
    printf("  %s\n", dirs.lang);

    // Cria arquivo de boas vindas
    char welcome[512];
    snprintf(welcome, 512, "%s/NuclearCloud/Config/User/welcome.ncdocs", base);
    const char *msg = "Bem vindo ao NuclearCloud OS!\n© 2026 NWL-Systems\n";
    nc_create_file(welcome, NC_MAGIC_DOCS, msg, strlen(msg), NC_FLAG_READ);
    printf("\nArquivo de boas vindas criado: welcome.ncdocs\n");

    printf("\n✓ NuclearCloud OS pronto!\n");
    return 0;
}
