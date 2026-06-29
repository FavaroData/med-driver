#pragma once
#include <windows.h>

/* Importa perfis, impressoras e configuracoes a partir de um arquivo JSON
   gerado pelo botao "Exportar configuracao". Exibe o dialogo de abertura
   de arquivo e mostra um resumo em MessageBox ao terminar. */
void import_config_run(HWND parent);
