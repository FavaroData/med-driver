#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "mainwnd.h"
#include "dlg_add.h"
#include "store.h"
#include "resource.h"

/* ── Cores ─────────────────────────────────────────────────────────── */
#define CLR_BG            RGB(0x1E,0x1E,0x1E)
#define CLR_TAB_BG        RGB(0x2D,0x2D,0x2D)
#define CLR_TAB_TEXT      RGB(0x9D,0x9D,0x9D)
#define CLR_TAB_ACTTEXT   RGB(0xFF,0xFF,0xFF)
#define CLR_ACCENT        RGB(0x00,0x7A,0xCC)
#define CLR_SEPARATOR     RGB(0x3C,0x3C,0x3C)
#define CLR_LIST_BG       RGB(0x1E,0x1E,0x1E)
#define CLR_LIST_TEXT     RGB(0xD4,0xD4,0xD4)
#define CLR_LIST_SEL      RGB(0x26,0x4F,0x78)
#define CLR_HDR_BG        RGB(0x2D,0x2D,0x2D)
#define CLR_HDR_TEXT      RGB(0xC8,0xC8,0xC8)
#define CLR_HDR_BORDER    RGB(0x3F,0x3F,0x3F)
#define CLR_BTN_BG        RGB(0x3C,0x3C,0x3C)
#define CLR_BTN_HOV       RGB(0x50,0x50,0x50)
#define CLR_BTN_PRS       RGB(0x28,0x28,0x28)
#define CLR_BTN_BORDER    RGB(0x60,0x60,0x60)
#define CLR_BTN_TEXT      RGB(0xD4,0xD4,0xD4)
#define CLR_DIM_TEXT      RGB(0x6A,0x6A,0x6A)

/* ── Layout ─────────────────────────────────────────────────────────── */
#define TABBAR_H   32
#define BTNBAR_H   44
#define TAB_W     130
#define TAB_COUNT   2
#define BTN_W     120
#define BTN_H      28
#define PAD        10
#define LV_MARGIN   8

#define MAX_PRINTERS 512

static const wchar_t *TAB_LABELS[TAB_COUNT] = {
    L"Impressoras",
    L"Configurações"
};

/* ── Estado global ──────────────────────────────────────────────────── */
static HWND g_hwndMain;
static HWND g_hwndList;
static HWND g_hwndBtnAdd;
static HWND g_hwndBtnRemove;
static HWND g_hwndStatus;
static HFONT g_hFont;
static HBRUSH g_hbrBg;
static HBRUSH g_hbrTabBg;
static HBRUSH g_hbrHdr;
static int g_activeTab = 0;
static PrinterEntry g_printers[MAX_PRINTERS];
static int g_count = 0;

