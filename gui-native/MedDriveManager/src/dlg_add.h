#pragma once
#include <windows.h>
#include "store.h"

/* Exibe o dialog modal "Adicionar Impressora".
   profiles/profileCount = perfis disponíveis (carregados em mainwnd).
   Retorna TRUE e preenche *out se o usuário clicar OK. */
BOOL dlg_add_show(HWND parent, PrinterEntry *out,
                  const ProfileEntry *profiles, int profileCount);
