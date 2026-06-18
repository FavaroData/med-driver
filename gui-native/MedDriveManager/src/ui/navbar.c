#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "navbar.h"
#include "theme.h"

#define NAV_COUNT 2

static const wchar_t *NAV_LABELS[NAV_COUNT] = {
    L"IMPRESSORAS",
    L"CONFIGURAÇÕES",
};

void navbar_paint(HDC dc, int clientW, int activeTab) {
    RECT rcBar = {0, TITLEBAR_H, clientW, TITLEBAR_H + NAVBAR_H};
    HBRUSH hbr = CreateSolidBrush(CLR_BG_SECONDARY);
    FillRect(dc, &rcBar, hbr);
    DeleteObject(hbr);

    for (int i = 0; i < NAV_COUNT; i++) {
        RECT rt = {i * NAV_TAB_W, TITLEBAR_H,
                   (i + 1) * NAV_TAB_W, TITLEBAR_H + NAVBAR_H};
        BOOL active = (i == activeTab);

        if (active) {
            HBRUSH ha = CreateSolidBrush(CLR_CARD);
            FillRect(dc, &rt, ha);
            DeleteObject(ha);
        }

        /* Ícone (20px centrado verticalmente) */
        HICON ico = (i == 0) ? g_icoPrinter20 : g_icoSettings20;
        if (ico) {
            int iconX = rt.left + 14;
            int iconY = TITLEBAR_H + (NAVBAR_H - 20) / 2;
            DrawIconEx(dc, iconX, iconY, ico, 20, 20, 0, NULL, DI_NORMAL);
        }

        /* Texto */
        SetTextColor(dc, active ? CLR_TEXT_PRIMARY : CLR_TEXT_SECONDARY);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
        RECT rtxt = {rt.left + 38, rt.top, rt.right - 4, rt.bottom};
        DrawTextW(dc, NAV_LABELS[i], -1, &rtxt,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);

        /* Linha azul inferior na aba ativa */
        if (active) {
            HBRUSH hbl = CreateSolidBrush(CLR_ACCENT);
            RECT rl = {rt.left, rt.bottom - 2, rt.right, rt.bottom};
            FillRect(dc, &rl, hbl);
            DeleteObject(hbl);
        }
    }

    /* Linha inferior da navbar */
    HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
    RECT rcLine = {0, TITLEBAR_H + NAVBAR_H - 1, clientW, TITLEBAR_H + NAVBAR_H};
    FillRect(dc, &rcLine, hbrd);
    DeleteObject(hbrd);
}

int navbar_hittest(int x, int y) {
    if (y < TITLEBAR_H || y >= TITLEBAR_H + NAVBAR_H) return -1;
    int tab = x / NAV_TAB_W;
    if (tab < 0 || tab >= NAV_COUNT) return -1;
    return tab;
}
