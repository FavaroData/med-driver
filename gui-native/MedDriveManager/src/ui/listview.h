#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "theme.h"

/* Cria o ListView owner-draw e subclassa o Header. Retorna o HWND. */
HWND listview_create(HWND hwndParent, HINSTANCE hInst, int x, int y, int w, int h);

/* Posiciona o ListView e seu Header (WM_SIZE). */
void listview_resize(HWND hwndList, int x, int y, int w, int h);

/* Responde WM_MEASUREITEM para forçar ROW_H. */
void listview_measure(MEASUREITEMSTRUCT *mis);

/* Desenha uma linha (WM_DRAWITEM), incluindo todas as colunas. */
void listview_draw_item(DRAWITEMSTRUCT *dis);

/* Handle de hover externo (usado para invalidar a linha anterior). */
extern int g_hoverRow;
