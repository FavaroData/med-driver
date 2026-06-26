#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <winspool.h>
#include "mainwnd.h"
#include "dialogs/dlg_add.h"
#include "dialogs/dlg_progress.h"
#include "store.h"
#include "resource.h"
#include "ui/theme.h"
#include "ui/titlebar.h"
#include "ui/navbar.h"
#include "ui/listview.h"
#include "ui/statusbar.h"
#include "ui/buttons.h"
#include "settings/settings_tab.h"
#include "profiles/profiles_tab.h"

#define MAX_PRINTERS 512

/* ── Estado global ──────────────────────────────────────────────────────── */
static HWND g_hwndMain;
static int  g_activeTab = 0; /* 0=Perfis  1=Impressoras  2=Configurações */

/* Impressoras */
static HWND g_hwndList;
static HWND g_hwndBtnAdd;
static HWND g_hwndBtnRemove;
static HWND g_hwndBtnEditPrinter;
static HWND g_hwndBtnRefresh;
static PrinterEntry g_printers[MAX_PRINTERS];
static int  g_count = 0;

/* Status bar */
static HWND g_hwndStatus;
static BOOL s_navTrackingMouse = FALSE;

/* ── Helpers — Impressoras ──────────────────────────────────────────────── */
static void list_refresh(void) {
    ListView_DeleteAllItems(g_hwndList);
    for (int i = 0; i < g_count; i++) {
        LVITEMW lvi = {0};
        lvi.mask    = LVIF_TEXT;
        lvi.iItem   = i;
        lvi.pszText = g_printers[i].name;
        ListView_InsertItem(g_hwndList, &lvi);
        ListView_SetItemText(g_hwndList, i, 1, g_printers[i].profileName);
    }
    statusbar_set_text(g_hwndStatus, g_count);
    InvalidateRect(g_hwndMain, NULL, FALSE);
}

static void sync_with_system(void) {
    static const WCHAR PORT_PREFIX[] = L"Meddrive Printer PORT ";
    int prefixLen = (int)wcslen(PORT_PREFIX);

    DWORD needed = 0, returned = 0;
    EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &needed, &returned);

    int newCount = 0;
    PrinterEntry newPrinters[MAX_PRINTERS];
    memset(newPrinters, 0, sizeof(newPrinters));

    if (needed > 0) {
        BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, needed);
        if (buf) {
            if (EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, buf, needed, &needed, &returned)) {
                PRINTER_INFO_2W *info = (PRINTER_INFO_2W *)buf;
                for (DWORD i = 0; i < returned && newCount < MAX_PRINTERS; i++) {
                    if (!info[i].pDriverName) continue;
                    if (_wcsicmp(info[i].pDriverName, L"Meddrive Printer DRIVER") != 0) continue;

                    PrinterEntry *e = &newPrinters[newCount];
                    wcsncpy_s(e->name, PRINTER_NAME_MAX, info[i].pPrinterName, _TRUNCATE);
                    if (info[i].pPortName) {
                        wcsncpy_s(e->portName, PRINTER_PORT_MAX, info[i].pPortName, _TRUNCATE);
                        if (wcsncmp(info[i].pPortName, PORT_PREFIX, (size_t)prefixLen) == 0)
                            wcsncpy_s(e->profileName, PRINTER_NAME_MAX,
                                      info[i].pPortName + prefixLen, _TRUNCATE);
                        else
                            wcsncpy_s(e->profileName, PRINTER_NAME_MAX,
                                      info[i].pPortName, _TRUNCATE);
                    }
                    newCount++;
                }
            }
            HeapFree(GetProcessHeap(), 0, buf);
        }
    }

    g_count = newCount;
    memcpy(g_printers, newPrinters, (size_t)newCount * sizeof(PrinterEntry));
    list_refresh();
}

static void sync_and_notify(void) {
    sync_with_system();
    profiles_tab_update_printers(g_printers, g_count);
}

static void on_add(HWND hwnd) {
    if (g_count >= MAX_PRINTERS) return;

    wchar_t dllPath[MAX_PATH];
    GetSystemDirectoryW(dllPath, MAX_PATH);
    wcsncat_s(dllPath, MAX_PATH, L"\\meddrivemon.dll", _TRUNCATE);
    if (GetFileAttributesW(dllPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hwnd,
            L"meddrivemon.dll não encontrada em System32.\r\n"
            L"Execute o instalador principal antes de adicionar impressoras.",
            L"Pré-requisito ausente", MB_ICONERROR | MB_OK);
        return;
    }

    int profileCount;
    const ProfileEntry *profiles = profiles_tab_get(&profileCount);
    if (profileCount == 0) {
        MessageBoxW(hwnd,
            L"Nenhum perfil cadastrado.\r\n"
            L"Crie um perfil na aba PERFIS antes de adicionar uma impressora.",
            L"Sem perfis disponíveis", MB_ICONWARNING | MB_OK);
        return;
    }

    PrinterEntry entry = {0};
    if (!dlg_add_show(hwnd, &entry, profiles, profileCount, NULL)) return;
    if (!dlg_progress_run(hwnd, entry.name, entry.profileName)) return;
    sync_and_notify();
}

