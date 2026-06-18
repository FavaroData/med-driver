#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "listview.h"
#include "theme.h"
#include "resource.h"

int g_hoverRow = -1;

/* ── Colunas ─────────────────────────────────────────────────────────── */
#define COL_COUNT 4
static const int COL_W[COL_COUNT] = {150, 200, 185, 377}; /* soma = 912 */
static const HICON *COL_ICO[COL_COUNT];  /* preenchido em listview_create */

/* ── SubclassProc do Header: fundo CLR_CARD + ícones ─────────────────── */
static LRESULT CALLBACK HeaderSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                        UINT_PTR uid, DWORD_PTR data) {
    (void)uid; (void)data;
    if (msg == WM_ERASEBKGND) {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_hbrCard);
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_hbrCard);

        int count = (int)SendMessageW(hwnd, HDM_GETITEMCOUNT, 0, 0);
        for (int i = 0; i < count; i++) {
            RECT ir = {0};
            SendMessageW(hwnd, HDM_GETITEMRECT, (WPARAM)i, (LPARAM)&ir);

            wchar_t buf[128] = {0};
            HDITEMW hdi = {0};
            hdi.mask       = HDI_TEXT;
            hdi.pszText    = buf;
            hdi.cchTextMax = 128;
            SendMessageW(hwnd, HDM_GETITEM, (WPARAM)i, (LPARAM)&hdi);

            /* Ícone 16px */
            if (COL_ICO[i] && *COL_ICO[i]) {
                int iy = (ir.bottom - ir.top - 16) / 2;
                DrawIconEx(dc, ir.left + 8, ir.top + iy,
                           *COL_ICO[i], 16, 16, 0, NULL, DI_NORMAL);
            }
            int textX = ir.left + 30;

            SetTextColor(dc, CLR_TEXT_SECONDARY);
            SetBkMode(dc, TRANSPARENT);
            HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
            RECT rt = {textX, ir.top, ir.right - 4, ir.bottom};
            DrawTextW(dc, buf, -1, &rt,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(dc, of);

            /* Borda vertical entre colunas */
            if (i < count - 1) {
                HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
                HPEN op = (HPEN)SelectObject(dc, hp);
                MoveToEx(dc, ir.right - 1, ir.top + 4, NULL);
                LineTo(dc, ir.right - 1, ir.bottom - 4);
                SelectObject(dc, op);
                DeleteObject(hp);
            }
        }

        /* Linha inferior */
        HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
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

/* ── SubclassProc do ListView: hover tracking ────────────────────────── */
static LRESULT CALLBACK ListSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                      UINT_PTR uid, DWORD_PTR data) {
    (void)uid; (void)data;
    if (msg == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);

        LVHITTESTINFO ht = {0};
        ht.pt.x = (int)(short)LOWORD(lp);
        ht.pt.y = (int)(short)HIWORD(lp);
        SendMessageW(hwnd, LVM_HITTEST, 0, (LPARAM)&ht);

        int newHov = (ht.flags & LVHT_ONITEM) ? ht.iItem : -1;
        if (newHov != g_hoverRow) {
            int old = g_hoverRow;
            g_hoverRow = newHov;
            if (old >= 0)  ListView_RedrawItems(hwnd, old,  old);
            if (newHov >= 0) ListView_RedrawItems(hwnd, newHov, newHov);
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    if (msg == WM_MOUSELEAVE) {
        int old = g_hoverRow;
        g_hoverRow = -1;
        if (old >= 0) ListView_RedrawItems(hwnd, old, old);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* ── API pública ─────────────────────────────────────────────────────── */

HWND listview_create(HWND hwndParent, HINSTANCE hInst, int x, int y, int w, int h) {
    HWND hwndList = CreateWindowExW(
        0, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL |
        LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED,
        x, y, w, h,
        hwndParent, (HMENU)(UINT_PTR)IDC_PRINTER_LIST, hInst, NULL);

    ListView_SetExtendedListViewStyle(hwndList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(hwndList,     CLR_BG_PRIMARY);
    ListView_SetTextBkColor(hwndList, CLR_BG_PRIMARY);
    ListView_SetTextColor(hwndList,   CLR_TEXT_ROW);
    SendMessageW(hwndList, WM_SETFONT, (WPARAM)g_fontContent, TRUE);

    /* Colunas */
    LVCOLUMNW col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = COL_W[0]; col.pszText = L"Porta";
    ListView_InsertColumn(hwndList, 0, &col);
    col.cx = COL_W[1]; col.pszText = L"Impressora";
    ListView_InsertColumn(hwndList, 1, &col);
    col.cx = COL_W[2]; col.pszText = L"Nome do arquivo";
    ListView_InsertColumn(hwndList, 2, &col);
    col.cx = COL_W[3]; col.pszText = L"Pasta de destino";
    ListView_InsertColumn(hwndList, 3, &col);

    /* Ponteiros para ícones dos cabeçalhos (preenchidos aqui porque theme já foi init) */
    COL_ICO[0] = &g_icoPlug16;
    COL_ICO[1] = &g_icoPrinter16;
    COL_ICO[2] = &g_icoDocument16;
    COL_ICO[3] = &g_icoFolder16;

    /* Subclass header e listview */
    HWND hwndHdr = ListView_GetHeader(hwndList);
    SetWindowSubclass(hwndHdr, HeaderSubclass, 1, 0);
    SetWindowSubclass(hwndList, ListSubclass,  2, 0);

    return hwndList;
}

void listview_resize(HWND hwndList, int x, int y, int w, int h) {
    SetWindowPos(hwndList, NULL, x, y, w, h, SWP_NOZORDER);
}

void listview_measure(MEASUREITEMSTRUCT *mis) {
    mis->itemHeight = ROW_H;
}

void listview_draw_item(DRAWITEMSTRUCT *dis) {
    if (dis->itemID == (UINT)-1) return;

    HDC     dc  = dis->hDC;
    RECT    rc  = dis->rcItem;
    UINT    idx = dis->itemID;
    BOOL    sel = (dis->itemState & ODS_SELECTED) != 0;
    BOOL    hov = ((int)idx == g_hoverRow) && !sel;

    COLORREF bgColor = sel ? CLR_ROW_SELECTED
                     : hov ? CLR_ROW_HOVER
                           : (idx % 2 == 0 ? CLR_BG_PRIMARY : CLR_BG_SECONDARY);

    HBRUSH hbr = CreateSolidBrush(bgColor);
    FillRect(dc, &rc, hbr);
    DeleteObject(hbr);

    /* Linha horizontal de separação (1px, bottom da linha) */
    if (!sel && !hov) {
        HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HPEN op = (HPEN)SelectObject(dc, hp);
        MoveToEx(dc, rc.left, rc.bottom - 1, NULL);
        LineTo(dc, rc.right, rc.bottom - 1);
        SelectObject(dc, op);
        DeleteObject(hp);
    }

    /* Texto de cada coluna */
    SetTextColor(dc, sel ? CLR_TEXT_PRIMARY : CLR_TEXT_ROW);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontContent);

    int x = rc.left;
    for (int col = 0; col < COL_COUNT; col++) {
        int colW = ListView_GetColumnWidth(dis->hwndItem, col);
        wchar_t text[512] = {0};
        ListView_GetItemText(dis->hwndItem, (int)idx, col, text, 512);

        RECT rcCell = {x + 8, rc.top, x + colW - 4, rc.bottom};
        DrawTextW(dc, text, -1, &rcCell,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        x += colW;
    }

    SelectObject(dc, of);

    if (dis->itemState & ODS_FOCUS) {
        RECT rcFocus = rc;
        InflateRect(&rcFocus, -1, -1);
        DrawFocusRect(dc, &rcFocus);
    }
}
