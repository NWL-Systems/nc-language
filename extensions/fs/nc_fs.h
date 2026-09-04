#ifndef NC_FS_H
#define NC_FS_H

// NuclearCloud File System - NCFS
// © 2026 NWL-Systems

// Extensoes oficiais NC
#define NC_EXT_FILE     ".ncfile"    // arquivos/pastas/linguagens
#define NC_EXT_IMG      ".ncimg"     // imagens
#define NC_EXT_VIDEO    ".ncvideo"   // videos e gifs
#define NC_EXT_DOCS     ".ncdocs"    // documentos/textos/json
#define NC_EXT_APP      ".ncapp"     // aplicativos normais
#define NC_EXT_PRIVAPP  ".ncprivapp" // apps especiais
#define NC_EXT_DEVAPP   ".ncdevapp"  // apps dev/terminal

// Magic bytes de cada tipo
#define NC_MAGIC_FILE    "NCLF"
#define NC_MAGIC_IMG     "NCIM"
#define NC_MAGIC_VIDEO   "NCVD"
#define NC_MAGIC_DOCS    "NCDC"
#define NC_MAGIC_APP     "NCAP"
#define NC_MAGIC_PRIVAPP "NCPA"
#define NC_MAGIC_DEVAPP  "NCDA"

// Flags de permissao
#define NC_FLAG_READ     0x01
#define NC_FLAG_WRITE    0x02
#define NC_FLAG_EXEC     0x04
#define NC_FLAG_PRIVATE  0x08
#define NC_FLAG_DEV      0x10
#define NC_FLAG_SYSTEM   0x20

// Cabecalho de todo arquivo NC
typedef struct {
    char     magic[4];       // tipo do arquivo
    char     version[4];     // versao "2.0"
    char     name[256];      // nome do arquivo
    char     owner[64];      // dono
    char     created[32];    // data criacao
    unsigned int size;       // tamanho do conteudo
    unsigned int flags;      // permissoes
    char     checksum[32];   // hash de integridade
} NCFileHeader;

// Estrutura de pastas do usuario
typedef struct {
    char root[512];       // raiz: /sdcard/NuclearCloud/
    char config[512];     // Config/
    char user[512];       // Config/User/
    char sdcard[512];     // Config/User/SDcard_Cloud/
    char tmp[512];        // Config/User/SDcard_Cloud/TMP/
    char apps[512];       // Config/User/Apps/
    char docs[512];       // Config/User/Docs/
    char imgs[512];       // Config/User/Images/
    char devapps[512];    // Config/User/DevApps/
    char privapps[512];   // Config/User/PrivApps/
    char lang[512];       // Config/User/Languages/
} NCUserDirs;

// Funcoes
void nc_init_dirs(NCUserDirs *dirs, const char *base);
int  nc_create_dirs(NCUserDirs *dirs);
int  nc_create_file(const char *path, const char *type, const char *data, unsigned int size, unsigned int flags);
int  nc_read_file(const char *path, NCFileHeader *header, char *data, unsigned int maxsize);
const char* nc_get_type(const char *filename);
int  nc_is_valid(const char *path);

#endif
