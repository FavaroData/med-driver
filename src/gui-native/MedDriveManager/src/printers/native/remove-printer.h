#pragma once
#include <windows.h>

/* Remove uma impressora do Spooler, nativo (sem PowerShell).
   Equivalente ao remove-printer.ps1. Loga em C:\Windows\Temp\meddrive_printer_manager.log.
   Retorna TRUE se a impressora foi removida (ou ja nao existia). */
BOOL remove_printer(const wchar_t *printerName);
