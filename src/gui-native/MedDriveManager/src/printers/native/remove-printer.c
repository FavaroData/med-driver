// remove-printer.c — remove uma impressora do Spooler, nativo (equivalente ao remove-printer.ps1).
// Sem PowerShell/Add-Type
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>
#include <stdio.h>
#include <stdarg.h>
#include "remove-printer.h"

#define LOG_PATH L"C:\\Windows\\Temp\\meddrive_printer_manager.log"

// Loga no mesmo arquivo dos scripts .ps1, em UTF-16LE (com BOM se o arquivo for novo).
// Usa _vsnwprintf/_snwprintf (nao as versoes _s) porque o msvcrt do XP nao exporta as _s.
static void rp_log(const wchar_t *fmt, ...)
{
    wchar_t line[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(line, 512, fmt, ap);
    va_end(ap);
    line[511] = L'\0';

    HANDLE h = CreateFileW(LOG_PATH, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD created = GetLastError();   // 0 = arquivo novo; ERROR_ALREADY_EXISTS = ja existia
    if (h == INVALID_HANDLE_VALUE) return;

    DWORD w;
    if (created != ERROR_ALREADY_EXISTS) {
        WORD bom = 0xFEFF;            // BOM UTF-16LE para arquivo recem-criado
        WriteFile(h, &bom, sizeof(bom), &w, NULL);
    }

    wchar_t out[520];
    int n = _snwprintf(out, 520, L"%s\r\n", line);
    if (n < 0 || n > 519) n = 519;
    WriteFile(h, out, (DWORD)(n * sizeof(wchar_t)), &w, NULL);
    CloseHandle(h);
}

// Garante o Spooler em execucao (OpenPrinter/DeletePrinter dependem dele).
static void ensure_spooler(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    SC_HANDLE svc = OpenServiceW(scm, L"Spooler", SERVICE_QUERY_STATUS | SERVICE_START);
    if (svc) {
        SERVICE_STATUS ss;
        if (QueryServiceStatus(svc, &ss) && ss.dwCurrentState != SERVICE_RUNNING) {
            StartServiceW(svc, 0, NULL);
            for (int i = 0; i < 20; i++) {   // espera ate ~5s
                if (QueryServiceStatus(svc, &ss) && ss.dwCurrentState == SERVICE_RUNNING) break;
                Sleep(250);
            }
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

BOOL remove_printer(const wchar_t *printerName)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    rp_log(L"");
    rp_log(L"=== [%04d-%02d-%02d %02d:%02d:%02d] remove-printer (nativo) ===",
           st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    rp_log(L"[INFO] Removendo impressora '%s'...", printerName);

    ensure_spooler();

    // DeletePrinter exige um handle aberto com PRINTER_ACCESS_ADMINISTER;
    // OpenPrinter sem PRINTER_DEFAULTS da so PRINTER_ACCESS_USE (erro 5 no delete).
    PRINTER_DEFAULTS pd;
    ZeroMemory(&pd, sizeof(pd));
    pd.DesiredAccess = PRINTER_ALL_ACCESS;

    HANDLE hPrinter = NULL;
    if (!OpenPrinterW((LPWSTR)printerName, &hPrinter, &pd)) {
        // impressora nao existe: idempotente, tratamos como sucesso
        rp_log(L"[AVISO] Impressora '%s' nao encontrada (Win32 %lu)",
               printerName, GetLastError());
        return TRUE;
    }

    BOOL  ok  = DeletePrinter(hPrinter);
    DWORD err = ok ? 0 : GetLastError();
    ClosePrinter(hPrinter);

    if (ok) {
        rp_log(L"[OK] Impressora removida com sucesso!");
        rp_log(L"     Impressora : %s", printerName);
    } else {
        rp_log(L"[ERRO] DeletePrinter falhou (Win32 erro %lu)", err);
    }
    return ok;
}
