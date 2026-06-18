#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <winspool.h>
#include "mainwnd.h"
#include "dlg_add.h"
#include "dlg_progress.h"
#include "store.h"
#include "resource.h"
#include "ui/theme.h"
#include "ui/titlebar.h"
#include "ui/navbar.h"
#include "ui/listview.h"
#include "ui/statusbar.h"
#include "ui/buttons.h"

#define MAX_PRINTERS 512

/* ── Estado global ──────────────────────────────────────────────────── */
static HWND g_hwndMain;
static HWND g_hwndList;
static HWND g_hwndBtnAdd;
static HWND g_hwndBtnRemove;
static HWND g_hwndBtnRefresh;
static HWND g_hwndStatus;
static int  g_activeTab = 0;
static PrinterEntry g_printers[MAX_PRINTERS];
static int  g_count = 0;

/* ── Helpers de negócio (preservados integralmente) ──────────────────── */
static void list_refresh(void) {
    ListView_DeleteAllItems(g_hwndList);
    for (int i = 0; i < g_count; i++) {
        LVITEMW lvi = {0};
        lvi.mask    = LVIF_TEXT;
        lvi.iItem   = i;
        lvi.pszText = g_printers[i].portName;
        ListView_InsertItem(g_hwndList, &lvi);

        ListView_SetItemText(g_hwndList, i, 1, g_printers[i].name);

        wchar_t filePattern[PRINTER_BASENAME_MAX + 8] = {0};
        if (g_printers[i].outputBaseName[0])
            _snwprintf_s(filePattern, PRINTER_BASENAME_MAX + 8, _TRUNCATE,
                         L"%s.pdf", g_printers[i].outputBaseName);
        ListView_SetItemText(g_hwndList, i, 2, filePattern);
        ListView_SetItemText(g_hwndList, i, 3, g_printers[i].outputPath);
    }
    statusbar_set_text(g_hwndStatus, g_count);
    InvalidateRect(g_hwndMain, NULL, FALSE);
}

static void switch_tab(int tab) {
    g_activeTab = tab;
    BOOL imp = (tab == 0);
    ShowWindow(g_hwndList,       imp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnAdd,     imp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRemove,  imp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRefresh, imp ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_hwndMain, NULL, TRUE);
}

static void sync_with_system(void) {
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
                    wcsncpy_s(e->name,     PRINTER_NAME_MAX, info[i].pPrinterName, _TRUNCATE);
                    if (info[i].pPortName)
                        wcsncpy_s(e->portName, PRINTER_PORT_MAX, info[i].pPortName, _TRUNCATE);

                    if (info[i].pPortName) {
                        wchar_t regKey[512];
                        _snwprintf_s(regKey, 512, _TRUNCATE,
                            L"SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\"
                            L"Meddrive Printer MONITOR\\Ports\\%s",
                            info[i].pPortName);
                        HKEY hKey;
                        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regKey, 0,
                                          KEY_READ, &hKey) == ERROR_SUCCESS) {
                            DWORD type, sz;
                            sz = PRINTER_PATH_MAX * sizeof(wchar_t);
                            RegQueryValueExW(hKey, L"OutputPath", NULL, &type,
                                             (BYTE *)e->outputPath, &sz);
                            sz = PRINTER_BASENAME_MAX * sizeof(wchar_t);
                            RegQueryValueExW(hKey, L"OutputBaseName", NULL, &type,
                                             (BYTE *)e->outputBaseName, &sz);
                            RegCloseKey(hKey);
                        }
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
    store_save(g_printers, g_count);
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

    PrinterEntry entry = {0};
    if (!dlg_add_show(hwnd, &entry)) return;
    if (!dlg_progress_run(hwnd, entry.name, entry.outputPath, entry.outputBaseName)) return;
    sync_with_system();
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
    sync_with_system();
}

