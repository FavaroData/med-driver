#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "titlebar.h"
#include "theme.h"
#include "buttons.h"
#include "resource.h"

void titlebar_create_buttons(HWND hwndParent, HINSTANCE hInst) {
    int btnY = (TITLEBAR_H - TITLEBTN_W) / 2;
    int btnX = WIN_W - TITLEBTN_W * 2;

    HWND hMin = CreateWindowW(L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        btnX, btnY, TITLEBTN_W, TITLEBTN_W,
        hwndParent, (HMENU)(UINT_PTR)IDC_BTN_TITLEMIN, hInst, NULL);
    buttons_install_hover(hMin);

    HWND hClose = CreateWindowW(L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        btnX + TITLEBTN_W, btnY, TITLEBTN_W, TITLEBTN_W,
        hwndParent, (HMENU)(UINT_PTR)IDC_BTN_TITLECLOSE, hInst, NULL);
    buttons_install_hover(hClose);
}

void titlebar_draw_button(DRAWITEMSTRUCT *dis) {
    BOOL isClose = (dis->CtlID == IDC_BTN_TITLECLOSE);
    HDC  dc  = dis->hDC;
    RECT rc  = dis->rcItem;
    BOOL hot = (dis->itemState & ODS_HOTLIGHT) != 0
            || GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA) != 0;
    BOOL sel = (dis->itemState & ODS_SELECTED) != 0;

    COLORREF bg;
    if (isClose)
        bg = sel ? CLR_CLOSE_PRS : hot ? CLR_CLOSE_HOV : CLR_BG_PRIMARY;
    else
        bg = sel ? CLR_CARD      : hot ? CLR_TITLEBTN_HOV : CLR_BG_PRIMARY;

    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(dc, &rc, hbr);
    DeleteObject(hbr);

    /* × para fechar, − para minimizar */
    const wchar_t *sym = isClose ? L"\xD7" : L"\x2212";
    SetTextColor(dc, CLR_TEXT_PRIMARY);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSubtitle);
    DrawTextW(dc, sym, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

void titlebar_paint(HDC dc, int clientW) {
    RECT rcBar = {0, 0, clientW, TITLEBAR_H};
    HBRUSH hbr = CreateSolidBrush(CLR_BG_PRIMARY);
    FillRect(dc, &rcBar, hbr);
    DeleteObject(hbr);

    HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
    RECT rcLine = {0, TITLEBAR_H - 1, clientW, TITLEBAR_H};
    FillRect(dc, &rcLine, hbrd);
    DeleteObject(hbrd);

    if (g_icoPrinter20) {
        int iconY = (TITLEBAR_H - 24) / 2;
        DrawIconEx(dc, 20, iconY, g_icoPrinter20, 24, 24, 0, NULL, DI_NORMAL);
    }

    SetTextColor(dc, CLR_TEXT_PRIMARY);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontTitle);
    RECT rcTxt = {54, 0, clientW - TITLEBTN_W * 2, TITLEBAR_H};
    DrawTextW(dc, L"Meddrive Printer Manager", -1, &rcTxt,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

LRESULT titlebar_nchittest(HWND hwnd, int xScreen, int yScreen) {
    POINT pt = {xScreen, yScreen};
    ScreenToClient(hwnd, &pt);

    if (pt.y < 0 || pt.y >= TITLEBAR_H) return HTCLIENT;

    int btnAreaX = WIN_W - TITLEBTN_W * 2;
    if (pt.x >= btnAreaX) return HTCLIENT;

    return HTCAPTION;
}
