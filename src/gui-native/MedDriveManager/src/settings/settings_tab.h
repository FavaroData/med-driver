#pragma once
#include <windows.h>

void    settings_tab_create(HWND parent, HINSTANCE hInst);
void    settings_tab_show(BOOL visible);
void    settings_tab_load(void);
void    settings_tab_paint(HDC dc);
BOOL    settings_tab_command(UINT id);
LRESULT settings_tab_ctlcolor(HWND hctl, HDC hdc);
BOOL    settings_tab_drawitem(DRAWITEMSTRUCT *dis);
BOOL    settings_tab_require_agent(void);
void    settings_tab_enable(BOOL enabled);
BOOL    settings_tab_is_locked(void);
void    settings_tab_set_on_unlock(void (*cb)(BOOL enabled));
void    settings_tab_vscroll(WPARAM wp);
void    settings_tab_mousewheel(int delta);
