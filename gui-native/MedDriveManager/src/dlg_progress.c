#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include "dlg_progress.h"
#include "resource.h"

#define WM_APP_PS_OUTPUT  (WM_APP + 1)  /* lParam = heap wchar_t*, dialog free */
#define WM_APP_PS_DONE    (WM_APP + 2)  /* wParam = exit code                  */

#define PROGRESS_LOG        L"C:\\Windows\\Temp\\meddrive_ps_addprinter.log"
#define PROGRESS_LOG_REMOVE L"C:\\Windows\\Temp\\meddrive_ps_removeprinter.log"

/* Cores escuras */
#define CLR_BG         RGB(0x1E,0x1E,0x1E)
#define CLR_EDIT_BG    RGB(0x16,0x16,0x16)
#define CLR_TEXT       RGB(0xD4,0xD4,0xD4)
#define CLR_BTN_BG     RGB(0x3C,0x3C,0x3C)
#define CLR_BTN_HOV    RGB(0x50,0x50,0x50)
#define CLR_BTN_PRS    RGB(0x28,0x28,0x28)
#define CLR_BTN_BORDER RGB(0x60,0x60,0x60)
#define CLR_BTN_TEXT   RGB(0xD4,0xD4,0xD4)
#define CLR_BTN_DIS    RGB(0x55,0x55,0x55)
#define CLR_BTN_DISTXT RGB(0x50,0x50,0x50)

typedef struct {
    HWND    hwnd;
    wchar_t scriptPath[MAX_PATH];
    wchar_t printerName[256];
    wchar_t outputPath[MAX_PATH];
    BOOL    removeMode;
} ProgressParams;

static HBRUSH s_hbrBg;
static HBRUSH s_hbrEdit;
static HFONT  s_hFont;
static BOOL   s_done;
static BOOL   s_success;
static BOOL   s_removeMode;

/* ─── Utilitarios ─────────────────────────────────────────────────────── */

