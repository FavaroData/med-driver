#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "statusbar.h"
#include "theme.h"
#include "resource.h"

#define SB_CLASS L"MedDriveSB"

static wchar_t s_sbText[128] = L"0 impressoras cadastradas";

static LRESULT CALLBACK SbWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        /* Fundo */
        FillRect(dc, &rc, g_hbrSecondary);

        /* Linha superior (1px borda) */
        HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
        RECT rl = {rc.left, rc.top, rc.right, rc.top + 1};
        FillRect(dc, &rl, hbrd);
        DeleteObject(hbrd);

        /* Ícone info */
        if (g_icoInfo16)
            DrawIconEx(dc, 12, (rc.bottom - rc.top - 16) / 2,
                       g_icoInfo16, 16, 16, 0, NULL, DI_NORMAL);

        /* Texto */
        SetTextColor(dc, CLR_TEXT_SECONDARY);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
        RECT rt = {36, rc.top, rc.right - 8, rc.bottom};
        DrawTextW(dc, s_sbText, -1, &rt,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        /* Copyright à direita */
        RECT rr = {rc.left, rc.top, rc.right - 12, rc.bottom};
        DrawTextW(dc, L"StachIt \xA9 2026", -1, &rr,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HWND statusbar_create(HWND hwndParent, HINSTANCE hInst) {
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = SbWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = NULL;
    wc.lpszClassName = SB_CLASS;
    RegisterClassExW(&wc);  /* pode falhar se já registrada — ok */

    return CreateWindowExW(0, SB_CLASS, NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        hwndParent, (HMENU)(UINT_PTR)IDC_STATUS_BAR, hInst, NULL);
}

void statusbar_resize(HWND hwndSb, int clientW, int clientH) {
    SetWindowPos(hwndSb, NULL,
        0, clientH - STATUSBAR_H,
        clientW, STATUSBAR_H,
        SWP_NOZORDER);
}

void statusbar_set_text(HWND hwndSb, int count) {
    if (count == 1)
        _snwprintf_s(s_sbText, 128, _TRUNCATE, L"1 impressora cadastrada");
    else
        _snwprintf_s(s_sbText, 128, _TRUNCATE, L"%d impressoras cadastradas", count);
    InvalidateRect(hwndSb, NULL, TRUE);
}

void statusbar_set_text_raw(HWND hwndSb, const wchar_t *text) {
    wcsncpy_s(s_sbText, 128, text, _TRUNCATE);
    InvalidateRect(hwndSb, NULL, TRUE);
}
