# Meddrive Printer v3.4
> Desenvolvido para a empresa **StachIt**.

Impressora virtual PDF para Windows que captura jobs de impressão e os converte automaticamente em arquivos PDF, salvando-os em uma pasta configurável.

Não exige interação do usuário após a instalação — basta instalar, criar um perfil de saída e adicionar uma impressora pelo **MedDriveManager**.

---

## Componentes

| Arquivo | Descrição |
|---|---|
| `meddrivemon.dll` | DLL do monitor de impressão — intercepta jobs do Spooler e delega ao agente via named pipe |
| `MeddrivePrinterAgent.exe` | Agente de sessão — executa o Ghostscript com credenciais de rede do usuário logado |
| `MedDriveManager.exe` | Aplicativo gráfico de gerenciamento de perfis e impressoras |
| `MeddrivePrinter-Setup.exe` | Instalador para Windows 10/11 |
| `MeddrivePrinter-Win7-Setup.exe` | Instalador para Windows 7 x64 |
| `MeddrivePrinter-Vista-Setup.exe` | Instalador para Windows Vista x64 SP2 |

### Código-fonte

| Arquivo | Descrição |
|---|---|
| `src/monitor.c` | Implementação do monitor de impressão (MONITOR2 API) |
| `src/monitor.h` | Variáveis e estruturas compartilhadas |
| `src/monitor.def` | Exportações lidas pelo Spooler |
| `agent/MeddrivePrinterAgent.c` | Agente de sessão de usuário (named pipe + Ghostscript) |
| `installer/agent/register-agent.ps1` | Registro do agente no Task Scheduler (COM, compatível Vista+) |

### Scripts de gerenciamento (`installer/*/conf/`)

| Script | Descrição |
|---|---|
| `add-printer.ps1` | Cria uma impressora virtual vinculada a um perfil |
| `remove-printer.ps1` | Remove impressora e porta associada |
| `create-profile.ps1` | Cria um perfil de saída no registry |
| `edit-profile.ps1` | Edita nome, caminho e configurações de um perfil |
| `remove-profile.ps1` | Remove perfil e porta do registry |
| `edit-printer.ps1` | Renomeia impressora e reassocia a um perfil |

---

## Como funciona

O driver é baseado na arquitetura **PSCRIPT5** (PostScript) do Windows. Ao imprimir, o Spooler converte o documento para PostScript e entrega os bytes ao monitor (`meddrivemon.dll`). O monitor delega a conversão ao **MeddrivePrinterAgent** via named pipe — um processo leve que roda na sessão interativa do usuário e executa o **Ghostscript** com as credenciais de rede do usuário, permitindo salvar PDFs em pastas de rede (`\\servidor\pasta`).

Todas as configurações (caminho de saída, nome de arquivo, estratégia de conflito) são armazenadas no **Registry do Windows** e lidas pelo monitor em tempo de execução a cada job.

### Fluxo de um job de impressão