static HFONT make_font(int ptSize) {
    HDC dc = GetDC(NULL);
    HFONT f = CreateFontW(
        -MulDiv(ptSize, GetDeviceCaps(dc, LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    ReleaseDC(NULL, dc);
    return f;
}

static void append_text(HWND hEdit, const wchar_t *text) {
    int len = GetWindowTextLengthW(hEdit);
    SendMessageW(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
}

/* ─── Thread de execucao do PowerShell ───────────────────────────────── */

static void post_text(HWND hwnd, const BYTE *raw, DWORD bytes, BOOL utf16) {
    if (bytes == 0) return;
    wchar_t *wbuf;
    if (utf16) {
        DWORD wchars = bytes / 2;
        wbuf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                    (wchars + 1) * sizeof(wchar_t));
        if (!wbuf) return;
        memcpy(wbuf, raw, wchars * 2);
        wbuf[wchars] = 0;
    } else {
        int n = MultiByteToWideChar(CP_ACP, 0, (const char *)raw, (int)bytes, NULL, 0);
        if (n <= 0) return;
        wbuf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (n + 1) * sizeof(wchar_t));
        if (!wbuf) return;
        MultiByteToWideChar(CP_ACP, 0, (const char *)raw, (int)bytes, wbuf, n);
        wbuf[n] = 0;
    }
    PostMessage(hwnd, WM_APP_PS_OUTPUT, 0, (LPARAM)wbuf);
}

static void read_log(HWND hwnd, const wchar_t *logPath,
                     DWORD *pOffset, BOOL *pUtf16, BOOL *pBomChecked) {
    HANDLE hf = CreateFileW(logPath, GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) return;

    DWORD fileSize = GetFileSize(hf, NULL);
    DWORD offset   = *pOffset;
    if (fileSize <= offset) { CloseHandle(hf); return; }

    DWORD toRead = fileSize - offset;
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, toRead + 4);
    if (!buf) { CloseHandle(hf); return; }

    DWORD bytesRead = 0;
    SetFilePointer(hf, (LONG)offset, NULL, FILE_BEGIN);
    ReadFile(hf, buf, toRead, &bytesRead, NULL);
    CloseHandle(hf);

    if (bytesRead == 0) { HeapFree(GetProcessHeap(), 0, buf); return; }
    buf[bytesRead] = buf[bytesRead + 1] = buf[bytesRead + 2] = 0;

    BYTE *data    = buf;
    DWORD dataLen = bytesRead;

    /* Detecta BOM UTF-16 LE no inicio do arquivo */
    if (!*pBomChecked && offset == 0 && bytesRead >= 2) {
        *pBomChecked = TRUE;
        if (buf[0] == 0xFF && buf[1] == 0xFE) {
            *pUtf16 = TRUE;
            data    += 2;
            dataLen -= 2;
            *pOffset += 2;
        }
    }

    if (dataLen == 0) { HeapFree(GetProcessHeap(), 0, buf); return; }

    if (*pUtf16) {
        DWORD even = dataLen & ~1u;
        if (even > 0) {
            post_text(hwnd, data, even, TRUE);
            *pOffset += even;
        }
    } else {
        post_text(hwnd, data, dataLen, FALSE);
        *pOffset += dataLen;
    }

    HeapFree(GetProcessHeap(), 0, buf);
}

static DWORD WINAPI ps_thread(LPVOID param) {
    ProgressParams *p = (ProgressParams *)param;

    DeleteFileW(PROGRESS_LOG);

    wchar_t ps_params[4096];
    _snwprintf_s(ps_params, 4096, _TRUNCATE,
        L"-ExecutionPolicy Bypass -NoProfile -File \"%s\""
        L" -PrinterName \"%s\" -OutputPath \"%s\"",
        p->scriptPath, p->printerName, p->outputPath);

    SHELLEXECUTEINFOW sei = {sizeof(sei)};
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = L"runas";
    sei.lpFile       = L"powershell.exe";
    sei.lpParameters = ps_params;
    sei.nShow        = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        wchar_t msg[200];
        if (err == ERROR_CANCELLED)
            wcscpy_s(msg, 200, L"\r\n[Operação cancelada pelo usuário.]\r\n");
        else
            _snwprintf_s(msg, 200, _TRUNCATE,
                L"\r\n[Erro ao iniciar PowerShell: código %lu]\r\n", err);

        wchar_t *copy = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, 200 * sizeof(wchar_t));
        if (copy) {
            wcscpy_s(copy, 200, msg);
            PostMessage(p->hwnd, WM_APP_PS_OUTPUT, 0, (LPARAM)copy);
        }
        PostMessage(p->hwnd, WM_APP_PS_DONE, 1, 0);
        HeapFree(GetProcessHeap(), 0, p);
        return 1;
    }

    HANDLE hProcess = sei.hProcess;

    for (int i = 0; i < 50; i++) {
        DWORD attr = GetFileAttributesW(PROGRESS_LOG);
        if (attr != INVALID_FILE_ATTRIBUTES) break;
        Sleep(100);
    }

    DWORD offset     = 0;
    BOOL  utf16      = FALSE;
    BOOL  bomChecked = FALSE;

    for (;;) {
        BOOL done = (WaitForSingleObject(hProcess, 300) != WAIT_TIMEOUT);
        read_log(p->hwnd, PROGRESS_LOG, &offset, &utf16, &bomChecked);
        if (done) break;
    }
    read_log(p->hwnd, PROGRESS_LOG, &offset, &utf16, &bomChecked);

    DWORD exitCode = 1;
    GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);

    PostMessage(p->hwnd, WM_APP_PS_DONE, (WPARAM)exitCode, 0);
    HeapFree(GetProcessHeap(), 0, p);
    return 0;
}

static DWORD WINAPI ps_thread_remove(LPVOID param) {
    ProgressParams *p = (ProgressParams *)param;

    DeleteFileW(PROGRESS_LOG_REMOVE);

    wchar_t ps_params[4096];
    _snwprintf_s(ps_params, 4096, _TRUNCATE,
        L"-ExecutionPolicy Bypass -NoProfile -File \"%s\""
        L" -PrinterName \"%s\"",
        p->scriptPath, p->printerName);

    SHELLEXECUTEINFOW sei = {sizeof(sei)};
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = L"runas";
    sei.lpFile       = L"powershell.exe";
    sei.lpParameters = ps_params;
    sei.nShow        = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        wchar_t msg[200];
        if (err == ERROR_CANCELLED)
            wcscpy_s(msg, 200, L"\r\n[Operação cancelada pelo usuário.]\r\n");
        else
            _snwprintf_s(msg, 200, _TRUNCATE,
                L"\r\n[Erro ao iniciar PowerShell: código %lu]\r\n", err);

        wchar_t *copy = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, 200 * sizeof(wchar_t));
        if (copy) {
            wcscpy_s(copy, 200, msg);
            PostMessage(p->hwnd, WM_APP_PS_OUTPUT, 0, (LPARAM)copy);
        }
        PostMessage(p->hwnd, WM_APP_PS_DONE, 1, 0);
        HeapFree(GetProcessHeap(), 0, p);
        return 1;
    }

    HANDLE hProcess = sei.hProcess;

    for (int i = 0; i < 50; i++) {
        DWORD attr = GetFileAttributesW(PROGRESS_LOG_REMOVE);
        if (attr != INVALID_FILE_ATTRIBUTES) break;
        Sleep(100);
    }

    DWORD offset     = 0;
    BOOL  utf16      = FALSE;
    BOOL  bomChecked = FALSE;

    for (;;) {
        BOOL done = (WaitForSingleObject(hProcess, 300) != WAIT_TIMEOUT);
        read_log(p->hwnd, PROGRESS_LOG_REMOVE, &offset, &utf16, &bomChecked);
        if (done) break;
    }
    read_log(p->hwnd, PROGRESS_LOG_REMOVE, &offset, &utf16, &bomChecked);

    DWORD exitCode = 1;
    GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);

    PostMessage(p->hwnd, WM_APP_PS_DONE, (WPARAM)exitCode, 0);
    HeapFree(GetProcessHeap(), 0, p);
    return 0;
}

