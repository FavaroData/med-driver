
[Meddrive Printer v1.2]

É Impressora virtual PDF para Windows que captura jobs de impressão e os converte automaticamente em arquivos PDF, salvando-os em uma pasta configurável desenvolvido para a empresa Stach IT.

Não exige a necessidade da interação do usuário após a instalação, basta instalar, decidir o nome da impressora, decidir o caminho de saída do arquivo e imprimir normalmente.

---

Como funciona?

São 3 arquivos source: 
- monitor.c - O código da DLL, com toda a lógica de interação com o Spooler e o Ghostscript (Processo de impressão nativo do windows)
- monitor.h - Módulo das variáveis de interação entre Spooler e Monitor (DLL)
- monitor.def - Módulo onde está determinado quais os caminhos o spooler lê

Os demais são arquivos de intalação com a lógica de configuração da impressora virtual.

Foi desenvolvido um driver personalizado baseado na arquitetura do PSCRIPT5 (Driver Postscript) que converte o documento (job enviado para impressão) em PostScript.

E todas as configurações (caminhos personalizados e do Ghostscript) são armazenadas no Registry do Windows e lidas pelo monitor no momento em que cada job de impressão é iniciado.

O Ghostscript é um interpretador de PostScript e PDF. 
Nesse projeto, ele é chamado recebendo os parâmetros necessários para chamada da API (parâmetros definidos de acordo com a configuração - OutputPath, PORT, Local da DLL e local do .exe do próprio ghostscript)

---

Fluxo completo de um job de impressão:

1. O Spooler carrega `meddrivemon.dll` e chama `InitializePrintMonitor2`, que preenche a estrutura `MONITOR2` (API do windows) com todos os ponteiros de função e armazena a chave raiz do Registry em `g_hkRoot`.

2. O Spooler consulta as portas disponíveis por meio do monitor, onde lê as subchaves de `Ports\` no Registry e retorna cada uma como uma porta virtual.

3. Ao receber um job, o Spooler chama `OpenPort` com o nome da porta. O monitor aloca um `PORT_CONTEXT` na heap e chama `ReadConfig` para ler `OutputPath` e `GhostscriptPath` do Registry. O ponteiro é devolvido como handle.

4. O monitor cria um arquivo temporário `.ps` em `%TEMP%` para acumular os dados PostScript.

5. O Spooler entrega os bytes PostScript em uma ou mais chamadas. Cada chamada acrescenta ao arquivo temporário via `WriteFile`.

6. Ao finalizar, o Spooler sinaliza o fim do job. O monitor fecha o handle do arquivo temporário, invoca `ConvertPsToPdf` (que executa o Ghostscript), aguarda a conclusão e deleta o arquivo temporário.

7. É chamado o `Monitor_ClosePort` e libera a memória do `PORT_CONTEXT`.

---

Pré-requisitos

- Windows 10 ou 11 (x64)
- Ghostscript instalado (testado com gs10.07.1 — `gswin64c.exe`)
- PowerShell com permissão para executar scripts (ver nota na seção de instalação)
- Execução como Administrador

---

Instalação

1. Execute como administrador o instalador MeddrivePrinter-Setup.exe

---

Configuração

As configurações ficam no Registry em:

HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\Meddrive Printer MONITOR\Ports\Meddrive Printer PORT <nome>

- Há uma regra de negócio definida onde a porta sempre vai ser salva com o nome Meddrive Printer PORT e o texto extraido do nome da impressora (remove "Meddrive Printer", "-" e espaços extras do nome informado).
Ex: 

Impressora: Meddrive Printer - Triton

Será salva como: Meddrive Printer PORT `Triton`

Valores:
- OutputPath — caminho completo do arquivo PDF de saída (ex: C:\PDF\saida.pdf)
- GhostscriptPath — caminho do executável do Ghostscript (ex: C:\Program Files\gs\gs10.07.1\bin\gswin64c.exe)
- Nome da subchave (porta) — derivado automaticamente do nome da impressora pelo instalador:

