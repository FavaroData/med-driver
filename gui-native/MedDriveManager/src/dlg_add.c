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
        SetDlgItemTextW(hwnd, IDC_EDIT_PATH, path);
        CoTaskMemFree(pidl);
    }
}

static const struct { int id; const wchar_t *label; const wchar_t *token; } TOKEN_ITEMS[] = {
    { 1001, L"Contador simples       {n}",         L"{n}"         },
    { 1002, L"Contador 3 dígitos     {nnn}",       L"{nnn}"       },
    { 1003, L"Data                   {data}",      L"{data}"      },
    { 1004, L"Hora                   {hora}",      L"{hora}"      },
    { 1005, L"Nome do documento      {documento}", L"{documento}" },
};
#define TOKEN_COUNT 5

static void on_insert_token(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    for (int i = 0; i < TOKEN_COUNT; i++)
        AppendMenuW(hMenu, MF_STRING, TOKEN_ITEMS[i].id, TOKEN_ITEMS[i].label);

    HWND hBtn = GetDlgItem(hwnd, IDC_BTN_TOKEN);
    RECT rc;
    GetWindowRect(hBtn, &rc);

    int id = (int)TrackPopupMenu(hMenu,
        TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
        rc.left, rc.bottom, 0, hwnd, NULL);
    DestroyMenu(hMenu);

    for (int i = 0; i < TOKEN_COUNT; i++) {
        if (TOKEN_ITEMS[i].id == id) {
            HWND hEdit = GetDlgItem(hwnd, IDC_EDIT_BASENAME);
            SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)TOKEN_ITEMS[i].token);
            SetFocus(hEdit);
            break;
        }
    }
}

static void update_preview(HWND hwnd) {
    wchar_t tmpl[PRINTER_BASENAME_MAX] = {0};
    GetDlgItemTextW(hwnd, IDC_EDIT_BASENAME, tmpl, PRINTER_BASENAME_MAX);

    if (tmpl[0] == L'\0') {
        SetDlgItemTextW(hwnd, IDC_LBL_PREVIEW, L"");
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t dateStr[16], timeStr[16];
    _snwprintf_s(dateStr, 16, _TRUNCATE, L"%04d-%02d-%02d",
                 st.wYear, st.wMonth, st.wDay);
    _snwprintf_s(timeStr, 16, _TRUNCATE, L"%02d-%02d-%02d",
                 st.wHour, st.wMinute, st.wSecond);

    wchar_t preview[MAX_PATH] = {0};
    int di = 0;
    const wchar_t *p = tmpl;
    struct { const wchar_t *tok; const wchar_t *val; } fixed[3] = {
        { L"{data}",      dateStr         },
        { L"{hora}",      timeStr         },
        { L"{documento}", L"MeuDocumento" },
    };

    while (*p && di < MAX_PATH - 1) {
        if (*p != L'{') { preview[di++] = *p++; continue; }

        BOOL matched = FALSE;
        for (int k = 0; k < 3 && !matched; k++) {
            int tlen = (int)wcslen(fixed[k].tok);
            if (wcsncmp(p, fixed[k].tok, tlen) == 0) {
                for (int j = 0; fixed[k].val[j] && di < MAX_PATH-1; j++)
                    preview[di++] = fixed[k].val[j];
                p += tlen;
                matched = TRUE;
            }
        }
        if (matched) continue;

        const wchar_t *q = p + 1;
        int nc = 0;
        while (*q == L'n') { nc++; q++; }
        if (nc > 0 && *q == L'}') {
            wchar_t nStr[16];
            if (nc == 1)
                _snwprintf_s(nStr, 16, _TRUNCATE, L"1");
            else
                _snwprintf_s(nStr, 16, _TRUNCATE, L"%0*d", nc, 1);
            for (int j = 0; nStr[j] && di < MAX_PATH-1; j++)
                preview[di++] = nStr[j];
            p = q + 1;
            continue;
        }
        preview[di++] = *p++;
    }
    preview[di] = 0;

    wchar_t previewLine[MAX_PATH + 16];
    _snwprintf_s(previewLine, MAX_PATH + 16, _TRUNCATE, L"Exemplo: %s.pdf", preview);
    SetDlgItemTextW(hwnd, IDC_LBL_PREVIEW, previewLine);
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
        update_preview(hwnd);
        return TRUE;

    case WM_COMMAND:
        if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == IDC_EDIT_NAME) {
            update_port_preview(hwnd);
            return TRUE;
        }
        if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == IDC_EDIT_BASENAME) {
            update_preview(hwnd);
            return TRUE;
        }
        switch (LOWORD(wp)) {
        case IDOK: {
            GetDlgItemTextW(hwnd, IDC_EDIT_PORT,     s_entry->portName,       PRINTER_PORT_MAX);
            GetDlgItemTextW(hwnd, IDC_EDIT_NAME,     s_entry->name,           PRINTER_NAME_MAX);
            GetDlgItemTextW(hwnd, IDC_EDIT_BASENAME, s_entry->outputBaseName, PRINTER_BASENAME_MAX);
            GetDlgItemTextW(hwnd, IDC_EDIT_PATH,     s_entry->outputPath,     PRINTER_PATH_MAX);

            if (s_entry->name[0] == L'\0') {
                MessageBoxW(hwnd, L"O nome da impressora não pode estar vazio.",
                            L"Campo obrigatório", MB_ICONWARNING | MB_OK);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_NAME));
                return TRUE;
            }
            if (s_entry->outputBaseName[0] == L'\0') {
                MessageBoxW(hwnd, L"O template do nome não pode estar vazio.",
                            L"Campo obrigatório", MB_ICONWARNING | MB_OK);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_BASENAME));
                return TRUE;
            }
            if (s_entry->outputPath[0] == L'\0') {
                MessageBoxW(hwnd, L"A pasta de destino não pode estar vazia.",
                            L"Campo obrigatório", MB_ICONWARNING | MB_OK);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_PATH));
                return TRUE;
            }
            /* Bloqueia caracteres inválidos fora de tokens */
            {
                static const wchar_t invalid[] = L"\\/:*?\"<>|";
                const wchar_t *c = s_entry->outputBaseName;
                while (*c) {
                    if (*c == L'{') {
                        while (*c && *c != L'}') c++; // pula o token inteiro
                        if (*c) c++;
                        continue;
                    }
                    if (wcschr(invalid, *c)) {
                        MessageBoxW(hwnd,
                            L"O template contém caracteres inválidos fora dos tokens.\r\n"
                            L"Não são permitidos: \\ / : * ? \" < > |",
                            L"Template inválido", MB_ICONWARNING | MB_OK);
                        SetFocus(GetDlgItem(hwnd, IDC_EDIT_BASENAME));
                        return TRUE;
                    }
                    c++;
                }
            }
            EndDialog(hwnd, IDOK);
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;

        case IDC_BTN_BROWSE:
            on_browse(hwnd);
            return TRUE;

        case IDC_BTN_TOKEN:
            on_insert_token(hwnd);
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
