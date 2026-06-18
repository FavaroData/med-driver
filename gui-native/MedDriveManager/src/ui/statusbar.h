#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "theme.h"

/* Cria a janela filha da status bar. Retorna o HWND. */
HWND statusbar_create(HWND hwndParent, HINSTANCE hInst);

/* Posiciona a status bar no rodapé. */
void statusbar_resize(HWND hwndSb, int clientW, int clientH);

/* Atualiza o texto exibido e repinta. */
void statusbar_set_text(HWND hwndSb, int count);
