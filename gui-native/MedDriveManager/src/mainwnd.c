#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <winspool.h>
#include "mainwnd.h"
#include "dlg_add.h"
#include "dlg_progress.h"
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
static HWND g_hwndHeader;
static HWND g_hwndBtnAdd;
static HWND g_hwndBtnRemove;
static HWND g_hwndBtnRefresh;
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
        lvi.pszText = g_printers[i].portName;
        ListView_InsertItem(g_hwndList, &lvi);

        ListView_SetItemText(g_hwndList, i, 1, g_printers[i].name);

        /* Coluna "Nome do arquivo": exibe o padrão gerado na impressão */
        wchar_t filePattern[PRINTER_BASENAME_MAX + 8] = {0};
        if (g_printers[i].outputBaseName[0])
            _snwprintf_s(filePattern, PRINTER_BASENAME_MAX + 8, _TRUNCATE,
                         L"%s-N.pdf", g_printers[i].outputBaseName);
        ListView_SetItemText(g_hwndList, i, 2, filePattern);
        ListView_SetItemText(g_hwndList, i, 3, g_printers[i].outputPath);
    }
    update_status();
}

static void switch_tab(int tab) {
    g_activeTab = tab;
    BOOL imp = (tab == 0);
    ShowWindow(g_hwndList,       imp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnAdd,     imp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRemove,  imp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRefresh, imp ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_hwndMain, NULL, TRUE);
    UpdateWindow(g_hwndMain);
}

/* ── Subclass do Header: fundo escuro completo ───────────────────────── */
static LRESULT CALLBACK HeaderSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                        UINT_PTR uid, DWORD_PTR data) {
    if (msg == WM_ERASEBKGND) {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_hbrHdr);
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        /* Fundo completo — cobre a area vazia apos a ultima coluna */
        FillRect(dc, &rc, g_hbrHdr);

        int count = (int)SendMessageW(hwnd, HDM_GETITEMCOUNT, 0, 0);
        for (int i = 0; i < count; i++) {
            RECT itemRc = {0};
            SendMessageW(hwnd, HDM_GETITEMRECT, (WPARAM)i, (LPARAM)&itemRc);

            wchar_t buf[128] = {0};
            HDITEMW hdi = {0};
            hdi.mask       = HDI_TEXT;
            hdi.pszText    = buf;
            hdi.cchTextMax = 128;
            SendMessageW(hwnd, HDM_GETITEM, (WPARAM)i, (LPARAM)&hdi);

            /* Texto */
            SetTextColor(dc, CLR_HDR_TEXT);
            SetBkMode(dc, TRANSPARENT);
            HFONT of = (HFONT)SelectObject(dc, g_hFont);
            RECT rt = itemRc;
            InflateRect(&rt, -6, 0);
            DrawTextW(dc, buf, -1, &rt,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(dc, of);

            /* Borda direita — apenas entre colunas, nao na ultima */
            if (i < count - 1) {
                HPEN hp = CreatePen(PS_SOLID, 1, CLR_HDR_BORDER);
                HPEN op = (HPEN)SelectObject(dc, hp);
                MoveToEx(dc, itemRc.right - 1, itemRc.top, NULL);
                LineTo(dc, itemRc.right - 1, itemRc.bottom);
                SelectObject(dc, op);
                DeleteObject(hp);
            }
        }

        /* Linha inferior do header */
        HPEN hp = CreatePen(PS_SOLID, 1, CLR_HDR_BORDER);
        HPEN op = (HPEN)SelectObject(dc, hp);
        MoveToEx(dc, rc.left, rc.bottom - 1, NULL);
        LineTo(dc, rc.right, rc.bottom - 1);
        SelectObject(dc, op);
        DeleteObject(hp);

        EndPaint(hwnd, &ps);
        return 0;
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
    col.cx = 150; col.pszText = L"Porta";
    ListView_InsertColumn(g_hwndList, 0, &col);
    col.cx = 180; col.pszText = L"Impressora";
    ListView_InsertColumn(g_hwndList, 1, &col);
    col.cx = 160; col.pszText = L"Nome do arquivo";
    ListView_InsertColumn(g_hwndList, 2, &col);
    col.cx = 270; col.pszText = L"Pasta de destino";
    ListView_InsertColumn(g_hwndList, 3, &col);

    /* Subclass direto no header para fundo escuro completo */
    g_hwndHeader = ListView_GetHeader(g_hwndList);
    SetWindowSubclass(g_hwndHeader, HeaderSubclass, 0, 0);

    /* Botoes na barra inferior */
    g_hwndBtnAdd = CreateWindowW(L"BUTTON", L"+ Adicionar",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0,
        hwnd, (HMENU)(UINT_PTR)IDC_BTN_ADD, hInst, NULL);

    g_hwndBtnRemove = CreateWindowW(L"BUTTON", L"- Remover",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0,
        hwnd, (HMENU)(UINT_PTR)IDC_BTN_REMOVE, hInst, NULL);

    g_hwndBtnRefresh = CreateWindowW(L"BUTTON", L"Atualizar",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0,
        hwnd, (HMENU)(UINT_PTR)IDC_BTN_REFRESH, hInst, NULL);

    SendMessageW(g_hwndBtnAdd,     WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_hwndBtnRemove,  WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_hwndBtnRefresh, WM_SETFONT, (WPARAM)g_hFont, TRUE);

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
    SetWindowPos(g_hwndBtnAdd,     NULL, PAD,             btnY, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(g_hwndBtnRemove,  NULL, PAD*2 + BTN_W,   btnY, BTN_W, BTN_H, SWP_NOZORDER);
    SetWindowPos(g_hwndBtnRefresh, NULL, PAD*3 + BTN_W*2, btnY, BTN_W, BTN_H, SWP_NOZORDER);

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

/* ── Sincroniza g_printers[] com EnumPrinters ────────────────────────── */
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
                    wcsncpy_s(e->name, PRINTER_NAME_MAX, info[i].pPrinterName, _TRUNCATE);
                    if (info[i].pPortName)
                        wcsncpy_s(e->portName, PRINTER_PORT_MAX, info[i].pPortName, _TRUNCATE);

                    /* Lê OutputPath direto do registry da porta */
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

/* ── Acoes ───────────────────────────────────────────────────────────── */
static void on_add(HWND hwnd) {
    if (g_count >= MAX_PRINTERS) return;

    /* Verifica pre-requisito: DLL instalada em System32 */
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
        case IDC_BTN_ADD:     on_add(hwnd);        break;
        case IDC_BTN_REMOVE:  on_remove(hwnd);     break;
        case IDC_BTN_REFRESH: sync_with_system();  break;
        case IDM_EXIT:        DestroyWindow(hwnd); break;
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
        RemoveWindowSubclass(g_hwndHeader, HeaderSubclass, 0);
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
    int w  = 800, h  = 560;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x  = (sw - w) / 2;
    int y  = (sh - h) / 2;
    return CreateWindowExW(
        0,
        WC_MAINWND,
        L"Meddrive Printer Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,
        x, y, w, h,
        NULL, NULL, hInst, NULL);
}
