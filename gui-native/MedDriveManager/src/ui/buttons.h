#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "theme.h"

typedef enum { BTN_STYLE_PRIMARY, BTN_STYLE_SECONDARY } BtnStyle;

/* Instala rastreamento de hover num botão BS_OWNERDRAW qualquer. */
void buttons_install_hover(HWND hwnd);

/* Cria um botão de ação (owner-draw) já com hover instalado. */
HWND buttons_create(HWND hwndParent, HINSTANCE hInst,
                    int id, const wchar_t *label, BtnStyle style,
                    int x, int y, int w, int h);

/* Desenha o botão (WM_DRAWITEM). Retorna TRUE se tratou. */
BOOL buttons_draw(DRAWITEMSTRUCT *dis, BtnStyle style);
