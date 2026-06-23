#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include <objbase.h>
#include "dlg_profile.h"
#include "resource.h"
#include "ui/theme.h"
#include "ui/buttons.h"

typedef struct {
    ProfileEntry       *out;
    const ProfileEntry *prefill;
    const wchar_t      *title;
} ProfileDlgParams;

static ProfileEntry *s_entry;

static void on_browse(HWND hwnd) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"Selecione a pasta de destino";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        SHGetPathFromIDListW(pidl, path);
        SetDlgItemTextW(hwnd, IDC_EDIT_PROFILE_PATH, path);
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

    HWND hBtn = GetDlgItem(hwnd, IDC_BTN_PROFILE_TOKEN);
    RECT rc;
    GetWindowRect(hBtn, &rc);

    int id = (int)TrackPopupMenu(hMenu,
        TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
        rc.left, rc.bottom, 0, hwnd, NULL);
    DestroyMenu(hMenu);

    for (int i = 0; i < TOKEN_COUNT; i++) {
        if (TOKEN_ITEMS[i].id == id) {
            HWND hEdit = GetDlgItem(hwnd, IDC_EDIT_PROFILE_BASENAME);
            SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)TOKEN_ITEMS[i].token);
            SetFocus(hEdit);
            break;
        }
    }
}

static void update_preview(HWND hwnd) {
    wchar_t tmpl[PRINTER_BASENAME_MAX] = {0};
    GetDlgItemTextW(hwnd, IDC_EDIT_PROFILE_BASENAME, tmpl, PRINTER_BASENAME_MAX);

    if (tmpl[0] == L'\0') {
        SetDlgItemTextW(hwnd, IDC_LBL_PROFILE_PREVIEW, L"");
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
            if (nc == 1) _snwprintf_s(nStr, 16, _TRUNCATE, L"1");
            else         _snwprintf_s(nStr, 16, _TRUNCATE, L"%0*d", nc, 1);
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
    SetDlgItemTextW(hwnd, IDC_LBL_PROFILE_PREVIEW, previewLine);
}

static void make_btn_ownerdraw(HWND hwnd, int id) {
    HWND hBtn = GetDlgItem(hwnd, id);
    LONG_PTR s = GetWindowLongPtrW(hBtn, GWL_STYLE);
    SetWindowLongPtrW(hBtn, GWL_STYLE, (s & ~0xFL) | BS_OWNERDRAW);
    SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_fontContent, TRUE);
    buttons_install_hover(hBtn);
}

