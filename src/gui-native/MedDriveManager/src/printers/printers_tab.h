#pragma once
#include <windows.h>
#include "store.h"

void                printers_tab_create(HWND parent, HINSTANCE hInst, HWND hwndStatus);
void                printers_tab_show(BOOL visible);
void                printers_tab_sync(void);
void                printers_tab_paint(HDC dc, RECT rcContent);
BOOL                printers_tab_command(UINT id);
BOOL                printers_tab_drawitem(DRAWITEMSTRUCT *dis);
BOOL                printers_tab_measure(MEASUREITEMSTRUCT *mis);
void                printers_tab_set_on_change(void (*cb)(void));
const PrinterEntry* printers_tab_get(int *out_count);
