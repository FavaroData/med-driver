#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>     /* SHBrowseForFolderW, SHGetPathFromIDListW */
#include <objbase.h>    /* CoTaskMemFree */
#include "dlg_add.h"
#include "resource.h"

static PrinterEntry *s_entry;

static void on_browse(HWND hwnd) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"Selecione a pasta de destino";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        SHGetPathFromIDListW(pidl, path);
        SetDlgItemTextW(hwnd, IDC_EDIT_PATH, path);
        CoTaskMemFree(pidl);
    }
}

static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
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
