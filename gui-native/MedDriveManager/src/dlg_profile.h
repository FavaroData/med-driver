#pragma once
#include <windows.h>
#include "store.h"

/* Exibe o dialog modal "Novo Perfil".
   Retorna TRUE e preenche *out se o usuário clicar Salvar. */
BOOL dlg_profile_show(HWND parent, ProfileEntry *out);
