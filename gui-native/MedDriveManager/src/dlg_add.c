#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include <objbase.h>
#include <wctype.h>
#include "dlg_add.h"
#include "resource.h"

static PrinterEntry *s_entry;

/* Mirrors PS: $PrinterName -replace 'Meddrive Printer','' -replace '-','' -replace '\s','' */
void derive_port_name(const wchar_t *printerName, wchar_t *portName, int cchPort) {
    int nameLen = (int)wcslen(printerName);

    /* lowercase copy for case-insensitive search */
    wchar_t lower[256] = {0};
    for (int i = 0; i < nameLen && i < 255; i++)
        lower[i] = (wchar_t)towlower(printerName[i]);

    /* remove "meddrive printer" from the original string */
    const wchar_t *prefix = L"meddrive printer";
    wchar_t *found = wcsstr(lower, prefix);
    wchar_t tmp[256] = {0};
    if (found) {
        int start  = (int)(found - lower);
        int pfxLen = (int)wcslen(prefix);
        wcsncpy_s(tmp, 256, printerName, (size_t)start);
        wcsncat_s(tmp, 256, printerName + start + pfxLen, _TRUNCATE);
    } else {
        wcsncpy_s(tmp, 256, printerName, _TRUNCATE);
    }

    /* remove '-' and whitespace */
    wchar_t suffix[256] = {0};
    int si = 0;
    for (int i = 0; tmp[i] && si < 255; i++) {
        if (tmp[i] != L'-' && !iswspace(tmp[i]))
            suffix[si++] = tmp[i];
    }
    suffix[si] = 0;

    if (suffix[0])
        _snwprintf_s(portName, cchPort, _TRUNCATE, L"Meddrive Printer PORT - %s", suffix);
    else
        wcsncpy_s(portName, cchPort, L"Meddrive Printer PORT", _TRUNCATE);
}

static void on_browse(HWND hwnd) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"Selecione a pasta de destino";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        SHGetPathFromIDListW(pidl, path);
        wcsncat_s(path, MAX_PATH, L"\\saida.pdf", _TRUNCATE);
        SetDlgItemTextW(hwnd, IDC_EDIT_PATH, path);
        CoTaskMemFree(pidl);
    }
}

static void update_port_preview(HWND hwnd) {
    wchar_t name[PRINTER_NAME_MAX] = {0};
    GetDlgItemTextW(hwnd, IDC_EDIT_NAME, name, PRINTER_NAME_MAX);
    wchar_t port[PRINTER_PORT_MAX] = {0};
    derive_port_name(name, port, PRINTER_PORT_MAX);
    SetDlgItemTextW(hwnd, IDC_EDIT_PORT, port);
}

static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        SendDlgItemMessageW(hwnd, IDC_EDIT_PORT, EM_SETREADONLY, TRUE, 0);
        update_port_preview(hwnd);
        return TRUE;

    case WM_COMMAND:
        if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == IDC_EDIT_NAME) {
            update_port_preview(hwnd);
            return TRUE;
        }
        switch (LOWORD(wp)) {
        case IDOK:
            GetDlgItemTextW(hwnd, IDC_EDIT_NAME, s_entry->name,       PRINTER_NAME_MAX);
            GetDlgItemTextW(hwnd, IDC_EDIT_PORT, s_entry->portName,   PRINTER_PORT_MAX);
            GetDlgItemTextW(hwnd, IDC_EDIT_PATH, s_entry->outputPath, PRINTER_PATH_MAX);
            if (s_entry->name[0] == L'\0') {
                MessageBoxW(hwnd, L"O nome da impressora não pode estar vazio.",
                            L"Campo obrigatório", MB_ICONWARNING | MB_OK);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_NAME));
                return TRUE;
            }
            EndDialog(hwnd, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;

        case IDC_BTN_BROWSE:
            on_browse(hwnd);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL dlg_add_show(HWND parent, PrinterEntry *out) {
    s_entry = out;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result  = DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_ADD_PRINTER), parent, DlgProc);
    return result == IDOK;
}