1. O Spooler carrega `meddrivemon.dll` e chama `InitializePrintMonitor2`.
2. O Spooler consulta as portas disponíveis — o monitor lê as subchaves de `Ports\` no registry.
3. Ao receber um job, o Spooler chama `OpenPort`. O monitor aloca um `PORT_CONTEXT` e lê as configurações do registry.
4. O monitor acumula os bytes PostScript em um arquivo temporário em `C:\Windows\Temp\`.
5. Ao finalizar o job (`EndDocPort`), o monitor localiza a sessão interativa via `WTSGetActiveConsoleSessionId()` e conecta ao named pipe `\\.\pipe\MeddrivePrinter_<sessionId>`.
6. O monitor envia uma struct `PrintJobMsg` com os ingredientes brutos: caminho do `.tmp` (PostScript), pasta de destino, template de nome (`OutputBaseName`), nome do documento e caminho do `gswin64c.exe`.
7. O `MeddrivePrinterAgent` (rodando na sessão do usuário, com credenciais de rede do mesmo) resolve o nome final do PDF — incluindo a varredura de numeração em pastas de rede — e chama `CreateProcessW` para executar o Ghostscript. O PDF é salvo no caminho resolvido.
8. O agente devolve uma struct `PrintJobResponse` com o `exitCode` e o caminho final do PDF pelo mesmo pipe. Se `OpenAfterGenerate` estiver ativo, o agente abre o PDF no visualizador padrão via `ShellExecuteW`.
9. O monitor recebe a resposta, deleta o arquivo `.tmp` e remove o job da fila do Spooler.
10. Se o agente não estiver rodando, o monitor exibe uma mensagem de erro na sessão do usuário via `WTSSendMessage` e cancela o job.

---

## Pré-requisitos

**Windows 10/11:**
- Ghostscript (bundled no instalador)
- PowerShell com permissão para executar scripts
- Execução como Administrador

**Windows 7 x64:**
- Ghostscript 9.56.1 (bundled no instalador, compatível com Win7)
- PowerShell 2.0+
- Execução como Administrador

**Windows Vista x64 SP2:**
- Vista SP2 obrigatório (build ≥ 6002)
- PowerShell 2.0 obrigatório (KB968930 — não vem instalado em todas as edições)
- Ghostscript 9.56.1 (bundled, mesmo bundle do Win7)
- Execução como Administrador

---

## Instalação

### Windows 10/11
Execute `MeddrivePrinter-Setup.exe` como administrador.

### Windows 7
Execute `MeddrivePrinter-Win7-Setup.exe` como administrador.

O instalador Win7 usa `install_helper.exe` (binário nativo C) em vez de PowerShell para registrar o monitor e o driver, contornando limitações do PS 2.0.

### Windows Vista SP2
Antes de instalar, certifique-se de que o **PowerShell 2.0** está instalado (KB968930 via Windows Update). O instalador verifica isso automaticamente e aborta com instruções se não estiver presente.

Execute `MeddrivePrinter-Vista-Setup.exe` como administrador.

> Se aparecer o erro "execução de scripts foi desabilitada", execute:
> ```powershell
> Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
> ```

Após a instalação, abra o **MedDriveManager** para criar perfis e impressoras.

---

## Gerenciamento

O **MedDriveManager** oferece três abas:

- **Perfis** — cria, edita, duplica e remove perfis de saída (caminho, nome de arquivo, estratégia de conflito)
- **Impressoras** — adiciona e remove impressoras virtuais vinculadas a perfis
- **Configurações** — *(em desenvolvimento)*

---

## Registry

As configurações ficam em:

```
HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\Meddrive Printer MONITOR\Ports\Meddrive Printer PORT <nome>
```

| Valor | Descrição | Exemplo |
|---|---|---|
| `OutputPath` | Pasta de destino dos PDFs | `C:\PDF\` |
| `OutputBaseName` | Template do nome de arquivo. Tokens: `{data}`, `{hora}`, `{documento}`, `{n}` / `{nn}` / `{nnn}` | `{documento}_{data}` |
| `GhostscriptPath` | Caminho do executável do Ghostscript | `C:\ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe` |
| `OverwriteFile` | `1` = sobrescrever · `0` = incrementar contador | `0` |
| `OpenAfterGenerate` | `1` = abrir o PDF no visualizador após gerar | `0` |
| `ChoosePath` | `1` = exibir diálogo "Salvar como" antes de converter | `0` |

---

## Build

```bash
bash build.sh
```

Requer: `x86_64-w64-mingw32-gcc`, `makensis` (NSIS ≥ 3.0).

Gera:
- `installer/win10-11/x64/Debug/MedDriveManager.exe`
- `MeddrivePrinter-Setup.exe`
- `MeddrivePrinter-Win7-Setup.exe`
- `MeddrivePrinter-Vista-Setup.exe`