/* ── Estado vazio ────────────────────────────────────────────────────── */
static void paint_empty_state(HDC dc, RECT rcContent) {
    int cx = (rcContent.left + rcContent.right)  / 2;
    int cy = (rcContent.top  + rcContent.bottom) / 2;

    /* Ícone 48px */
    if (g_icoPrinter48) {
        DrawIconEx(dc, cx - 48, cy - 80,
                   g_icoPrinter48, 48, 48, 0, NULL, DI_NORMAL);
    }

    /* Texto principal */
    SetTextColor(dc, CLR_TEXT_PRIMARY);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSubtitle);
    RECT rt1 = {rcContent.left, cy - 20, rcContent.right, cy + 20};
    DrawTextW(dc, L"Nenhuma impressora cadastrada", -1, &rt1,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);

    /* Texto secundário */
    SetTextColor(dc, CLR_TEXT_SECONDARY);
    of = (HFONT)SelectObject(dc, g_fontContent);
    RECT rt2 = {rcContent.left, cy + 28, rcContent.right, cy + 52};
    DrawTextW(dc, L"Clique em 'Adicionar' para cadastrar uma nova impressora.", -1, &rt2,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

/* ── WM_CREATE ───────────────────────────────────────────────────────── */
static void on_create(HWND hwnd) {
    g_hwndMain = hwnd;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

    theme_init(hInst);

    /* Ícone da janela (taskbar) */
    SendMessageW(hwnd, WM_SETICON, ICON_BIG,
        (LPARAM)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_ICO_APP),
                           IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
        (LPARAM)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_ICO_APP),
                           IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

    /* Title bar — botões Min/Close */
    titlebar_create_buttons(hwnd, hInst);

    /* ListView — área de conteúdo */
    int lvX = CONTENT_PAD;
    int lvY = TITLEBAR_H + NAVBAR_H + CONTENT_PAD;
    int lvW = WIN_W - CONTENT_PAD * 2;
    int lvH = WIN_H - lvY - BTNBAR_H - STATUSBAR_H - CONTENT_PAD;
    g_hwndList = listview_create(hwnd, hInst, lvX, lvY, lvW, lvH);

    /* Botões de ação */
    int btnY = WIN_H - STATUSBAR_H - BTNBAR_H + (BTNBAR_H - BTN_H) / 2;
    g_hwndBtnAdd = buttons_create(hwnd, hInst, IDC_BTN_ADD,
                                  L"Adicionar", BTN_STYLE_PRIMARY,
                                  CONTENT_PAD, btnY, BTN_W, BTN_H);
    g_hwndBtnRemove = buttons_create(hwnd, hInst, IDC_BTN_REMOVE,
                                     L"Remover", BTN_STYLE_SECONDARY,
                                     CONTENT_PAD + BTN_W + 8, btnY, BTN_W, BTN_H);
    g_hwndBtnRefresh = buttons_create(hwnd, hInst, IDC_BTN_REFRESH,
                                      L"Atualizar", BTN_STYLE_SECONDARY,
                                      CONTENT_PAD + (BTN_W + 8) * 2, btnY, BTN_W, BTN_H);

    /* Status bar customizada */
    g_hwndStatus = statusbar_create(hwnd, hInst);
    statusbar_resize(g_hwndStatus, WIN_W, WIN_H);

    /* Dados */
    PrinterEntry *loaded = NULL;
    int n = store_load(&loaded);
    g_count = (n > MAX_PRINTERS) ? MAX_PRINTERS : n;
    if (g_count > 0)
        memcpy(g_printers, loaded, (size_t)g_count * sizeof(PrinterEntry));
    store_free(loaded);
    list_refresh();
}

/* ── WM_PAINT ────────────────────────────────────────────────────────── */
static void on_paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right;

    titlebar_paint(dc, w);
    navbar_paint(dc, w, g_activeTab);

    /* Fundo da área de conteúdo */
    int contentTop = TITLEBAR_H + NAVBAR_H;
    int contentBot  = WIN_H - STATUSBAR_H - BTNBAR_H;
    RECT rcContent = {0, contentTop, w, contentBot};
    FillRect(dc, &rcContent, g_hbrPrimary);

    /* Estado vazio */
    if (g_activeTab == 0 && g_count == 0) {
        RECT rcEmp = {CONTENT_PAD, contentTop + CONTENT_PAD,
                      w - CONTENT_PAD, contentBot - CONTENT_PAD};
        paint_empty_state(dc, rcEmp);
    }

    /* Aba Configurações */
    if (g_activeTab == 1) {
        SetTextColor(dc, CLR_TEXT_DISABLED);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_fontSubtitle);
        DrawTextW(dc, L"Configurações — em breve.", -1, &rcContent,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);
    }

    /* Barra de botões */
    int btnBarTop = WIN_H - STATUSBAR_H - BTNBAR_H;
    RECT rcBtnBar = {0, btnBarTop, w, btnBarTop + BTNBAR_H};
    FillRect(dc, &rcBtnBar, g_hbrSecondary);

    /* Separador superior da barra de botões */
    HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
    RECT rcSep = {0, btnBarTop, w, btnBarTop + 1};
    FillRect(dc, &rcSep, hbrd);
    DeleteObject(hbrd);

    EndPaint(hwnd, &ps);
}

/* ── WndProc ─────────────────────────────────────────────────────────── */
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
        if (dis->CtlID == IDC_BTN_REMOVE || dis->CtlID == IDC_BTN_REFRESH)
            return buttons_draw(dis, BTN_STYLE_SECONDARY);
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_ADD:           on_add(hwnd);       break;
        case IDC_BTN_REMOVE:        on_remove(hwnd);    break;
        case IDC_BTN_REFRESH:       sync_with_system(); break;
        case IDC_BTN_TITLEMIN:      ShowWindow(hwnd, SW_MINIMIZE); break;
        case IDC_BTN_TITLECLOSE:    DestroyWindow(hwnd); break;
        }
        return 0;

    case WM_DESTROY:
        theme_destroy();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ── Registro e criação (Q1–Q3, Q8, Q9) ─────────────────────────────── */
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

    /* Q1/Q2: WS_POPUP — sem decoração nativa. Q4: sem Maximizar. */
    HWND hwnd = CreateWindowExW(
        0,
        WC_MAINWND,
        L"Meddrive Printer Manager",
        WS_POPUP | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        x, y, WIN_W, WIN_H,
        NULL, NULL, hInst, NULL);

    if (!hwnd) return NULL;

    /* Q8: sombra DWM */
    DWORD policy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                          &policy, sizeof(policy));

    /* Margins = 0 para a sombra aparecer numa janela WS_POPUP */
    MARGINS m = {0, 0, 0, 1};
    DwmExtendFrameIntoClientArea(hwnd, &m);

    return hwnd;
}
