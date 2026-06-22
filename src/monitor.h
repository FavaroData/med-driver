#ifndef PDF_MONITOR_H
#define PDF_MONITOR_H

#define WIN32_LEAN_AND_MEAN
#define NTDDI_VERSION 0x0A000000
#include <windows.h>
// winspool.h serve para as estruturas e definições de funções do Spooler
#include <winspool.h>
// e winsplp.h para as definições de funções do monitor de impressão
#include <winsplp.h>

#define MONITOR_NAME  L"Meddrive Printer MONITOR"

// variável persistente durante o uso da dll para guardar os dados do registry e do arquivo temporário para cada job de impressão
typedef struct {
    WCHAR  outputPath[MAX_PATH];       // pasta de destino (registry: OutputPath)
    WCHAR  outputBaseName[256];        // template do nome do arquivo (registry: OutputBaseName)
    WCHAR  ghostscriptPath[MAX_PATH];
    WCHAR  docName[512];               // nome do job de impressão (capturado em StartDocPort)
    WCHAR  tempPsFile[MAX_PATH];
    WCHAR  resolvedPath[MAX_PATH];     // caminho final do PDF (preenchido em resolve_template)
    HANDLE hTempFile;
    DWORD  openAfterGenerate;          // registry: OpenAfterGenerate
    DWORD  overwriteFile;              // registry: OverwriteFile — usa counter=1 sem escanear pasta
} PORT_CONTEXT;

// InitializePrintMonitor2 declarada pelo winsplp.h

#endif
