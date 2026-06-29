#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <stdio.h>
#include "mainwnd.h"
#include "resource.h"
#include "ui/theme.h"
#include "ui/titlebar.h"
#include "ui/navbar.h"
#include "ui/statusbar.h"
#include "ui/listview.h"
#include "settings/settings_tab.h"
#include "profiles/profiles_tab.h"
#include "printers/printers_tab.h"

/* ── Estado global ──────────────────────────────────────────────────────── */
static HWND g_hwndMain;
static int  g_activeTab = 0; /* 0=Perfis  1=Impressoras  2=Configurações */
static HWND g_hwndStatus;
static BOOL s_navTrackingMouse = FALSE;

/* ── Cabeamento entre tabs ──────────────────────────────────────────────── */

static void on_printers_changed(void) {
    int n;
    const PrinterEntry *p = printers_tab_get(&n);
    profiles_tab_update_printers(p, n);
}

/* ── Troca de aba ───────────────────────────────────────────────────────── */
static void switch_tab(int tab) {
    g_activeTab = tab;
    g_hoverRow  = -1;

    BOOL isPerfis      = (tab == 0);
    BOOL isImpressoras = (tab == 1);
    BOOL isConfig      = (tab == 2);

    profiles_tab_show(isPerfis);
    printers_tab_show(isImpressoras);
    settings_tab_show(isConfig);
    if (isConfig) settings_tab_load();

    if (g_hwndStatus) {
        if (isPerfis) {
            int n;
            profiles_tab_get(&n);
            wchar_t t[128];
            if (n == 1)
                _snwprintf_s(t, 128, _TRUNCATE, L"1 perfil cadastrado");
            else
                _snwprintf_s(t, 128, _TRUNCATE, L"%d perfis cadastrados", n);
            statusbar_set_text_raw(g_hwndStatus, t);
        } else if (isConfig) {
            statusbar_set_text_raw(g_hwndStatus, L"Configurações do sistema");
        } else {
            int n;
            printers_tab_get(&n);
            statusbar_set_text(g_hwndStatus, n);
        }
    }

    InvalidateRect(g_hwndMain, NULL, TRUE);
}

/* ── WM_CREATE ──────────────────────────────────────────────────────────── */
static void on_create(HWND hwnd) {
    g_hwndMain = hwnd;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

    theme_init(hInst);

    SendMessageW(hwnd, WM_SETICON, ICON_BIG,
        (LPARAM)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_ICO_APP),
                           IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
        (LPARAM)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_ICO_APP),
                           IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

    titlebar_create_buttons(hwnd, hInst);

    g_hwndStatus = statusbar_create(hwnd, hInst);
    statusbar_resize(g_hwndStatus, WIN_W, WIN_H);

    profiles_tab_create(hwnd, hInst, g_hwndStatus);
    printers_tab_create(hwnd, hInst, g_hwndStatus);
    settings_tab_create(hwnd, hInst);

    /* Cabeamento cruzado */
    printers_tab_set_on_change(on_printers_changed);

    /* Carga inicial */
    printers_tab_sync();

    profiles_tab_load();

    switch_tab(0);
}

/* ── WM_PAINT ───────────────────────────────────────────────────────────── */
static void on_paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right;

    titlebar_paint(dc, w);
    navbar_paint(dc, w, g_activeTab);

    int contentTop = TITLEBAR_H + NAVBAR_H;
    int contentBot = (g_activeTab == 0)
                   ? WIN_H - STATUSBAR_H
                   : WIN_H - STATUSBAR_H - BTNBAR_H;
    RECT rcContent = {0, contentTop, w, contentBot};
    FillRect(dc, &rcContent, g_hbrPrimary);

    /* Cabeçalho de seção */
    const wchar_t *sub = (g_activeTab == 0) ? L"PERFIS DE IMPRESSÃO"
                       : (g_activeTab == 1) ? L"IMPRESSORAS CADASTRADAS"
                                            : L"CONFIGURAÇÕES";
    SetTextColor(dc, CLR_ACCENT);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
    RECT rcSub = {CONTENT_PAD, contentTop + 6, w - CONTENT_PAD, contentTop + SUBTITLE_H};
    DrawTextW(dc, sub, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);

    if (g_activeTab == 0)
        profiles_tab_paint(dc, w);

    if (g_activeTab == 1) {
        RECT rcEmp = {CONTENT_PAD, contentTop + CONTENT_PAD,
                      w - CONTENT_PAD, contentBot - CONTENT_PAD};
        printers_tab_paint(dc, rcEmp);
    }

    if (g_activeTab == 2)
        settings_tab_paint(dc);

    /* Barra de botões (Impressoras / Configurações) */
    if (g_activeTab != 0) {
        int btnBarTop = WIN_H - STATUSBAR_H - BTNBAR_H;
        RECT rcBtnBar = {0, btnBarTop, w, btnBarTop + BTNBAR_H};
        FillRect(dc, &rcBtnBar, g_hbrSecondary);
        HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
        RECT rcSep  = {0, btnBarTop, w, btnBarTop + 1};
        FillRect(dc, &rcSep, hbrd);
        DeleteObject(hbrd);
    }

    /* Borda da janela (1px) */
    HBRUSH hbWnd = CreateSolidBrush(CLR_BORDER);
    FrameRect(dc, &rc, hbWnd);
    DeleteObject(hbWnd);

    EndPaint(hwnd, &ps);
}

