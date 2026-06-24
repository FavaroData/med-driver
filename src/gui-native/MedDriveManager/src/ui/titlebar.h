#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "theme.h"

void titlebar_create_buttons(HWND hwndParent, HINSTANCE hInst);
void titlebar_draw_button(DRAWITEMSTRUCT *dis);
void titlebar_paint(HDC dc, int clientW);
LRESULT titlebar_nchittest(HWND hwnd, int x, int y);
