#pragma once
#include <windows.h>
#include "store.h"

void                profiles_tab_create(HWND parent, HINSTANCE hInst, HWND hwndStatus);
void                profiles_tab_show(BOOL visible);
void                profiles_tab_load(void);
void                profiles_tab_paint(HDC dc, int clientW);
BOOL                profiles_tab_command(UINT id);
BOOL                profiles_tab_drawitem(DRAWITEMSTRUCT *dis);
void                profiles_tab_update_printers(const PrinterEntry *printers, int count);
const ProfileEntry* profiles_tab_get(int *out_count);
void                profiles_tab_enable(BOOL enabled);