/* ── WndProc ────────────────────────────────────────────────────────────── */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        on_create(hwnd);
        return 0;

    case WM_PAINT:
        on_paint(hwnd);
        return 0;

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_hbrPrimary);
        return 1;
    }

    case WM_NCHITTEST: {
        LRESULT def = DefWindowProcW(hwnd, msg, wp, lp);
        if (def == HTCLIENT) {
            LRESULT tb = titlebar_nchittest(hwnd,
                                            (int)(short)LOWORD(lp),
                                            (int)(short)HIWORD(lp));
            if (tb != HTCLIENT) return tb;
        }
        return def;
    }

    case WM_LBUTTONDOWN: {
        int x = (int)(short)LOWORD(lp);
        int y = (int)(short)HIWORD(lp);
        int tab = navbar_hittest(x, y);
        if (tab >= 0 && tab != g_activeTab)
            switch_tab(tab);
        return 0;
    }

    case WM_MEASUREITEM:
        return printers_tab_measure((MEASUREITEMSTRUCT *)lp);

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (settings_tab_drawitem(dis)) return TRUE;
        if (profiles_tab_drawitem(dis))  return TRUE;
        if (printers_tab_drawitem(dis))  return TRUE;
        if (dis->CtlID == IDC_BTN_TITLEMIN || dis->CtlID == IDC_BTN_TITLECLOSE) {
            titlebar_draw_button(dis);
            return TRUE;
        }
        return FALSE;
    }

    case WM_CTLCOLORSTATIC: {
        LRESULT br = settings_tab_ctlcolor((HWND)lp, (HDC)wp);
        if (br) return br;
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_COMBO_PROFILE_SEL && HIWORD(wp) == CBN_SELCHANGE) {
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (LOWORD(wp) == IDC_BTN_REFRESH)
            profiles_tab_load();
        if (settings_tab_command(LOWORD(wp))) return 0;
        if (profiles_tab_command(LOWORD(wp))) return 0;
        if (printers_tab_command(LOWORD(wp))) return 0;
        switch (LOWORD(wp)) {
        case IDC_BTN_TITLEMIN:   ShowWindow(hwnd, SW_MINIMIZE); break;
        case IDC_BTN_TITLECLOSE: DestroyWindow(hwnd);           break;
        }
        return 0;

    case WM_MOUSEMOVE: {
        int x = (int)(short)LOWORD(lp);
        int y = (int)(short)HIWORD(lp);
        if (navbar_set_hover(navbar_hittest(x, y))) {
            RECT rcNav = {0, TITLEBAR_H, WIN_W, TITLEBAR_H + NAVBAR_H};
            InvalidateRect(hwnd, &rcNav, FALSE);
        }
        if (!s_navTrackingMouse) {
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            s_navTrackingMouse = TRUE;
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        s_navTrackingMouse = FALSE;
        if (navbar_set_hover(-1)) {
            RECT rcNav = {0, TITLEBAR_H, WIN_W, TITLEBAR_H + NAVBAR_H};
            InvalidateRect(hwnd, &rcNav, FALSE);
        }
        return 0;

    case WM_APP_PROFILES_CHANGED: {
        printers_tab_sync();
        return 0;
    }

    case WM_DESTROY:
        theme_destroy();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ── Registro e criação ─────────────────────────────────────────────────── */
BOOL mainwnd_register(HINSTANCE hInst) {
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = WC_MAINWND;
    return RegisterClassExW(&wc) != 0;
}

HWND mainwnd_create(HINSTANCE hInst) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x  = (sw - WIN_W) / 2;
    int y  = (sh - WIN_H) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        WC_MAINWND,
        L"Meddrive Printer Manager",
        WS_POPUP | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        x, y, WIN_W, WIN_H,
        NULL, NULL, hInst, NULL);

    if (!hwnd) return NULL;

    DWORD policy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                          &policy, sizeof(policy));

    MARGINS m = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &m);

    return hwnd;
}
