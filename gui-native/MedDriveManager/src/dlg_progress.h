#pragma once
#include <windows.h>

/* Exibe o dialog de progresso, lanca add-printer.ps1 elevado via runas e
   mostra a saida em tempo real. Retorna TRUE se o script saiu com codigo 0. */
BOOL dlg_progress_run(HWND parent,
                      const wchar_t *printerName,
                      const wchar_t *outputPath);
