#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "dlg_progress.h"
#include "resource.h"
#include "ui/theme.h"
#include "ui/buttons.h"

#define WM_APP_PS_OUTPUT  (WM_APP + 1)
#define WM_APP_PS_DONE    (WM_APP + 2)

typedef enum { MODE_ADD, MODE_REMOVE, MODE_CREATE_PROFILE,
               MODE_EDIT_PROFILE, MODE_REMOVE_PROFILE,
               MODE_EDIT_PRINTER } ProgressMode;

typedef struct {
    HWND         hwnd;
    ProgressMode mode;
    wchar_t      scriptPath[MAX_PATH];
    wchar_t      printerName[256];
    wchar_t      newPrinterName[256];
    wchar_t      profileName[256];
    wchar_t      newProfileName[256];
    wchar_t      outputPath[MAX_PATH];
    wchar_t      outputBaseName[256];
    wchar_t      gsPath[MAX_PATH];
    BOOL         openAfterGenerate;
    BOOL         overwriteFile;
    BOOL         choosePath;
} ProgressParams;

static void load_gs_path(wchar_t *out, int len) {
    wchar_t ini[MAX_PATH];
    ExpandEnvironmentStringsW(
        L"%ProgramData%\\Meddrive Printer\\settings.ini", ini, MAX_PATH);
    wchar_t def[MAX_PATH];
    ExpandEnvironmentStringsW(
        L"%ProgramData%\\Meddrive Printer\\Ghostscript\\bin\\gswin64c.exe", def, MAX_PATH);
    GetPrivateProfileStringW(L"Ghostscript", L"ExecutablePath", def, out, len, ini);
}

static BOOL         s_done;
static BOOL         s_success;
static ProgressMode s_mode;

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
        L" -ProfileName \"%s\" -PrinterName \"%s\"",
        p->scriptPath, p->profileName, p->printerName);
    DWORD r = run_ps(p, cmd);
    HeapFree(GetProcessHeap(), 0, p);
    return r;
}

static DWORD WINAPI ps_thread_create_profile(LPVOID param) {
    ProgressParams *p = (ProgressParams *)param;
    wchar_t cmd[4096];
    _snwprintf_s(cmd, 4096, _TRUNCATE,
        L"powershell.exe -ExecutionPolicy Bypass -NoProfile -File \"%s\""
        L" -ProfileName \"%s\" -OutputPath \"%s\" -OutputBaseName \"%s\""
        L" -GhostscriptPath \"%s\"%s%s%s",
        p->scriptPath, p->printerName, p->outputPath, p->outputBaseName,
        p->gsPath,
        p->openAfterGenerate ? L" -OpenAfterGenerate" : L"",
        p->overwriteFile     ? L" -OverwriteFile"     : L"",
        p->choosePath        ? L" -ChoosePath"        : L"");
    DWORD r = run_ps(p, cmd);
    HeapFree(GetProcessHeap(), 0, p);
    return r;
}

static DWORD WINAPI ps_thread_edit_profile(LPVOID param) {
    ProgressParams *p = (ProgressParams *)param;
    wchar_t cmd[4096];
    _snwprintf_s(cmd, 4096, _TRUNCATE,
        L"powershell.exe -ExecutionPolicy Bypass -NoProfile -File \"%s\""
        L" -ProfileName \"%s\" -NewName \"%s\""
        L" -OutputPath \"%s\" -OutputBaseName \"%s\""
        L" -GhostscriptPath \"%s\"%s%s%s",
        p->scriptPath, p->profileName, p->newProfileName,
        p->outputPath, p->outputBaseName,
        p->gsPath,
        p->openAfterGenerate ? L" -OpenAfterGenerate" : L"",
        p->overwriteFile     ? L" -OverwriteFile"     : L"",
        p->choosePath        ? L" -ChoosePath"        : L"");
    DWORD r = run_ps(p, cmd);
    HeapFree(GetProcessHeap(), 0, p);
    return r;
}

