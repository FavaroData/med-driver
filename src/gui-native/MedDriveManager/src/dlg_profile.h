#pragma once
#include <windows.h>
#include "store.h"

/* Exibe o dialog de perfil.
   prefill != NULL  → pré-preenche campos (modo editar/duplicar).
   title   != NULL  → substitui o caption do dialog.
   Retorna TRUE e preenche *out se o usuário clicar Salvar. */
BOOL dlg_profile_show(HWND parent, ProfileEntry *out,
                      const ProfileEntry *prefill, const wchar_t *title);
