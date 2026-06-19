#pragma once
#include <windows.h>

#define PRINTER_NAME_MAX     256
#define PRINTER_PORT_MAX     128
#define PRINTER_PATH_MAX     512
#define PRINTER_BASENAME_MAX 256

typedef struct {
    wchar_t name[PRINTER_NAME_MAX];
    wchar_t portName[PRINTER_PORT_MAX];
    wchar_t outputPath[PRINTER_PATH_MAX];
    wchar_t outputBaseName[PRINTER_BASENAME_MAX];
} PrinterEntry;

typedef struct {
    wchar_t name[PRINTER_NAME_MAX];         /* nome do perfil (sem prefixo "PORT ") */
    wchar_t portName[PRINTER_PORT_MAX];     /* nome completo da porta no spooler     */
    wchar_t outputPath[PRINTER_PATH_MAX];
    wchar_t outputBaseName[PRINTER_BASENAME_MAX];
    DWORD   openAfterGenerate;
    DWORD   overwriteFile;
} ProfileEntry;

/* Impressoras — JSON em %USERPROFILE% */
int  store_load(PrinterEntry **out);
int  store_save(const PrinterEntry *entries, int count);
void store_free(PrinterEntry *entries);

/* Perfis — registry do monitor */
int  profile_load(ProfileEntry **out);
void profile_free(ProfileEntry *entries);
