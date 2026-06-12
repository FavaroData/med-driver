#ifndef PDF_MONITOR_H
#define PDF_MONITOR_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>

#define MONITOR_NAME  L"Med-driver Monitor"
#define PORT_NAME     L"Med-driver Port"

// entrega do spooler para o monitor
// o monitor desempacota e lê o que precisa (nesse caso só o hckRegistryRoot)
// No entanto, elas foram definidas manualmente para garantir problemas de compatibilidade
typedef struct _MONITORINIT {
    DWORD  cbSize;
    HANDLE hSpooler;
    HKEY   hckRegistryRoot;
    PVOID  pSpoolerFunctions;
    LPWSTR pszServerName;
    LPWSTR pszMonitorName;
} MONITORINIT, *PMONITORINIT;

// entrega do monitor para o Spooler, com as funções implementadas para lidar com os jobs de impressão
// algumas estão NULL porque não estão sendo usadas nesse monitor
// mas poderiam ser implementadas futuramente para adicionar funcionalidades como configuração da porta, etc
typedef struct _MONITOR2 {
    DWORD cbSize;
    // funções obrigatórias para o monitor de impressão, chamadas pelo Spooler
    BOOL  (WINAPI *pfnEnumPorts)             (HANDLE, LPWSTR, DWORD, LPBYTE, DWORD, LPDWORD, LPDWORD);
    BOOL  (WINAPI *pfnOpenPort)              (HANDLE, LPWSTR, PHANDLE);
    BOOL  (WINAPI *pfnOpenPortEx)            (HANDLE, LPWSTR, LPWSTR, PHANDLE, PVOID);
    BOOL  (WINAPI *pfnStartDocPort)          (HANDLE, LPWSTR, DWORD, DWORD, LPBYTE);
    BOOL  (WINAPI *pfnWritePort)             (HANDLE, LPBYTE, DWORD, LPDWORD);
    BOOL  (WINAPI *pfnReadPort)              (HANDLE, LPBYTE, DWORD, LPDWORD);
    BOOL  (WINAPI *pfnEndDocPort)            (HANDLE);
    BOOL  (WINAPI *pfnClosePort)             (HANDLE);
    // funções que estão NULL porque não estão sendo usadas nesse monitor
    // mas poderiam ser implementadas para adicionar funcionalidades como configuração da porta, etc
    BOOL  (WINAPI *pfnAddPort)               (HANDLE, LPWSTR, HWND, LPWSTR);
    BOOL  (WINAPI *pfnAddPortEx)             (HANDLE, LPWSTR, DWORD, LPBYTE, LPWSTR);
    BOOL  (WINAPI *pfnConfigurePort)         (HANDLE, LPWSTR, HWND, LPWSTR);
    BOOL  (WINAPI *pfnDeletePort)            (HANDLE, LPWSTR, HWND, LPWSTR);
    BOOL  (WINAPI *pfnGetPrinterDataFromPort)(HANDLE, DWORD, LPWSTR, LPWSTR, DWORD, LPWSTR, DWORD, LPDWORD);
    BOOL  (WINAPI *pfnSetPortTimeOuts)       (HANDLE, LPCOMMTIMEOUTS, DWORD);
    BOOL  (WINAPI *pfnXcvOpenPort)           (HANDLE, LPCWSTR, DWORD, PHANDLE);
    DWORD (WINAPI *pfnXcvDataPort)           (HANDLE, LPCWSTR, PBYTE, DWORD, PBYTE, DWORD, PDWORD);
    BOOL  (WINAPI *pfnXcvClosePort)          (HANDLE);
} MONITOR2, *LPMONITOR2;

// variável persistente durante o uso da dll para guardar os dados do registry e do arquivo temporário para cada job de impressão
typedef struct {
    WCHAR  outputPath[MAX_PATH];
    WCHAR  ghostscriptPath[MAX_PATH];
    WCHAR  tempPsFile[MAX_PATH];
    HANDLE hTempFile;
} PORT_CONTEXT;

LPMONITOR2 WINAPI InitializePrintMonitor2(PMONITORINIT pMonitorInit, PHANDLE phMonitor);

#endif
