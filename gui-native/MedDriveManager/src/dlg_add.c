#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dlg_add.h"
#include "resource.h"
#include "store.h"
#include "ui/theme.h"
#include "ui/buttons.h"

static PrinterEntry       *s_entry;
static const ProfileEntry *s_profiles;
static int                 s_profileCount;

static void update_preview(HWND hwnd, int sel) {
    if (sel < 0 || sel >= s_profileCount) {
        SetDlgItemTextW(hwnd, IDC_LBL_PRV_PATH,       L"—");
        SetDlgItemTextW(hwnd, IDC_LBL_PRV_BASENAME,   L"—");
        SetDlgItemTextW(hwnd, IDC_LBL_PRV_STRATEGY,   L"—");
        SetDlgItemTextW(hwnd, IDC_LBL_PRV_OPEN_AFTER, L"—");
        return;
    }
    const ProfileEntry *p = &s_profiles[sel];
    SetDlgItemTextW(hwnd, IDC_LBL_PRV_PATH,       p->outputPath);
    SetDlgItemTextW(hwnd, IDC_LBL_PRV_BASENAME,   p->outputBaseName);
    SetDlgItemTextW(hwnd, IDC_LBL_PRV_STRATEGY,   p->overwriteFile    ? L"Sobrescrever" : L"Incrementar");
    SetDlgItemTextW(hwnd, IDC_LBL_PRV_OPEN_AFTER, p->openAfterGenerate ? L"Sim"          : L"Não");
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
        ? (sel ? CLR_ACCENT       : hot ? CLR_BTN_PRIMARY_HOV : CLR_BTN_PRIMARY)
        : (sel ? CLR_CARD         : hot ? CLR_BTN_SEC_HOV     : CLR_BTN_SECONDARY);

    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(dc, &rc, hbr);
    DeleteObject(hbr);

    wchar_t txt[64]; GetWindowTextW(dis->hwndItem, txt, 64);
    SetTextColor(dc, CLR_TEXT_PRIMARY);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontContent);
    DrawTextW(dc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG: {
        HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_PROFILE);
        for (int i = 0; i < s_profileCount; i++)
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)s_profiles[i].name);
        SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
        update_preview(hwnd, 0);

        make_btn_ownerdraw(hwnd, IDOK);
        make_btn_ownerdraw(hwnd, IDCANCEL);
        SendDlgItemMessageW(hwnd, IDC_EDIT_NAME, WM_SETFONT, (WPARAM)g_fontContent, TRUE);
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return (INT_PTR)g_hbrPrimary;

    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wp, CLR_TEXT_PRIMARY);
        SetBkColor((HDC)wp,   CLR_CARD);
        return (INT_PTR)g_hbrCard;

    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)wp, CLR_TEXT_PRIMARY);
        SetBkColor((HDC)wp,   CLR_CARD);
        return (INT_PTR)g_hbrCard;

    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)wp, CLR_TEXT_SECONDARY);
        SetBkMode((HDC)wp, TRANSPARENT);
        return (INT_PTR)g_hbrPrimary;

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)lp;
        draw_dlg_btn(di, di->CtlID == IDOK);
        return TRUE;
    }

    case WM_COMMAND:
        if (HIWORD(wp) == CBN_SELCHANGE && LOWORD(wp) == IDC_COMBO_PROFILE) {
            int sel = (int)SendDlgItemMessageW(hwnd, IDC_COMBO_PROFILE, CB_GETCURSEL, 0, 0);
            update_preview(hwnd, sel);
            return TRUE;
        }
        switch (LOWORD(wp)) {
        case IDOK: {
            int sel = (int)SendDlgItemMessageW(hwnd, IDC_COMBO_PROFILE, CB_GETCURSEL, 0, 0);
            if (sel < 0 || sel >= s_profileCount) {
                MessageBoxW(hwnd, L"Selecione um perfil.",
                            L"Campo obrigatório", MB_ICONWARNING | MB_OK);
                return TRUE;
            }
            GetDlgItemTextW(hwnd, IDC_EDIT_NAME, s_entry->name, PRINTER_NAME_MAX);
            if (s_entry->name[0] == L'\0') {
                MessageBoxW(hwnd, L"O nome da impressora não pode estar vazio.",
                            L"Campo obrigatório", MB_ICONWARNING | MB_OK);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_NAME));
                return TRUE;
            }
            wcsncpy_s(s_entry->portName,    PRINTER_PORT_MAX, s_profiles[sel].portName, _TRUNCATE);
            wcsncpy_s(s_entry->profileName, PRINTER_NAME_MAX, s_profiles[sel].name,     _TRUNCATE);
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL dlg_add_show(HWND parent, PrinterEntry *out,
                  const ProfileEntry *profiles, int profileCount) {
    s_entry        = out;
    s_profiles     = profiles;
    s_profileCount = profileCount;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    INT_PTR result  = DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_ADD_PRINTER), parent, DlgProc);
    return result == IDOK;
}
