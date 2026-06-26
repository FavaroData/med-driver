#pragma once
#include <windows.h>

void    settings_tab_create(HWND parent, HINSTANCE hInst);
void    settings_tab_show(BOOL visible);
void    settings_tab_load(void);
void    settings_tab_paint(HDC dc);
BOOL    settings_tab_command(UINT id);
LRESULT settings_tab_ctlcolor(HWND hctl, HDC hdc);
BOOL    settings_tab_drawitem(DRAWITEMSTRUCT *dis);
