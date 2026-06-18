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
        HBRUSH hbr = CreateSolidBrush(CLR_STATUSBAR_BG);
        FillRect(dc, &rc, hbr);
        DeleteObject(hbr);

        /* Linha superior azul (2px) */
        HBRUSH hbl = CreateSolidBrush(CLR_ACCENT);
        RECT rl = {rc.left, rc.top, rc.right, rc.top + 2};
        FillRect(dc, &rl, hbl);
        DeleteObject(hbl);

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
