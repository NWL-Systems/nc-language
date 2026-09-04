#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include "nc_fs.h"

// Inicializa estrutura de diretorios
void nc_init_dirs(NCUserDirs *dirs, const char *base) {
    snprintf(dirs->root,     512, "%s/NuclearCloud", base);
    snprintf(dirs->config,   512, "%s/NuclearCloud/Config", base);
    snprintf(dirs->user,     512, "%s/NuclearCloud/Config/User", base);
    snprintf(dirs->sdcard,   512, "%s/NuclearCloud/Config/User/SDcard_Cloud", base);
    snprintf(dirs->tmp,      512, "%s/NuclearCloud/Config/User/SDcard_Cloud/TMP", base);
    snprintf(dirs->apps,     512, "%s/NuclearCloud/Config/User/Apps", base);
    snprintf(dirs->docs,     512, "%s/NuclearCloud/Config/User/Docs", base);
    snprintf(dirs->imgs,     512, "%s/NuclearCloud/Config/User/Images", base);
    snprintf(dirs->devapps,  512, "%s/NuclearCloud/Config/User/DevApps", base);
    snprintf(dirs->privapps, 512, "%s/NuclearCloud/Config/User/PrivApps", base);
    snprintf(dirs->lang,     512, "%s/NuclearCloud/Config/User/Languages", base);
}

// Cria um diretorio, sem erro se ja existir
static int nc_mkdir_one(const char *path) {
    #ifdef _WIN32
        if (mkdir(path) != 0 && errno != EEXIST) return -1;
    #else
        if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
    #endif
    return 0;
}

// Cria todos os diretorios (inclusive a pasta base, se ainda nao existir)
int nc_create_dirs(NCUserDirs *dirs) {
    // Garante que a pasta base (pai de "NuclearCloud") existe antes de tudo,
    // senao os mkdir seguintes falham silenciosamente (parent inexistente).
    char base[512];
    strncpy(base, dirs->root, sizeof(base));
    base[sizeof(base)-1] = 0;
    char *suffix = strstr(base, "/NuclearCloud");
    if (suffix) *suffix = 0;
    if (strlen(base) > 0 && nc_mkdir_one(base) != 0) {
        fprintf(stderr, "[NCFS] Erro ao criar pasta base %s: %s\n", base, strerror(errno));
        return -1;
    }

    char *paths[] = {
        dirs->root, dirs->config, dirs->user,
        dirs->sdcard, dirs->tmp, dirs->apps,
        dirs->docs, dirs->imgs, dirs->devapps,
        dirs->privapps, dirs->lang, NULL
    };
    int falhas = 0;
    for (int i = 0; paths[i]; i++) {
        if (nc_mkdir_one(paths[i]) != 0) {
            fprintf(stderr, "[NCFS] Erro ao criar %s: %s\n", paths[i], strerror(errno));
            falhas++;
        }
    }
    if (falhas > 0) {
        fprintf(stderr, "[NCFS] %d diretorio(s) nao puderam ser criados.\n", falhas);
        return -1;
    }
    printf("[NCFS] Estrutura de pastas criada em: %s\n", dirs->root);
    return 0;
}

// Cria um arquivo NC com cabecalho
int nc_create_file(const char *path, const char *magic, const char *data, unsigned int size, unsigned int flags) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    NCFileHeader h;
    memset(&h, 0, sizeof(h));
    strncpy(h.magic, magic, 4);
    strncpy(h.version, "2.0", 4);

    // Nome do arquivo (sem path)
    const char *name = strrchr(path, '/');
    strncpy(h.name, name ? name+1 : path, 255);

    strncpy(h.owner, "NuclearCloud", 63);
    h.size = size;
    h.flags = flags;

    // Data de criacao
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(h.created, 32, "%Y-%m-%d %H:%M:%S", tm);

    fwrite(&h, sizeof(h), 1, f);
    if (data && size > 0) fwrite(data, 1, size, f);
    fclose(f);
    return 0;
}

// Le um arquivo NC
int nc_read_file(const char *path, NCFileHeader *header, char *data, unsigned int maxsize) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(header, sizeof(NCFileHeader), 1, f) != 1) { fclose(f); return -1; }
    if (data && maxsize > 0) {
        unsigned int toread = header->size < maxsize ? header->size : maxsize;
        if (fread(data, 1, toread, f) != toread) { fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}

// Retorna o tipo baseado na extensao
const char* nc_get_type(const char *filename) {
    if (strstr(filename, ".ncfile"))   return "Arquivo NC";
    if (strstr(filename, ".ncimg"))    return "Imagem NC";
    if (strstr(filename, ".ncvideo"))  return "Video/GIF NC";
    if (strstr(filename, ".ncdocs"))   return "Documento NC";
    if (strstr(filename, ".ncapp"))    return "Aplicativo NC";
    if (strstr(filename, ".ncprivapp"))return "App Especial NC";
    if (strstr(filename, ".ncdevapp")) return "App Dev NC";
    if (strstr(filename, ".nc"))       return "Codigo NC Language";
    return "Arquivo desconhecido";
}

// Verifica se e um arquivo NC valido
int nc_is_valid(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char magic[4] = {0};
    size_t n = fread(magic, 1, 4, f);
    fclose(f);
    if (n != 4) return 0;
    return (strncmp(magic, "NCL", 3) == 0 || strncmp(magic, "NCA", 3) == 0);
}
