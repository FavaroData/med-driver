#pragma once
#include <windows.h>

#define PRINTER_NAME_MAX     256
#define PRINTER_PORT_MAX     128
#define PRINTER_PATH_MAX     512
#define PRINTER_BASENAME_MAX 256

typedef struct {
    wchar_t name[PRINTER_NAME_MAX];
    wchar_t portName[PRINTER_PORT_MAX];
    wchar_t outputPath[PRINTER_PATH_MAX];         // pasta de destino
    wchar_t outputBaseName[PRINTER_BASENAME_MAX]; // nome base do arquivo (sem extensão, sem número)
} PrinterEntry;

/* Carrega do arquivo JSON. Retorna número de entradas; *out deve ser liberado com store_free(). */
int  store_load(PrinterEntry **out);
/* Salva todas as entradas no arquivo JSON. Retorna 0 em sucesso, -1 em erro. */
int  store_save(const PrinterEntry *entries, int count);
void store_free(PrinterEntry *entries);