/* ── Helpers ─────────────────────────────────────────────────────────── */
static HFONT create_font(int size, BOOL bold) {
    return CreateFontW(
        -MulDiv(size, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0,
        bold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI");
}

static void update_status(void) {
    wchar_t buf[64];
    if (g_count == 1)
        _snwprintf_s(buf, 64, _TRUNCATE, L"  1 impressora cadastrada");
    else
        _snwprintf_s(buf, 64, _TRUNCATE, L"  %d impressoras cadastradas", g_count);
    SendMessageW(g_hwndStatus, SB_SETTEXT, 0, (LPARAM)buf);
}

static void list_refresh(void) {
    ListView_DeleteAllItems(g_hwndList);
    for (int i = 0; i < g_count; i++) {
        LVITEMW lvi = {0};
        lvi.mask    = LVIF_TEXT;
        lvi.iItem   = i;
        lvi.pszText = g_printers[i].name;
        ListView_InsertItem(g_hwndList, &lvi);
        ListView_SetItemText(g_hwndList, i, 1, g_printers[i].portName);
        ListView_SetItemText(g_hwndList, i, 2, g_printers[i].outputPath);
    }
    update_status();
}

static void switch_tab(int tab) {
    g_activeTab = tab;
    BOOL imp = (tab == 0);
    ShowWindow(g_hwndList,      imp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnAdd,    imp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRemove, imp ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_hwndMain, NULL, TRUE);
    UpdateWindow(g_hwndMain);
}

/* ── Subclass do ListView: header escuro ─────────────────────────────── */
static LRESULT CALLBACK ListViewSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                          UINT_PTR uid, DWORD_PTR data) {
    if (msg == WM_NOTIFY) {
        NMHDR *nhdr = (NMHDR *)lp;
        if (nhdr->code == NM_CUSTOMDRAW) {
            NMCUSTOMDRAW *cd = (NMCUSTOMDRAW *)lp;
            switch (cd->dwDrawStage) {
            case CDDS_PREPAINT:
                FillRect(cd->hdc, &cd->rc, g_hbrHdr);
                return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: {
                FillRect(cd->hdc, &cd->rc, g_hbrHdr);

                wchar_t buf[128] = {0};
                HDITEMW hdi = {0};
                hdi.mask       = HDI_TEXT;
                hdi.pszText    = buf;
                hdi.cchTextMax = 128;
                SendMessageW(nhdr->hwndFrom, HDM_GETITEM,
                             cd->dwItemSpec, (LPARAM)&hdi);

                SetTextColor(cd->hdc, CLR_HDR_TEXT);
                SetBkMode(cd->hdc, TRANSPARENT);
                HFONT of = (HFONT)SelectObject(cd->hdc, g_hFont);
                RECT rt = cd->rc;
                InflateRect(&rt, -6, 0);
                DrawTextW(cd->hdc, buf, -1, &rt,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                SelectObject(cd->hdc, of);

                HPEN hp = CreatePen(PS_SOLID, 1, CLR_HDR_BORDER);
                HPEN op = (HPEN)SelectObject(cd->hdc, hp);
                MoveToEx(cd->hdc, cd->rc.right - 1, cd->rc.top, NULL);
                LineTo(cd->hdc, cd->rc.right - 1, cd->rc.bottom);
                MoveToEx(cd->hdc, cd->rc.left, cd->rc.bottom - 1, NULL);
                LineTo(cd->hdc, cd->rc.right, cd->rc.bottom - 1);
                SelectObject(cd->hdc, op);
                DeleteObject(hp);

                return CDRF_SKIPDEFAULT;
            }
            }
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* ── Botão owner-draw ────────────────────────────────────────────────── */
static void draw_button(DRAWITEMSTRUCT *di) {
    HDC dc  = di->hDC;
    RECT rc = di->rcItem;
    BOOL hot = (di->itemState & ODS_HOTLIGHT) != 0;
    BOOL sel = (di->itemState & ODS_SELECTED) != 0;

    COLORREF bg = sel ? CLR_BTN_PRS : (hot ? CLR_BTN_HOV : CLR_BTN_BG);
    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(dc, &rc, hbr);
    DeleteObject(hbr);

    HPEN hpen = CreatePen(PS_SOLID, 1, CLR_BTN_BORDER);
    HPEN opn  = (HPEN)SelectObject(dc, hpen);
    HBRUSH obr = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dc, opn);
    SelectObject(dc, obr);
    DeleteObject(hpen);

    wchar_t txt[64];
    GetWindowTextW(di->hwndItem, txt, 64);
    SetTextColor(dc, CLR_BTN_TEXT);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_hFont);
    DrawTextW(dc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);

    if (di->itemState & ODS_FOCUS) {
        InflateRect(&rc, -3, -3);
        DrawFocusRect(dc, &rc);
    }
}

/* ── WM_CREATE ───────────────────────────────────────────────────────── */
static void on_create(HWND hwnd) {
    g_hwndMain = hwnd;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

    g_hFont    = create_font(9, FALSE);
    g_hbrBg    = CreateSolidBrush(CLR_BG);
    g_hbrTabBg = CreateSolidBrush(CLR_TAB_BG);
    g_hbrHdr   = CreateSolidBrush(CLR_HDR_BG);

    /* ListView com borda sunken e margem */
    g_hwndList = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        LV_MARGIN, TABBAR_H + LV_MARGIN, 0, 0,
        hwnd, (HMENU)(UINT_PTR)IDC_PRINTER_LIST, hInst, NULL);

    ListView_SetExtendedListViewStyle(g_hwndList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(g_hwndList,     CLR_LIST_BG);
    ListView_SetTextColor(g_hwndList,   CLR_LIST_TEXT);
    ListView_SetTextBkColor(g_hwndList, CLR_LIST_BG);
    SendMessageW(g_hwndList, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    LVCOLUMNW col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 200; col.pszText = L"Nome";
    ListView_InsertColumn(g_hwndList, 0, &col);
    col.cx = 150; col.pszText = L"Porta";
    ListView_InsertColumn(g_hwndList, 1, &col);
    col.cx = 260; col.pszText = L"Pasta de Destino";
    ListView_InsertColumn(g_hwndList, 2, &col);

    /* Subclass para header escuro */
    SetWindowSubclass(g_hwndList, ListViewSubclass, 0, 0);

    /* Botoes na barra inferior */
    g_hwndBtnAdd = CreateWindowW(L"BUTTON", L"+ Adicionar",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0,
        hwnd, (HMENU)(UINT_PTR)IDC_BTN_ADD, hInst, NULL);

    g_hwndBtnRemove = CreateWindowW(L"BUTTON", L"- Remover",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0,
        hwnd, (HMENU)(UINT_PTR)IDC_BTN_REMOVE, hInst, NULL);

    SendMessageW(g_hwndBtnAdd,    WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_hwndBtnRemove, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    /* Status bar */
    g_hwndStatus = CreateWindowExW(0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwnd, (HMENU)(UINT_PTR)IDC_STATUS_BAR, hInst, NULL);
    SendMessageW(g_hwndStatus, SB_SETBKCOLOR, 0, (LPARAM)CLR_ACCENT);
    SendMessageW(g_hwndStatus, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    /* Menu */
    HMENU hMenu = CreateMenu();
    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_EXIT, L"&Sair\tAlt+F4");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");
    HMENU hEdit = CreatePopupMenu();
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEdit, L"&Edit");
    SetMenu(hwnd, hMenu);

    /* Dados */
    PrinterEntry *loaded = NULL;
    int n = store_load(&loaded);
    g_count = (n > MAX_PRINTERS) ? MAX_PRINTERS : n;
    if (g_count > 0)
        memcpy(g_printers, loaded, (size_t)g_count * sizeof(PrinterEntry));
    store_free(loaded);
    list_refresh();
}

/* ── WM_SIZE ─────────────────────────────────────────────────────────── */
static void on_size(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;

    SendMessageW(g_hwndStatus, WM_SIZE, 0, 0);
    RECT rcSt; GetWindowRect(g_hwndStatus, &rcSt);
    int statusH = rcSt.bottom - rcSt.top;

    int contentTop = TABBAR_H;
    int btnBarTop  = h - BTNBAR_H - statusH;
    int listH      = btnBarTop - contentTop - LV_MARGIN * 2;

    /* ListView com margem */
    SetWindowPos(g_hwndList, NULL,
        LV_MARGIN, contentTop + LV_MARGIN,
        w - LV_MARGIN * 2, listH,
        SWP_NOZORDER);

    /* Botoes na barra inferior */
    int btnY = btnBarTop + (BTNBAR_H - BTN_H) / 2;
    SetWindowPos(g_hwndBtnAdd,    NULL, PAD,           btnY, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(g_hwndBtnRemove, NULL, PAD*2 + BTN_W, btnY, BTN_W, BTN_H, SWP_NOZORDER);

    RECT rcInv = {0, 0, w, TABBAR_H};
    InvalidateRect(hwnd, &rcInv, FALSE);
    RECT rcBtn = {0, btnBarTop, w, btnBarTop + BTNBAR_H};
    InvalidateRect(hwnd, &rcBtn, FALSE);
}

/* ── WM_PAINT ────────────────────────────────────────────────────────── */
static void on_paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;

    /* Tab bar background */
    RECT rcTabs = {0, 0, w, TABBAR_H};
    FillRect(dc, &rcTabs, g_hbrTabBg);

    /* Tabs */
    for (int i = 0; i < TAB_COUNT; i++) {
        RECT rt = {i * TAB_W, 0, (i + 1) * TAB_W, TABBAR_H};
        BOOL active = (i == g_activeTab);

        HBRUSH hbr = CreateSolidBrush(active ? CLR_BG : CLR_TAB_BG);
        FillRect(dc, &rt, hbr);
        DeleteObject(hbr);

        SetTextColor(dc, active ? CLR_TAB_ACTTEXT : CLR_TAB_TEXT);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_hFont);
        RECT rtxt = rt; rtxt.bottom -= 2;
        DrawTextW(dc, TAB_LABELS[i], -1, &rtxt,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);

        if (active) {
            HBRUSH ha = CreateSolidBrush(CLR_ACCENT);
            RECT rl = {rt.left, rt.bottom - 2, rt.right, rt.bottom};
            FillRect(dc, &rl, ha);
            DeleteObject(ha);
        }
    }

    /* Borda inferior da tab bar */
    {
        HBRUSH hs = CreateSolidBrush(CLR_SEPARATOR);
        RECT rs = {0, TABBAR_H - 1, w, TABBAR_H};
        FillRect(dc, &rs, hs);
        DeleteObject(hs);
    }

    /* Barra de botoes inferior */
    {
        RECT rcSt; GetWindowRect(g_hwndStatus, &rcSt);
        int sh = rcSt.bottom - rcSt.top;
        int btnBarTop = h - BTNBAR_H - sh;
        RECT rbb = {0, btnBarTop, w, btnBarTop + BTNBAR_H};
        FillRect(dc, &rbb, g_hbrBg);
        HBRUSH hs = CreateSolidBrush(CLR_SEPARATOR);
        RECT rs = {0, btnBarTop, w, btnBarTop + 1};
        FillRect(dc, &rs, hs);
        DeleteObject(hs);
    }

    /* Painel Configuracoes */
    if (g_activeTab == 1) {
        RECT rcSt; GetWindowRect(g_hwndStatus, &rcSt);
        int sh = rcSt.bottom - rcSt.top;
        RECT rcCfg = {0, TABBAR_H, w, h - sh - BTNBAR_H};
        FillRect(dc, &rcCfg, g_hbrBg);
        SetTextColor(dc, CLR_DIM_TEXT);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_hFont);
        DrawTextW(dc, L"Configurações em breve...", -1, &rcCfg,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);
    }

    EndPaint(hwnd, &ps);
}

/* ── Acoes ───────────────────────────────────────────────────────────── */
static void on_add(HWND hwnd) {
    if (g_count >= MAX_PRINTERS) return;
    PrinterEntry entry = {0};
    if (dlg_add_show(hwnd, &entry)) {
        g_printers[g_count++] = entry;
        list_refresh();
        store_save(g_printers, g_count);
    }
}

static void on_remove(HWND hwnd) {
    (void)hwnd;
    int sel = ListView_GetNextItem(g_hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= g_count) return;
    for (int i = sel; i < g_count - 1; i++)
        g_printers[i] = g_printers[i + 1];
    g_count--;
    list_refresh();
    store_save(g_printers, g_count);
}

/* ── WndProc ─────────────────────────────────────────────────────────── */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        on_create(hwnd);
        return 0;

    case WM_SIZE:
        on_size(hwnd);
        return 0;

    case WM_PAINT:
        on_paint(hwnd);
        return 0;

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_hbrBg);
        return 1;
    }

    case WM_LBUTTONDOWN: {
        int x = (int)(short)LOWORD(lp);
        int y = (int)(short)HIWORD(lp);
        if (y >= 0 && y < TABBAR_H) {
            int tab = x / TAB_W;
            if (tab >= 0 && tab < TAB_COUNT && tab != g_activeTab)
                switch_tab(tab);
        }
        return 0;
    }

    case WM_DRAWITEM:
        draw_button((DRAWITEMSTRUCT *)lp);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_ADD:    on_add(hwnd);        break;
        case IDC_BTN_REMOVE: on_remove(hwnd);     break;
        case IDM_EXIT:       DestroyWindow(hwnd); break;
        }
        return 0;

    case WM_NOTIFY: {
        NMHDR *hdr = (NMHDR *)lp;
        if (hdr->idFrom == IDC_PRINTER_LIST && hdr->code == NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW *cd = (NMLVCUSTOMDRAW *)lp;
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:     return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT:
                cd->clrText   = CLR_LIST_TEXT;
                cd->clrTextBk = (cd->nmcd.uItemState & CDIS_SELECTED)
                                ? CLR_LIST_SEL : CLR_LIST_BG;
                return CDRF_NEWFONT;
            }
        }
        return CDRF_DODEFAULT;
    }

    case WM_DESTROY:
        RemoveWindowSubclass(g_hwndList, ListViewSubclass, 0);
        DeleteObject(g_hFont);
        DeleteObject(g_hbrBg);
        DeleteObject(g_hbrTabBg);
        DeleteObject(g_hbrHdr);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ── Registro e criacao ──────────────────────────────────────────────── */
BOOL mainwnd_register(HINSTANCE hInst) {
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = WC_MAINWND;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    return RegisterClassExW(&wc) != 0;
}

HWND mainwnd_create(HINSTANCE hInst) {
    return CreateWindowExW(
        0,
        WC_MAINWND,
        L"MedDrive — Gerenciador de Impressoras",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 520,
        NULL, NULL, hInst, NULL);
}
