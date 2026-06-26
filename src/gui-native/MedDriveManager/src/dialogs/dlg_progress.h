#pragma once
#include <windows.h>

/* Exibe o dialog de progresso, lanca add-printer.ps1 com -ProfileName e
   -PrinterName, mostra a saida em tempo real. Retorna TRUE se saiu com 0. */
BOOL dlg_progress_run(HWND parent,
                      const wchar_t *printerName,
                      const wchar_t *profileName);

/* Exibe o dialog de progresso, lanca remove-printer.ps1 elevado via runas e
   mostra a saida em tempo real. Retorna TRUE se o script saiu com codigo 0. */
BOOL dlg_progress_remove(HWND parent, const wchar_t *printerName);

/* Exibe o dialog de progresso, lanca create-profile.ps1 e mostra a saida em
   tempo real. Retorna TRUE se o script saiu com codigo 0. */
BOOL dlg_progress_create_profile(HWND parent,
                                  const wchar_t *profileName,
                                  const wchar_t *outputPath,
                                  const wchar_t *outputBaseName,
                                  BOOL openAfterGenerate,
                                  BOOL overwriteFile,
                                  BOOL choosePath);

/* Lanca edit-profile.ps1. newProfileName pode ser igual a profileName (sem renomear). */
BOOL dlg_progress_edit_profile(HWND parent,
                                const wchar_t *profileName,
                                const wchar_t *newProfileName,
                                const wchar_t *outputPath,
                                const wchar_t *outputBaseName,
                                BOOL openAfterGenerate,
                                BOOL overwriteFile,
                                BOOL choosePath);

/* Lanca remove-profile.ps1. */
BOOL dlg_progress_remove_profile(HWND parent, const wchar_t *profileName);

/* Lanca edit-printer.ps1 (Rename-Printer + Set-Printer -PortName). */
BOOL dlg_progress_edit_printer(HWND parent,
                                const wchar_t *oldPrinterName,
                                const wchar_t *newPrinterName,
                                const wchar_t *profileName);