/* ─── Desenho do botao owner-draw ─────────────────────────────────────── */

static void draw_close_btn(DRAWITEMSTRUCT *di) {
    HDC  dc  = di->hDC;
    RECT rc  = di->rcItem;
    BOOL dis = (di->itemState & ODS_DISABLED) != 0;
    BOOL hot = (di->itemState & ODS_HOTLIGHT) != 0;
    BOOL sel = (di->itemState & ODS_SELECTED) != 0;

    COLORREF bg = dis ? CLR_BG :
                  sel ? CLR_BTN_PRS :
                  hot ? CLR_BTN_HOV : CLR_BTN_BG;
    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(dc, &rc, hbr);
    DeleteObject(hbr);

    HPEN hpen = CreatePen(PS_SOLID, 1, dis ? RGB(0x40,0x40,0x40) : CLR_BTN_BORDER);
    HPEN opn  = (HPEN)SelectObject(dc, hpen);
    HBRUSH obr = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dc, opn);
    SelectObject(dc, obr);
    DeleteObject(hpen);

    wchar_t txt[64];
    GetWindowTextW(di->hwndItem, txt, 64);
    SetTextColor(dc, dis ? CLR_BTN_DISTXT : CLR_BTN_TEXT);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, s_hFont);
    DrawTextW(dc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

/* ─── Dialog proc ─────────────────────────────────────────────────────── */

static INT_PTR CALLBACK ProgressDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        s_done       = FALSE;
        s_success    = FALSE;

        s_hbrBg   = CreateSolidBrush(CLR_BG);
        s_hbrEdit = CreateSolidBrush(CLR_EDIT_BG);
        s_hFont   = make_font(9);

        SendDlgItemMessageW(hwnd, IDC_EDIT_OUTPUT,        WM_SETFONT, (WPARAM)s_hFont, TRUE);
        SendDlgItemMessageW(hwnd, IDC_BTN_CLOSE_PROGRESS, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        HWND hBtn = GetDlgItem(hwnd, IDC_BTN_CLOSE_PROGRESS);
        LONG_PTR style = GetWindowLongPtrW(hBtn, GWL_STYLE);
        SetWindowLongPtrW(hBtn, GWL_STYLE, style | BS_OWNERDRAW);
        EnableWindow(hBtn, FALSE);

        ProgressParams *p = (ProgressParams *)lp;
        s_removeMode = p->removeMode;
        p->hwnd = hwnd;
        LPTHREAD_START_ROUTINE fn = p->removeMode ? ps_thread_remove : ps_thread;
        HANDLE hThread = CreateThread(NULL, 0, fn, p, 0, NULL);
        if (hThread) CloseHandle(hThread);
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return (INT_PTR)s_hbrBg;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)wp, CLR_TEXT);
        SetBkColor((HDC)wp, CLR_EDIT_BG);
        return (INT_PTR)s_hbrEdit;

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

    case WM_DESTROY:
        if (s_hbrBg)   { DeleteObject(s_hbrBg);   s_hbrBg   = NULL; }
        if (s_hbrEdit) { DeleteObject(s_hbrEdit);  s_hbrEdit = NULL; }
        if (s_hFont)   { DeleteObject(s_hFont);    s_hFont   = NULL; }
        return 0;
    }
    return FALSE;
}

/* ─── API publica ──────────────────────────────────────────────────────── */

BOOL dlg_progress_run(HWND parent,
                      const wchar_t *printerName,
                      const wchar_t *outputPath) {
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
    wcsncpy_s(params->scriptPath,  MAX_PATH, scriptPath,  _TRUNCATE);
    wcsncpy_s(params->printerName, 256,      printerName, _TRUNCATE);
    wcsncpy_s(params->outputPath,  MAX_PATH, outputPath,  _TRUNCATE);
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
