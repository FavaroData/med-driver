#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "buttons.h"
#include "theme.h"

/* ── Hover tracking ─────────────────────────────────────────────────────── */

static LRESULT CALLBACK BtnHoverProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                      UINT_PTR id, DWORD_PTR data) {
    (void)wp; (void)lp; (void)data;
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!GetWindowLongPtrW(hwnd, GWLP_USERDATA)) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 1);
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, BtnHoverProc, id);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void buttons_install_hover(HWND hwnd) {
    SetWindowSubclass(hwnd, BtnHoverProc, 1, 0);
}

/* ── Criação ─────────────────────────────────────────────────────────────── */

HWND buttons_create(HWND hwndParent, HINSTANCE hInst,
                    int id, const wchar_t *label, BtnStyle style,
                    int x, int y, int w, int h) {
    (void)style;
    HWND hwnd = CreateWindowW(L"BUTTON", label,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, w, h,
        hwndParent, (HMENU)(UINT_PTR)id, hInst, NULL);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_fontContent, TRUE);
    buttons_install_hover(hwnd);
    return hwnd;
}

BOOL buttons_draw(DRAWITEMSTRUCT *dis, BtnStyle style) {
    HDC  dc  = dis->hDC;
    RECT rc  = dis->rcItem;
    BOOL hot = (dis->itemState & ODS_HOTLIGHT) != 0
            || GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA) != 0;
    BOOL sel = (dis->itemState & ODS_SELECTED) != 0;
    BOOL dis_state = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF bg;
    if (dis_state)
        bg = CLR_BTN_SEC_HOV;
    else if (style == BTN_STYLE_PRIMARY)
        bg = sel ? CLR_ACCENT_HOVER : hot ? CLR_BTN_PRIMARY_HOV : CLR_BTN_PRIMARY;
    else
        bg = sel ? CLR_BTN_SEC_HOV : hot ? CLR_BTN_SEC_HOV : CLR_BTN_SECONDARY;

    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(dc, &rc, hbr);
    DeleteObject(hbr);

    /* Borda para botão secundário */
    if (style == BTN_STYLE_SECONDARY) {
        HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
        FrameRect(dc, &rc, hbrd);
        DeleteObject(hbrd);
    }

    /* Ícone (20px à esquerda do texto) */
    HICON ico = NULL;
    switch (dis->CtlID) {
    case 102: ico = g_icoAdd20;    break; /* IDC_BTN_ADD     */
    case 103: ico = g_icoDelete20; break; /* IDC_BTN_REMOVE  */
    case 105: ico = g_icoSync20;   break; /* IDC_BTN_REFRESH */
    }

    wchar_t txt[64] = {0};
    GetWindowTextW(dis->hwndItem, txt, 64);

    int totalW = 0;
    SIZE tsz = {0};
    HDC tmpDC = dc;
    HFONT of = (HFONT)SelectObject(tmpDC, g_fontContent);
    GetTextExtentPoint32W(tmpDC, txt, (int)wcslen(txt), &tsz);
    SelectObject(tmpDC, of);

    int iconW  = ico ? 20 : 0;
    int gap    = ico ? 6  : 0;
    totalW = iconW + gap + tsz.cx;
    int startX = rc.left + (rc.right - rc.left - totalW) / 2;
    int midY   = rc.top  + (rc.bottom - rc.top) / 2;

    if (ico)
        DrawIconEx(dc, startX, midY - 10, ico, 20, 20, 0, NULL, DI_NORMAL);

    COLORREF tc = dis_state    ? CLR_TEXT_DISABLED
                : style == BTN_STYLE_PRIMARY ? RGB(255, 255, 255)
                :                              CLR_TEXT_PRIMARY;
    SetTextColor(dc, tc);
    SetBkMode(dc, TRANSPARENT);
    of = (HFONT)SelectObject(dc, g_fontContent);
    RECT rcTxt = {startX + iconW + gap, rc.top, rc.right, rc.bottom};
    DrawTextW(dc, txt, -1, &rcTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);

    if (dis->itemState & ODS_FOCUS) {
        RECT rf = rc; InflateRect(&rf, -2, -2);
        DrawFocusRect(dc, &rf);
    }
    return TRUE;
}
