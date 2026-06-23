#pragma once
#include <windows.h>
#include "store.h"

/* Exibe o dialog "Adicionar / Editar Impressora".
   Se prefill != NULL, entra em modo edição: nome desabilitado, perfil pré-selecionado. */
BOOL dlg_add_show(HWND parent, PrinterEntry *out,
                  const ProfileEntry *profiles, int profileCount,
                  const PrinterEntry *prefill);