static void draw_dlg_btn(DRAWITEMSTRUCT *dis, BOOL isPrimary) {
    HDC  dc  = dis->hDC;
    RECT rc  = dis->rcItem;
    BOOL hot = (dis->itemState & ODS_HOTLIGHT) != 0
            || GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA) != 0;
    BOOL sel = (dis->itemState & ODS_SELECTED) != 0;

    COLORREF bg = isPrimary
        ? (sel ? CLR_ACCENT_HOVER  : hot ? CLR_BTN_PRIMARY_HOV : CLR_BTN_PRIMARY)
        : (sel ? CLR_BTN_SEC_HOV   : hot ? CLR_BTN_SEC_HOV     : CLR_BTN_SECONDARY);

    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(dc, &rc, hbr);
    DeleteObject(hbr);

    if (!isPrimary) {
        HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
        FrameRect(dc, &rc, hbrd);
        DeleteObject(hbrd);
    }

    wchar_t txt[64]; GetWindowTextW(dis->hwndItem, txt, 64);
    SetTextColor(dc, isPrimary ? RGB(255,255,255) : CLR_TEXT_PRIMARY);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontContent);
    DrawTextW(dc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        ProfileDlgParams *params = (ProfileDlgParams *)lp;
        s_entry = params->out;

        if (params->title)
            SetWindowTextW(hwnd, params->title);

        if (params->prefill) {
            SetDlgItemTextW(hwnd, IDC_EDIT_PROFILE_NAME,     params->prefill->name);
            SetDlgItemTextW(hwnd, IDC_EDIT_PROFILE_BASENAME, params->prefill->outputBaseName);
            SetDlgItemTextW(hwnd, IDC_EDIT_PROFILE_PATH,     params->prefill->outputPath);
            CheckDlgButton(hwnd, IDC_CHK_OPEN_AFTER,
                params->prefill->openAfterGenerate ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHK_OVERWRITE,
                params->prefill->overwriteFile ? BST_CHECKED : BST_UNCHECKED);
        }

        make_btn_ownerdraw(hwnd, IDOK);
        make_btn_ownerdraw(hwnd, IDCANCEL);
        make_btn_ownerdraw(hwnd, IDC_BTN_PROFILE_BROWSE);
        make_btn_ownerdraw(hwnd, IDC_BTN_PROFILE_TOKEN);
        update_preview(hwnd);
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return (INT_PTR)g_hbrPrimary;

    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wp, CLR_TEXT_PRIMARY);
        SetBkColor((HDC)wp,   CLR_CARD);
        return (INT_PTR)g_hbrCard;

    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)wp, CLR_TEXT_SECONDARY);
        SetBkMode((HDC)wp, TRANSPARENT);
        return (INT_PTR)g_hbrPrimary;

    case WM_CTLCOLORBTN:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (INT_PTR)g_hbrPrimary;

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)lp;
        BOOL isPrimary = (di->CtlID == IDOK || di->CtlID == IDC_BTN_PROFILE_BROWSE);
        draw_dlg_btn(di, isPrimary);
        return TRUE;
    }

    case WM_COMMAND:
        if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == IDC_EDIT_PROFILE_BASENAME) {
            update_preview(hwnd);
            return TRUE;
        }
        switch (LOWORD(wp)) {
        case IDOK: {
            GetDlgItemTextW(hwnd, IDC_EDIT_PROFILE_NAME,     s_entry->name,           PRINTER_NAME_MAX);
            GetDlgItemTextW(hwnd, IDC_EDIT_PROFILE_BASENAME, s_entry->outputBaseName, PRINTER_BASENAME_MAX);
            GetDlgItemTextW(hwnd, IDC_EDIT_PROFILE_PATH,     s_entry->outputPath,     PRINTER_PATH_MAX);
            s_entry->openAfterGenerate = (IsDlgButtonChecked(hwnd, IDC_CHK_OPEN_AFTER) == BST_CHECKED) ? 1 : 0;
            s_entry->overwriteFile     = (IsDlgButtonChecked(hwnd, IDC_CHK_OVERWRITE)  == BST_CHECKED) ? 1 : 0;

            if (s_entry->name[0] == L'\0') {
                MessageBoxW(hwnd, L"O nome do perfil não pode estar vazio.",
                            L"Campo obrigatório", MB_ICONWARNING | MB_OK);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_PROFILE_NAME));
                return TRUE;
            }
            if (wcschr(s_entry->name, L'\\')) {
                MessageBoxW(hwnd, L"O nome do perfil não pode conter '\\' (barra invertida).",
                            L"Nome inválido", MB_ICONWARNING | MB_OK);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_PROFILE_NAME));
                return TRUE;
            }
            if (s_entry->outputBaseName[0] == L'\0') {
                MessageBoxW(hwnd, L"O padrão do arquivo não pode estar vazio.",
                            L"Campo obrigatório", MB_ICONWARNING | MB_OK);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_PROFILE_BASENAME));
                return TRUE;
            }
            if (s_entry->outputPath[0] == L'\0') {
                MessageBoxW(hwnd, L"A pasta de destino não pode estar vazia.",
                            L"Campo obrigatório", MB_ICONWARNING | MB_OK);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_PROFILE_PATH));
                return TRUE;
            }
            /* Valida caracteres inválidos no padrão fora de tokens */
            {
                static const wchar_t invalid[] = L"\\/:*?\"<>|";
                const wchar_t *c = s_entry->outputBaseName;
                while (*c) {
                    if (*c == L'{') {
                        while (*c && *c != L'}') c++;
                        if (*c) c++;
                        continue;
                    }
                    if (wcschr(invalid, *c)) {
                        MessageBoxW(hwnd,
                            L"O padrão contém caracteres inválidos fora dos tokens.\r\n"
                            L"Não são permitidos: \\ / : * ? \" < > |",
                            L"Padrão inválido", MB_ICONWARNING | MB_OK);
                        SetFocus(GetDlgItem(hwnd, IDC_EDIT_PROFILE_BASENAME));
                        return TRUE;
                    }
                    c++;
                }
            }
            /* Deriva e armazena o nome completo da porta */
            _snwprintf_s(s_entry->portName, PRINTER_PORT_MAX, _TRUNCATE,
                         L"Meddrive Printer PORT %s", s_entry->name);
            EndDialog(hwnd, IDOK);
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;

        case IDC_BTN_PROFILE_BROWSE:
            on_browse(hwnd);
            return TRUE;

        case IDC_BTN_PROFILE_TOKEN:
            on_insert_token(hwnd);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL dlg_profile_show(HWND parent, ProfileEntry *out,
                      const ProfileEntry *prefill, const wchar_t *title) {
    ProfileDlgParams params = { out, prefill, title };
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result  = DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_ADD_PROFILE),
                                      parent, DlgProc, (LPARAM)&params);
    return result == IDOK;
}