static void on_remove(HWND hwnd) {
    int sel = ListView_GetNextItem(g_hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= g_count) return;

    wchar_t confirm[512];
    _snwprintf_s(confirm, 512, _TRUNCATE,
        L"Remover a impressora \"%s\"?\r\n\r\n"
        L"A impressora, a porta e todas as configurações associadas\r\n"
        L"serão removidas do Windows.",
        g_printers[sel].name);
    if (MessageBoxW(hwnd, confirm, L"Confirmar remoção",
                    MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
        return;

    wchar_t name[PRINTER_NAME_MAX];
    wcsncpy_s(name, PRINTER_NAME_MAX, g_printers[sel].name, _TRUNCATE);
    if (!dlg_progress_remove(hwnd, name)) return;
    sync_and_notify();
}

static void on_edit_printer(HWND hwnd) {
    int sel = ListView_GetNextItem(g_hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= g_count) return;

    int profileCount;
    const ProfileEntry *profiles = profiles_tab_get(&profileCount);
    if (profileCount == 0) {
        MessageBoxW(hwnd,
            L"Nenhum perfil cadastrado.\r\n"
            L"Crie um perfil na aba PERFIS antes de editar.",
            L"Sem perfis disponíveis", MB_ICONWARNING | MB_OK);
        return;
    }

    wchar_t oldName[PRINTER_NAME_MAX];
    wcsncpy_s(oldName, PRINTER_NAME_MAX, g_printers[sel].name, _TRUNCATE);

    PrinterEntry entry = {0};
    if (!dlg_add_show(hwnd, &entry, profiles, profileCount, &g_printers[sel])) return;
    if (!dlg_progress_edit_printer(hwnd, oldName, entry.name, entry.profileName)) return;
    sync_and_notify();
}

/* ── Estado vazio da aba Impressoras ────────────────────────────────────── */
static void paint_empty_state(HDC dc, RECT rcContent) {
    int cx = (rcContent.left + rcContent.right)  / 2;
    int cy = (rcContent.top  + rcContent.bottom) / 2;

    if (g_icoPrinter48) {
        DrawIconEx(dc, cx - 48, cy - 80,
                   g_icoPrinter48, 48, 48, 0, NULL, DI_NORMAL);
    }

    SetTextColor(dc, CLR_TEXT_PRIMARY);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSubtitle);
    RECT rt1 = {rcContent.left, cy - 20, rcContent.right, cy + 20};
    DrawTextW(dc, L"Nenhuma impressora cadastrada", -1, &rt1,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);

    SetTextColor(dc, CLR_TEXT_SECONDARY);
    of = (HFONT)SelectObject(dc, g_fontContent);
    RECT rt2 = {rcContent.left, cy + 28, rcContent.right, cy + 52};
    DrawTextW(dc, L"Clique em 'Adicionar' para cadastrar uma nova impressora.", -1, &rt2,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

/* ── Troca de aba ───────────────────────────────────────────────────────── */
static void switch_tab(int tab) {
    g_activeTab = tab;
    g_hoverRow  = -1;

    BOOL isPerfis      = (tab == 0);
    BOOL isImpressoras = (tab == 1);
    BOOL isConfig      = (tab == 2);

    profiles_tab_show(isPerfis);

    ShowWindow(g_hwndList,            isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnAdd,          isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRemove,       isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnEditPrinter,  isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRefresh,      isImpressoras ? SW_SHOW : SW_HIDE);

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
            statusbar_set_text(g_hwndStatus, g_count);
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

    /* Status bar criado primeiro (profiles_tab_create precisa dele) */
    g_hwndStatus = statusbar_create(hwnd, hInst);
    statusbar_resize(g_hwndStatus, WIN_W, WIN_H);

    profiles_tab_create(hwnd, hInst, g_hwndStatus);

    /* ListView e botões de impressoras */
    int lvX = CONTENT_PAD;
    int lvW = WIN_W - CONTENT_PAD * 2;
    int printerBtnY = WIN_H - STATUSBAR_H - BTNBAR_H + (BTNBAR_H - BTN_H) / 2;
    int lvY = TITLEBAR_H + NAVBAR_H + SUBTITLE_H + 5;
    int lvH = WIN_H - lvY - BTNBAR_H - STATUSBAR_H - CONTENT_PAD;

    g_hwndList = listview_create(hwnd, hInst, lvX, lvY, lvW, lvH);

    g_hwndBtnAdd = buttons_create(hwnd, hInst, IDC_BTN_ADD,
                                   L"Adicionar", BTN_STYLE_PRIMARY,
                                   CONTENT_PAD, printerBtnY, BTN_W, BTN_H);
    g_hwndBtnRemove = buttons_create(hwnd, hInst, IDC_BTN_REMOVE,
                                      L"Remover", BTN_STYLE_SECONDARY,
                                      CONTENT_PAD + BTN_W + 8, printerBtnY, BTN_W, BTN_H);
    g_hwndBtnEditPrinter = buttons_create(hwnd, hInst, IDC_BTN_EDIT_PRINTER,
                                           L"Editar", BTN_STYLE_SECONDARY,
                                           CONTENT_PAD + (BTN_W + 8) * 2, printerBtnY, BTN_W, BTN_H);
    g_hwndBtnRefresh = buttons_create(hwnd, hInst, IDC_BTN_REFRESH,
                                       L"Atualizar", BTN_STYLE_SECONDARY,
                                       CONTENT_PAD + (BTN_W + 8) * 3, printerBtnY, BTN_W, BTN_H);

    settings_tab_create(hwnd, hInst);

    /* Carrega dados */
    sync_with_system();
    profiles_tab_update_printers(g_printers, g_count);
    profiles_tab_set_sync_cb(sync_and_notify);
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
    if (g_activeTab == 0 || g_activeTab == 1 || g_activeTab == 2) {
        const wchar_t *sub = (g_activeTab == 0) ? L"PERFIS DE IMPRESSÃO"
                           : (g_activeTab == 1) ? L"IMPRESSORAS CADASTRADAS"
                                                : L"CONFIGURAÇÕES";
        SetTextColor(dc, CLR_ACCENT);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
        RECT rcSub = {CONTENT_PAD, contentTop + 6,
                      w - CONTENT_PAD, contentTop + SUBTITLE_H};
        DrawTextW(dc, sub, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);
    }

    if (g_activeTab == 0)
        profiles_tab_paint(dc, w);

    if (g_activeTab == 1 && g_count == 0) {
        RECT rcEmp = {CONTENT_PAD, contentTop + CONTENT_PAD,
                      w - CONTENT_PAD, contentBot - CONTENT_PAD};
        paint_empty_state(dc, rcEmp);
    }

    if (g_activeTab == 2)
        settings_tab_paint(dc);

    /* Barra de botões (Impressoras / Configurações) */
    if (g_activeTab != 0) {
        int btnBarTop = WIN_H - STATUSBAR_H - BTNBAR_H;
        RECT rcBtnBar = {0, btnBarTop, w, btnBarTop + BTNBAR_H};
        FillRect(dc, &rcBtnBar, g_hbrSecondary);
        HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
        RECT rcSep = {0, btnBarTop, w, btnBarTop + 1};
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

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lp;
        if (mis->CtlID == IDC_PRINTER_LIST) {
            listview_measure(mis);
            return TRUE;
        }
        return FALSE;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (settings_tab_drawitem(dis)) return TRUE;
        if (profiles_tab_drawitem(dis))  return TRUE;
        if (dis->CtlID == IDC_PRINTER_LIST) {
            listview_draw_item(dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_BTN_TITLEMIN || dis->CtlID == IDC_BTN_TITLECLOSE) {
            titlebar_draw_button(dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_BTN_ADD)
            return buttons_draw(dis, BTN_STYLE_PRIMARY);
        if (dis->CtlID == IDC_BTN_REMOVE      ||
            dis->CtlID == IDC_BTN_REFRESH      ||
            dis->CtlID == IDC_BTN_EDIT_PRINTER)
            return buttons_draw(dis, BTN_STYLE_SECONDARY);
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
        if (settings_tab_command(LOWORD(wp))) return 0;
        if (profiles_tab_command(LOWORD(wp))) return 0;
        switch (LOWORD(wp)) {
        case IDC_BTN_ADD:           on_add(hwnd);                      break;
        case IDC_BTN_REMOVE:        on_remove(hwnd);                   break;
        case IDC_BTN_EDIT_PRINTER:  on_edit_printer(hwnd);             break;
        case IDC_BTN_REFRESH:       sync_and_notify();                 break;
        case IDC_BTN_TITLEMIN:      ShowWindow(hwnd, SW_MINIMIZE);     break;
        case IDC_BTN_TITLECLOSE:    DestroyWindow(hwnd);               break;
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
