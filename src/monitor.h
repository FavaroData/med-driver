#ifndef PDF_MONITOR_H
#define PDF_MONITOR_H

#define WIN32_LEAN_AND_MEAN
#define NTDDI_VERSION 0x0A000000
#include <windows.h>
#include <winspool.h>
#include <winsplp.h>

#define MONITOR_NAME  L"Med-driver Monitor"
#define PORT_NAME     L"Med-driver Port"

// variável persistente durante o uso da dll para guardar os dados do registry e do arquivo temporário para cada job de impressão
typedef struct {
    WCHAR  outputPath[MAX_PATH];
    WCHAR  ghostscriptPath[MAX_PATH];
    WCHAR  tempPsFile[MAX_PATH];
    HANDLE hTempFile;
} PORT_CONTEXT;

// InitializePrintMonitor2 declarada pelo winsplp.h

#endif
