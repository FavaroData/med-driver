#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "dlg_progress.h"
#include "resource.h"
#include "ui/theme.h"
#include "ui/buttons.h"

#define WM_APP_PS_OUTPUT  (WM_APP + 1)
#define WM_APP_PS_DONE    (WM_APP + 2)

typedef struct {
    HWND    hwnd;
    wchar_t scriptPath[MAX_PATH];
    wchar_t printerName[256];
    wchar_t outputPath[MAX_PATH];
    wchar_t outputBaseName[256];
    BOOL    removeMode;
} ProgressParams;

static BOOL   s_done;
static BOOL   s_success;
static BOOL   s_removeMode;

static void append_text(HWND hEdit, const wchar_t *text) {
    int len = GetWindowTextLengthW(hEdit);
    SendMessageW(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
}

/* ─── Thread: lanca PS via CreateProcess e lê stdout em tempo real ───── */

static DWORD run_ps(ProgressParams *p, const wchar_t *cmd) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        PostMessage(p->hwnd, WM_APP_PS_DONE, 1, 0);
        return 1;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;
    si.hStdInput  = NULL;

    PROCESS_INFORMATION pi = {0};
    wchar_t cmdBuf[4096];
    wcsncpy_s(cmdBuf, 4096, cmd, _TRUNCATE);

    BOOL ok = CreateProcessW(NULL, cmdBuf, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWrite);

    if (!ok) {
        DWORD err = GetLastError();
        wchar_t msg[200];
        _snwprintf_s(msg, 200, _TRUNCATE,
            L"\r\n[Erro ao iniciar PowerShell: código %lu]\r\n", err);
        wchar_t *copy = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, 200 * sizeof(wchar_t));
        if (copy) {
            wcscpy_s(copy, 200, msg);
            PostMessage(p->hwnd, WM_APP_PS_OUTPUT, 0, (LPARAM)copy);
        }
        CloseHandle(hRead);
        PostMessage(p->hwnd, WM_APP_PS_DONE, 1, 0);
        return 1;
    }

    /* Lê stdout do PS em tempo real */
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, (int)bytesRead, NULL, 0);
        if (wlen > 0) {
            wchar_t *wbuf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                                  (wlen + 1) * sizeof(wchar_t));
            if (wbuf) {
                MultiByteToWideChar(CP_UTF8, 0, buf, (int)bytesRead, wbuf, wlen);
                wbuf[wlen] = 0;
                PostMessage(p->hwnd, WM_APP_PS_OUTPUT, 0, (LPARAM)wbuf);
            }
        }
    }
    CloseHandle(hRead);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    PostMessage(p->hwnd, WM_APP_PS_DONE, (WPARAM)exitCode, 0);
    return exitCode;
}

static DWORD WINAPI ps_thread(LPVOID param) {
    ProgressParams *p = (ProgressParams *)param;
    wchar_t cmd[4096];
    _snwprintf_s(cmd, 4096, _TRUNCATE,
        L"powershell.exe -ExecutionPolicy Bypass -NoProfile -File \"%s\""
        L" -PrinterName \"%s\" -OutputPath \"%s\" -OutputBaseName \"%s\"",
        p->scriptPath, p->printerName, p->outputPath, p->outputBaseName);
    DWORD r = run_ps(p, cmd);
    HeapFree(GetProcessHeap(), 0, p);
    return r;
}

static DWORD WINAPI ps_thread_remove(LPVOID param) {
    ProgressParams *p = (ProgressParams *)param;
    wchar_t cmd[4096];
    _snwprintf_s(cmd, 4096, _TRUNCATE,
        L"powershell.exe -ExecutionPolicy Bypass -NoProfile -File \"%s\""
        L" -PrinterName \"%s\"",
        p->scriptPath, p->printerName);
    DWORD r = run_ps(p, cmd);
    HeapFree(GetProcessHeap(), 0, p);
    return r;
}

/* ─── Desenho do botao owner-draw ─────────────────────────────────────── */