static DWORD WINAPI ps_thread_remove_profile(LPVOID param) {
    ProgressParams *p = (ProgressParams *)param;
    wchar_t cmd[4096];
    _snwprintf_s(cmd, 4096, _TRUNCATE,
        L"powershell.exe -ExecutionPolicy Bypass -NoProfile -File \"%s\""
        L" -ProfileName \"%s\"",
        p->scriptPath, p->profileName);
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

static DWORD WINAPI ps_thread_edit_printer(LPVOID param) {
    ProgressParams *p = (ProgressParams *)param;
    wchar_t cmd[4096];
    _snwprintf_s(cmd, 4096, _TRUNCATE,
        L"powershell.exe -ExecutionPolicy Bypass -NoProfile -File \"%s\""
        L" -OldPrinterName \"%s\" -NewPrinterName \"%s\" -ProfileName \"%s\"",
        p->scriptPath, p->printerName, p->newPrinterName, p->profileName);
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

    COLORREF bg = d   ? CLR_BTN_SEC_HOV  :
                  sel ? CLR_BTN_SEC_HOV  :
                  hot ? CLR_BTN_SEC_HOV  : CLR_BTN_SECONDARY;

    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(dc, &rc, hbr);
    DeleteObject(hbr);

    HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
    FrameRect(dc, &rc, hbrd);
    DeleteObject(hbrd);

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
        s_mode  = p->mode;
        p->hwnd = hwnd;

        LPTHREAD_START_ROUTINE fn;
        const wchar_t *title;
        if (s_mode == MODE_EDIT_PRINTER) {
            fn = ps_thread_edit_printer;   title = L"Editando Impressora";
        } else if (s_mode == MODE_REMOVE) {
            fn = ps_thread_remove;         title = L"Removendo Impressora";
        } else if (s_mode == MODE_CREATE_PROFILE) {
            fn = ps_thread_create_profile; title = L"Criando Perfil";
        } else if (s_mode == MODE_EDIT_PROFILE) {
            fn = ps_thread_edit_profile;   title = L"Editando Perfil";
        } else if (s_mode == MODE_REMOVE_PROFILE) {
            fn = ps_thread_remove_profile; title = L"Removendo Perfil";
        } else {
            fn = ps_thread;                title = L"Adicionando Impressora";
        }
        SetWindowTextW(hwnd, title);
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

    case WM_CTLCOLORSTATIC: {
        BOOL isSection = (GetWindowLongPtrW((HWND)lp, GWLP_ID) == IDC_SECTION_LBL);
        SetTextColor((HDC)wp, isSection ? CLR_ACCENT : CLR_TEXT_SECONDARY);
        SetBkMode((HDC)wp, TRANSPARENT);
        return (INT_PTR)g_hbrPrimary;
    }

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
        if (s_mode == MODE_REMOVE) {
            append_text(hEdit, s_success
                ? L"\r\n--- Impressora removida com sucesso. ---\r\n"
                : L"\r\n--- Falha ao remover impressora. ---\r\n");
        } else if (s_mode == MODE_CREATE_PROFILE) {
            append_text(hEdit, s_success
                ? L"\r\n--- Perfil criado com sucesso. ---\r\n"
                : L"\r\n--- Falha ao criar perfil. ---\r\n");
        } else if (s_mode == MODE_EDIT_PROFILE) {
            append_text(hEdit, s_success
                ? L"\r\n--- Perfil editado com sucesso. ---\r\n"
                : L"\r\n--- Falha ao editar perfil. ---\r\n");
        } else if (s_mode == MODE_REMOVE_PROFILE) {
            append_text(hEdit, s_success
                ? L"\r\n--- Perfil removido com sucesso. ---\r\n"
                : L"\r\n--- Falha ao remover perfil. ---\r\n");
        } else if (s_mode == MODE_EDIT_PRINTER) {
            append_text(hEdit, s_success
                ? L"\r\n--- Perfil da impressora atualizado com sucesso. ---\r\n"
                : L"\r\n--- Falha ao atualizar perfil da impressora. ---\r\n");
        } else {
            append_text(hEdit, s_success
                ? L"\r\n--- Impressora adicionada com sucesso. ---\r\n"
                : L"\r\n--- Falha ao adicionar impressora. ---\r\n");
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

static void build_script_path(wchar_t *out, const wchar_t *name)
{
    GetModuleFileNameW(NULL, out, MAX_PATH);
    wchar_t *slash = wcsrchr(out, L'\\');
    if (slash) *(slash + 1) = L'\0';
    wcsncat_s(out, MAX_PATH, L"conf\\", _TRUNCATE);
    wcsncat_s(out, MAX_PATH, name, _TRUNCATE);
}

BOOL dlg_progress_run(HWND parent,
                      const wchar_t *printerName,
                      const wchar_t *profileName) {
    wchar_t scriptPath[MAX_PATH];
    build_script_path(scriptPath, L"add-printer.ps1");

    if (GetFileAttributesW(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(parent,
            L"Arquivo conf\\add-printer.ps1 não encontrado.\r\n"
            L"Certifique-se de que a pasta conf\\ está junto a MedDriveManager.exe.",
            L"Erro", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    ProgressParams *params = (ProgressParams *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ProgressParams));
    if (!params) return FALSE;
    wcsncpy_s(params->scriptPath,  MAX_PATH, scriptPath,  _TRUNCATE);
    wcsncpy_s(params->printerName, 256,      printerName, _TRUNCATE);
    wcsncpy_s(params->profileName, 256,      profileName, _TRUNCATE);
    params->mode = MODE_ADD;

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
    build_script_path(scriptPath, L"remove-printer.ps1");

    if (GetFileAttributesW(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(parent,
            L"Arquivo conf\\remove-printer.ps1 não encontrado.\r\n"
            L"Certifique-se de que a pasta conf\\ está junto a MedDriveManager.exe.",
            L"Erro", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    ProgressParams *params = (ProgressParams *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ProgressParams));
    if (!params) return FALSE;
    wcsncpy_s(params->scriptPath,  MAX_PATH, scriptPath,  _TRUNCATE);
    wcsncpy_s(params->printerName, 256,      printerName, _TRUNCATE);
    params->mode = MODE_REMOVE;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result = DialogBoxParamW(hInst,
                                     MAKEINTRESOURCEW(IDD_PROGRESS),
                                     parent,
                                     ProgressDlgProc,
                                     (LPARAM)params);
    return result == IDOK;
}

BOOL dlg_progress_edit_profile(HWND parent,
                                const wchar_t *profileName,
                                const wchar_t *newProfileName,
                                const wchar_t *outputPath,
                                const wchar_t *outputBaseName,
                                BOOL openAfterGenerate,
                                BOOL overwriteFile,
                                BOOL choosePath) {
    wchar_t scriptPath[MAX_PATH];
    build_script_path(scriptPath, L"edit-profile.ps1");

    if (GetFileAttributesW(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(parent,
            L"Arquivo conf\\edit-profile.ps1 não encontrado.\r\n"
            L"Certifique-se de que a pasta conf\\ está junto a MedDriveManager.exe.",
            L"Erro", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    ProgressParams *params = (ProgressParams *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ProgressParams));
    if (!params) return FALSE;
    wcsncpy_s(params->scriptPath,     MAX_PATH, scriptPath,      _TRUNCATE);
    wcsncpy_s(params->profileName,    256,      profileName,     _TRUNCATE);
    wcsncpy_s(params->newProfileName, 256,      newProfileName,  _TRUNCATE);
    wcsncpy_s(params->outputPath,     MAX_PATH, outputPath,      _TRUNCATE);
    wcsncpy_s(params->outputBaseName, 256,      outputBaseName,  _TRUNCATE);
    load_gs_path(params->gsPath, MAX_PATH);
    params->mode              = MODE_EDIT_PROFILE;
    params->openAfterGenerate = openAfterGenerate;
    params->overwriteFile     = overwriteFile;
    params->choosePath        = choosePath;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result = DialogBoxParamW(hInst,
                                     MAKEINTRESOURCEW(IDD_PROGRESS),
                                     parent,
                                     ProgressDlgProc,
                                     (LPARAM)params);
    return result == IDOK;
}

BOOL dlg_progress_remove_profile(HWND parent, const wchar_t *profileName) {
    wchar_t scriptPath[MAX_PATH];
    build_script_path(scriptPath, L"remove-profile.ps1");

    if (GetFileAttributesW(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(parent,
            L"Arquivo conf\\remove-profile.ps1 não encontrado.\r\n"
            L"Certifique-se de que a pasta conf\\ está junto a MedDriveManager.exe.",
            L"Erro", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    ProgressParams *params = (ProgressParams *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ProgressParams));
    if (!params) return FALSE;
    wcsncpy_s(params->scriptPath,  MAX_PATH, scriptPath,  _TRUNCATE);
    wcsncpy_s(params->profileName, 256,      profileName, _TRUNCATE);
    params->mode = MODE_REMOVE_PROFILE;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result = DialogBoxParamW(hInst,
                                     MAKEINTRESOURCEW(IDD_PROGRESS),
                                     parent,
                                     ProgressDlgProc,
                                     (LPARAM)params);
    return result == IDOK;
}

BOOL dlg_progress_edit_printer(HWND parent,
                                const wchar_t *oldPrinterName,
                                const wchar_t *newPrinterName,
                                const wchar_t *profileName) {
    wchar_t scriptPath[MAX_PATH];
    build_script_path(scriptPath, L"edit-printer.ps1");

    if (GetFileAttributesW(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(parent,
            L"Arquivo conf\\edit-printer.ps1 não encontrado.\r\n"
            L"Certifique-se de que a pasta conf\\ está junto a MedDriveManager.exe.",
            L"Erro", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    ProgressParams *params = (ProgressParams *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ProgressParams));
    if (!params) return FALSE;
    wcsncpy_s(params->scriptPath,     MAX_PATH, scriptPath,     _TRUNCATE);
    wcsncpy_s(params->printerName,    256,      oldPrinterName, _TRUNCATE);
    wcsncpy_s(params->newPrinterName, 256,      newPrinterName, _TRUNCATE);
    wcsncpy_s(params->profileName,    256,      profileName,    _TRUNCATE);
    params->mode = MODE_EDIT_PRINTER;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result = DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_PROGRESS),
                                     parent, ProgressDlgProc, (LPARAM)params);
    return result == IDOK;
}

BOOL dlg_progress_create_profile(HWND parent,
                                  const wchar_t *profileName,
                                  const wchar_t *outputPath,
                                  const wchar_t *outputBaseName,
                                  BOOL openAfterGenerate,
                                  BOOL overwriteFile,
                                  BOOL choosePath) {
    wchar_t scriptPath[MAX_PATH];
    build_script_path(scriptPath, L"create-profile.ps1");

    if (GetFileAttributesW(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(parent,
            L"Arquivo conf\\create-profile.ps1 não encontrado.\r\n"
            L"Certifique-se de que a pasta conf\\ está junto a MedDriveManager.exe.",
            L"Erro", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    ProgressParams *params = (ProgressParams *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ProgressParams));
    if (!params) return FALSE;
    wcsncpy_s(params->scriptPath,     MAX_PATH, scriptPath,    _TRUNCATE);
    wcsncpy_s(params->printerName,    256,      profileName,   _TRUNCATE);
    wcsncpy_s(params->outputPath,     MAX_PATH, outputPath,    _TRUNCATE);
    wcsncpy_s(params->outputBaseName, 256,      outputBaseName,_TRUNCATE);
    load_gs_path(params->gsPath, MAX_PATH);
    params->mode              = MODE_CREATE_PROFILE;
    params->openAfterGenerate = openAfterGenerate;
    params->overwriteFile     = overwriteFile;
    params->choosePath        = choosePath;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result = DialogBoxParamW(hInst,
                                     MAKEINTRESOURCEW(IDD_PROGRESS),
                                     parent,
                                     ProgressDlgProc,
                                     (LPARAM)params);
    return result == IDOK;
}
