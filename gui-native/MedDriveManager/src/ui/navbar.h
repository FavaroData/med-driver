#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "theme.h"

/* Pinta a navbar horizontal (2 abas: Impressoras / Configurações). */
void navbar_paint(HDC dc, int clientW, int activeTab);

/* Retorna o índice da aba clicada (0 ou 1), ou -1 se nenhuma. */
int navbar_hittest(int x, int y);