static void draw_close_btn(DRAWITEMSTRUCT *di) {
    HDC  dc  = di->hDC;
    RECT rc  = di->rcItem;
    BOOL d   = (di->itemState & ODS_DISABLED) != 0;
    BOOL hot = (di->itemState & ODS_HOTLIGHT) != 0
            || GetWindowLongPtrW(di->hwndItem, GWLP_USERDATA) != 0;
    BOOL sel = (di->itemState & ODS_SELECTED) != 0;

    COLORREF bg = d   ? CLR_BTN_SECONDARY :
                  sel ? CLR_CARD          :
                  hot ? CLR_BTN_SEC_HOV   : CLR_BTN_SECONDARY;

    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(dc, &rc, hbr);
    DeleteObject(hbr);

    wchar_t txt[64];
    GetWindowTextW(di->hwndItem, txt, 64);
    SetTextColor(dc, d ? CLR_TEXT_DISABLED : CLR_TEXT_PRIMARY);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontContent);
    DrawTextW(dc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

/* ─── Dialog proc ─────────────────────────────────────────────────────── */

static INT_PTR CALLBACK ProgressDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        s_done    = FALSE;
        s_success = FALSE;

        SendDlgItemMessageW(hwnd, IDC_EDIT_OUTPUT,        WM_SETFONT, (WPARAM)g_fontContent, TRUE);
        SendDlgItemMessageW(hwnd, IDC_BTN_CLOSE_PROGRESS, WM_SETFONT, (WPARAM)g_fontContent, TRUE);

        HWND hBtn = GetDlgItem(hwnd, IDC_BTN_CLOSE_PROGRESS);
        LONG_PTR style = GetWindowLongPtrW(hBtn, GWL_STYLE);
        SetWindowLongPtrW(hBtn, GWL_STYLE, (style & ~0xFL) | BS_OWNERDRAW);
        buttons_install_hover(hBtn);
        EnableWindow(hBtn, FALSE);

        ProgressParams *p = (ProgressParams *)lp;
        s_removeMode = p->removeMode;
        p->hwnd = hwnd;
        SetWindowTextW(hwnd, s_removeMode ? L"Removendo Impressora" : L"Adicionando Impressora");
        LPTHREAD_START_ROUTINE fn = p->removeMode ? ps_thread_remove : ps_thread;
        HANDLE hThread = CreateThread(NULL, 0, fn, p, 0, NULL);
        if (hThread) CloseHandle(hThread);
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return (INT_PTR)g_hbrPrimary;

    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wp, CLR_TEXT_PRIMARY);
        SetBkColor((HDC)wp,   CLR_BG_SECONDARY);
        return (INT_PTR)g_hbrSecondary;

    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)wp, CLR_TEXT_SECONDARY);
        SetBkMode((HDC)wp, TRANSPARENT);
        return (INT_PTR)g_hbrPrimary;

    case WM_DRAWITEM:
        draw_close_btn((DRAWITEMSTRUCT *)lp);
        return TRUE;

    case WM_APP_PS_OUTPUT: {
        wchar_t *text = (wchar_t *)lp;
        HWND hEdit = GetDlgItem(hwnd, IDC_EDIT_OUTPUT);
        append_text(hEdit, text);
        HeapFree(GetProcessHeap(), 0, text);
        return 0;
    }

    case WM_APP_PS_DONE: {
        s_done    = TRUE;
        s_success = ((DWORD)wp == 0);
        HWND hEdit = GetDlgItem(hwnd, IDC_EDIT_OUTPUT);
        if (s_removeMode) {
            if (s_success)
                append_text(hEdit, L"\r\n--- Impressora removida com sucesso. ---\r\n");
            else
                append_text(hEdit, L"\r\n--- Falha ao remover impressora. ---\r\n");
        } else {
            if (s_success)
                append_text(hEdit, L"\r\n--- Impressora adicionada com sucesso. ---\r\n");
            else
                append_text(hEdit, L"\r\n--- Falha ao adicionar impressora. ---\r\n");
        }
        EnableWindow(GetDlgItem(hwnd, IDC_BTN_CLOSE_PROGRESS), TRUE);
        InvalidateRect(GetDlgItem(hwnd, IDC_BTN_CLOSE_PROGRESS), NULL, TRUE);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_BTN_CLOSE_PROGRESS && s_done) {
            EndDialog(hwnd, s_success ? IDOK : IDCANCEL);
        }
        return 0;

    case WM_CLOSE:
        if (s_done)
            EndDialog(hwnd, s_success ? IDOK : IDCANCEL);
        return 0;

    }
    return FALSE;
}

/* ─── API publica ──────────────────────────────────────────────────────── */

BOOL dlg_progress_run(HWND parent,
                      const wchar_t *printerName,
                      const wchar_t *outputPath,
                      const wchar_t *outputBaseName) {
    wchar_t scriptPath[MAX_PATH];
    GetModuleFileNameW(NULL, scriptPath, MAX_PATH);
    wchar_t *slash = wcsrchr(scriptPath, L'\\');
    if (slash) *(slash + 1) = L'\0';
    wcsncat_s(scriptPath, MAX_PATH, L"add-printer.ps1", _TRUNCATE);

    if (GetFileAttributesW(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(parent,
            L"Arquivo add-printer.ps1 não encontrado.\r\n"
            L"Certifique-se de que está na mesma pasta que MedDriveManager.exe.",
            L"Erro", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    ProgressParams *params = (ProgressParams *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ProgressParams));
    if (!params) return FALSE;
    wcsncpy_s(params->scriptPath,    MAX_PATH, scriptPath,    _TRUNCATE);
    wcsncpy_s(params->printerName,   256,      printerName,   _TRUNCATE);
    wcsncpy_s(params->outputPath,    MAX_PATH, outputPath,    _TRUNCATE);
    wcsncpy_s(params->outputBaseName,256,      outputBaseName,_TRUNCATE);
    params->removeMode = FALSE;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result = DialogBoxParamW(hInst,
                                     MAKEINTRESOURCEW(IDD_PROGRESS),
                                     parent,
                                     ProgressDlgProc,
                                     (LPARAM)params);
    return result == IDOK;
}

BOOL dlg_progress_remove(HWND parent, const wchar_t *printerName) {
    wchar_t scriptPath[MAX_PATH];
    GetModuleFileNameW(NULL, scriptPath, MAX_PATH);
    wchar_t *slash = wcsrchr(scriptPath, L'\\');
    if (slash) *(slash + 1) = L'\0';
    wcsncat_s(scriptPath, MAX_PATH, L"remove-printer.ps1", _TRUNCATE);

    if (GetFileAttributesW(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(parent,
            L"Arquivo remove-printer.ps1 não encontrado.\r\n"
            L"Certifique-se de que está na mesma pasta que MedDriveManager.exe.",
            L"Erro", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    ProgressParams *params = (ProgressParams *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ProgressParams));
    if (!params) return FALSE;
    wcsncpy_s(params->scriptPath,  MAX_PATH, scriptPath,  _TRUNCATE);
    wcsncpy_s(params->printerName, 256,      printerName, _TRUNCATE);
    params->removeMode = TRUE;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result = DialogBoxParamW(hInst,
                                     MAKEINTRESOURCEW(IDD_PROGRESS),
                                     parent,
                                     ProgressDlgProc,
                                     (LPARAM)params);
    return result == IDOK;
}
