#pragma once
#include <windows.h>

/* Abre o dialogo de senha unica e verifica contra o hash hardcoded.
   Retorna TRUE se a senha estiver correta. */
BOOL dlg_password_unlock(HWND parent);
