# Meddrive Printer — Documentação Técnica
**v2.5 · Junho 2026**

---

## 1. Visão Geral

Impressora virtual PDF para Windows que captura jobs de impressão e os converte automaticamente em arquivos PDF, salvando-os em uma pasta configurável. Não exige interação do usuário durante a impressão — basta selecionar "Meddrive Printer" e imprimir normalmente.

**Objetivos:**
- Instalar como impressora nativa no Windows (aparece no Painel de Controle)
- Interceptar o PostScript gerado pelo driver PSCRIPT5
- Converter automaticamente para PDF via Ghostscript
- Salvar na pasta configurada no perfil
- Funcionar em máquinas de terceiros sem controle de boot ou assinatura de driver
- Suporte a múltiplos perfis e impressoras gerenciados pela GUI

---

## 2. Contexto e Motivação

O projeto surgiu da necessidade de uma impressora virtual PDF leve, controlável e implantável em máquinas de terceiros sem burocracia de assinatura de driver ou instaladores gigantes.

### Alternativas descartadas

| Alternativa | Motivo do descarte |
|---|---|
| Redmon + Ghostscript | Projeto abandonado, incompatível com Windows 10/11 |
| PDFCreator / CutePDF | Instaladores pesados, licenças restritivas, menos controle |
| Microsoft Print to PDF | Não permite configurar pasta de destino automaticamente |
| pdfmon | Monitor antigo, não mantido |
| PowerShell + FileSystemWatcher | FileSystemWatcher não é confiável como serviço de interceptação de spooler |
| Microsoft PS Class Driver | Bloqueado pelo Windows 10/11 quando usado com port monitors de terceiros (Event ID 242) |
| Driver V4 próprio | Exige assinatura digital — inviável para máquinas de terceiros |

### Por que uma DLL de monitor de porta

O Windows Print Spooler foi projetado para que monitores de porta customizados sejam DLLs carregadas diretamente no processo do spooler. Isso garante que o `WritePort` receba os bytes do PostScript em tempo real, sem polling de arquivos e sem serviços externos. É a forma oficial e suportada pela Microsoft de interceptar impressão.

---

## 3. Arquitetura

### 3.1 Componentes

| Camada | Componente | Responsabilidade |
|---|---|---|
| Driver | PSCRIPT5.DLL (Meddrive Printer DRIVER) | Converte o documento em PostScript |
| PPD | MEDDRIVE.PPD | Descreve as capacidades da impressora para o PSCRIPT5 |
| Monitor | meddrivemon.dll (Meddrive Printer MONITOR) | Recebe os bytes PS e aciona o Ghostscript |
| Porta | Meddrive Printer PORT `<nome>` | Ponto de conexão entre driver e monitor — uma por perfil |
| Impressora | Meddrive Printer `<nome>` | Objeto visível ao usuário no Windows |
| Conversor | Ghostscript (gswin64c.exe) | Converte PostScript → PDF |
| Configuração | Registry (`HKLM\...\Monitors\Meddrive Printer MONITOR\Ports\`) | OutputPath, OutputBaseName, OverwriteFile, GhostscriptPath por porta |
| GUI | MedDriveManager.exe | Gerenciamento de perfis e impressoras |

### 3.2 Fluxo de impressão

```
Usuário clica em Imprimir (qualquer app Win32)
        ↓
Windows Print Spooler
        ↓
Driver PSCRIPT5 (Meddrive Printer DRIVER)
converte o documento em PostScript
        ↓
meddrivemon.dll — WritePort()
acumula os bytes PS em arquivo temporário (.ps)
        ↓
EndDocPort()
resolve o nome do arquivo de saída (numeração ou sobrescrita)
chama Ghostscript via CreateProcess():
  gswin64c.exe -dBATCH -dNOPAUSE -sDEVICE=pdfwrite
               -sOutputFile=saida.pdf arquivo.ps
        ↓
PDF salvo na pasta configurada
arquivo .ps temporário removido
```

---

## 4. Perfis e Configuração por Porta

Cada impressora cadastrada via MedDriveManager é associada a um **perfil**, que define como os PDFs são gerados.

### 4.1 Campos do perfil

| Campo | Descrição |
|---|---|
| Nome do perfil | Identificador único usado para nomear a porta no registry |
| Nome do arquivo | Padrão do nome do arquivo (suporta tokens) |
| Pasta de destino | Diretório onde os PDFs são salvos |
| Abrir após gerar | Abre o PDF no visualizador padrão após a conversão |
| Caso o arquivo já exista | `Sobrescrever` ou `Incrementar cópias` |

### 4.2 Tokens no nome do arquivo

| Token | Valor gerado |
|---|---|
| `{n}` | Contador simples (1, 2, 3…) |
| `{nnn}` | Contador 3 dígitos (001, 002…) |
| `{data}` | Data atual (AAAA-MM-DD) |
| `{hora}` | Hora atual (HH-MM-SS) |
| `{documento}` | Nome do documento enviado para impressão |

### 4.3 Estratégia de conflito

Quando `OverwriteFile = 0` (incrementar):
- O DLL escaneia a pasta com `FindFirstFileW`/`FindNextFileW`
- Extrai o maior N existente e usa `maxN + 1`
- Resultado: `relatorio-1.pdf`, `relatorio-2.pdf`, etc.

Quando `OverwriteFile = 1` (sobrescrever):
- O arquivo de saída tem nome fixo (sem sufixo numérico)
- Arquivo anterior é substituído

### 4.4 Registry da porta

```
HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\Meddrive Printer MONITOR\Ports\Meddrive Printer PORT <nome>
    OutputPath      = "C:\PDF\"
    OutputBaseName  = "relatorio"
    GhostscriptPath = "C:\ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe"
    OverwriteFile   = 0   (DWORD — 0=incrementar, 1=sobrescrever)
```

> **Retrocompatibilidade:** portas criadas antes da v1.5 sem `OutputBaseName` usam `"saida"` como fallback.

### 4.5 Regra de nomenclatura da porta

O nome da porta é derivado automaticamente do nome do perfil:

```
Perfil: "Triton"  →  Porta: "Meddrive Printer PORT Triton"
```

---

## 5. Nomenclatura e Localização

| Item | Nome | Localização no Windows |
|---|---|---|
| DLL | `meddrivemon.dll` | `C:\Windows\System32\` |
| Monitor | `Meddrive Printer MONITOR` | `HKLM\...\Print\Monitors\` |
| Porta | `Meddrive Printer PORT <nome>` | `HKLM\...\Print\Monitors\...\Ports\` |
| Driver | `Meddrive Printer DRIVER` | `HKLM\...\Print\Environments\Windows x64\Drivers\Version-3\` |
| PPD | `MEDDRIVE.PPD` | `C:\Windows\System32\spool\drivers\x64\3\` |
| GUI | `MedDriveManager.exe` | `C:\ProgramData\Meddrive Printer\` |
| Scripts | `conf\*.ps1` | `C:\ProgramData\Meddrive Printer\conf\` |
| Log da DLL | `meddrivemon_init.log` | `C:\Windows\Temp\` |
| Log do instalador | `meddrive_install.log` | `C:\Windows\Temp\` |

---

## 6. Decisões Técnicas

### 6.1 Print Monitor 2, não Monitor 1

A DLL exporta `InitializePrintMonitor2` e implementa a struct `MONITOR2` (winsplp.h). A versão 1 (`InitializePrintMonitor`) é legada e não suporta o contexto de monitor (`hMonitor`) necessário para múltiplas instâncias simultâneas.

### 6.2 PSCRIPT5 como driver, não MS PS Class Driver

O Windows 10/11 bloqueia drivers "inbox" da Microsoft quando usados com port monitors de terceiros (Event ID 242). A solução foi registrar um driver próprio (`Meddrive Printer DRIVER`) que aponta para os arquivos do PSCRIPT5 já presentes em System32. Como o nome não é "Microsoft PS Class Driver", o bloqueio não se aplica.

### 6.3 Registro do driver via registry, não via AddPrinterDriver

No Windows 10/11, escrever as chaves diretamente em `HKLM\...\Drivers\Version-3\` contorna a validação de assinatura de INF. No **Windows 7**, essa abordagem faz o spooler enumerar o driver mas falha ao carregá-lo (`ERROR_INVALID_HANDLE = 6`). A solução para Win7 é usar `AddPrinterDriverExW` via P/Invoke com `DRIVER_INFO_2` e flags `APD_COPY_ALL_FILES | APD_COPY_FROM_DIRECTORY`.

### 6.4 AddPortExW em vez de AddPort

`AddPort` falha para monitores customizados no Windows 10/11 (`ERROR_INVALID_DATA = 13`). `AddPortExW` chama `pfnAddPortEx` diretamente no monitor, contornando a validação RPC. A DLL implementa `Monitor_AddPortEx` (retorna `TRUE`).

### 6.5 Restart-Service Spooler, não Start-Sleep

O spooler enumera drivers registrados via registry somente na inicialização. O instalador usa `Restart-Service -Name Spooler -Force` e só continua após confirmar que o driver está visível via `Get-PrinterDriver`.

### 6.6 PPD sem *Protocols: PJL

Duas entradas causavam travamento completo do spooler em impressoras virtuais:
- `*Protocols: PJL` — tentava comunicação bidirecional, `ReadPort` bloqueava indefinidamente
- `*?TTRasterizer` — enviava PS ao dispositivo esperando resposta via `ReadPort`

Ambas foram removidas. `*TTRasterizer: Type42` permanece como declaração estática.

### 6.7 Instalador .exe com NSIS

As máquinas alvo são de terceiros — não há controle sobre configurações de boot. O instalador usa `RequestExecutionLevel admin` (manifesto UAC) para ganhar elevação. Gerado a partir do Linux via `makensis`.

### 6.8 install_helper.exe no Win7

O Win7 não possui o módulo `PrintManagement` do PowerShell. Para registrar o driver, foi criado um helper nativo em C (`install_helper.c`) que usa `AddPrinterDriverExW` diretamente, sem depender de cmdlets PS. O NSIS do instalador Win7 extrai e executa esse helper como etapa de instalação.

### 6.9 Scripts em conf/

Todos os scripts de gerenciamento de runtime (`add-printer.ps1`, `remove-printer.ps1`, etc.) são instalados em `C:\ProgramData\Meddrive Printer\conf\`. O `MedDriveManager.exe` constrói o caminho completo em runtime com `GetModuleFileNameW` + `conf\<script>.ps1`, garantindo que funcione de qualquer diretório.

### 6.10 Limitação aceita: Edge Ctrl+P não mostra preview

O diálogo nativo do Edge usa a Print Ticket API (XPS) e chama `PTGetPrintCapabilities` antes de mostrar o preview. Para o nosso driver, essa chamada retorna `E_FAIL (0x80004005)`.

**Decisão:** aceitar a limitação. `Ctrl+Shift+P` (diálogo Win32 do sistema) funciona normalmente, assim como qualquer outro aplicativo Win32.

---

## 7. Bugs Resolvidos

| Erro | Causa raiz | Correção |
|---|---|---|
| `ERROR_INVALID_PRINTER_NAME (1801)` em `AddPrinterW` | Port não estava na lista validada do spooler | `AddPortExW` no install.ps1 + `pfnAddPortEx` na DLL |
| `ERROR_INVALID_DATA (13)` em `EnumPorts` | Falha de validação RPC em monitors customizados | Removida verificação de `EnumPorts` do install.ps1 |
| Driver não reconhecido após registro | Spooler não recarrega drivers sem restart | `Restart-Service Spooler -Force` após gravar as chaves |
| PSCRIPT5 abandona job sem PPD | PPD não copiado quando o spooler reiniciava | PPD copiado para `drivers\x64\3\` antes do restart |
| Edge `Ctrl+P` travava indefinidamente | `*Protocols: PJL` e `*?TTRasterizer` bloqueavam `ReadPort` | Removidos do MEDDRIVE.PPD |
| Win7: `ERROR_INVALID_HANDLE (6)` em `AddPrinter` | Registro direto no registry não carrega driver no Win7 | `AddPrinterDriverExW` via install_helper.exe |

---

## 8. Estrutura do Projeto

```
med-driver/
├── src/
│   ├── monitor.c          — implementação do port monitor (MONITOR2)
│   ├── monitor.h          — defines: MONITOR_NAME, PORT_NAME, PORT_CONTEXT
│   └── monitor.def        — exports da DLL
├── gui-native/
│   └── MedDriveManager/
│       ├── src/
│       │   ├── mainwnd.c          — janela principal, orquestrador de eventos
│       │   ├── store.c            — leitura/escrita do registry (perfis e portas)
│       │   ├── dlg_profile.c      — dialog de criação/edição de perfis
│       │   ├── dlg_progress.c     — dialog de progresso (execução dos scripts PS)
│       │   └── ui/
│       │       ├── theme.c/.h     — cores, fontes, ícones
│       │       ├── titlebar.c/.h  — barra de título customizada
│       │       ├── navbar.c/.h    — navegação (Perfis / Impressoras / Configurações)
│       │       ├── listview.c/.h  — listview owner-draw
│       │       ├── statusbar.c/.h — barra de status inferior
│       │       └── buttons.c/.h   — botões de ação
│       └── res/
│           ├── app.rc             — recursos Win32 (dialogs, ícones)
│           └── resource.h         — IDs de controles e recursos
├── installer/
│   ├── win10-11/
│   │   ├── setup.nsi              — script NSIS Win10/11
│   │   └── conf/                  — scripts de gerenciamento (instalados em ProgramData)
│   │       ├── install.ps1
│   │       ├── add-printer.ps1
│   │       ├── remove-printer.ps1
│   │       ├── create-profile.ps1
│   │       ├── edit-profile.ps1
│   │       ├── remove-profile.ps1
│   │       └── edit-printer.ps1
│   └── win7/
│       ├── setup.nsi              — script NSIS Win7
│       ├── install_helper.c       — helper nativo C para registro do driver
│       ├── install_helper.exe     — binário compilado do helper
│       └── conf/                  — mesmos scripts PS (compatíveis com PS 2.0)
├── tests/
│   ├── diagnostico.ps1            — verifica DLL, registry, spooler, driver, impressora
│   └── test-ptcap.ps1             — testa PTGetPrintCapabilities
├── docs/
│   ├── impressora_virtual_documentacao.md  — este arquivo
│   ├── win7-particularidades.md            — detalhes do suporte Win7
│   ├── Meddrive_UI_Refatoracao.md          — spec visual do MedDriveManager
│   └── ui_refactoring_decisions.md         — decisões arquiteturais da UI
├── build.sh                       — script de build (DLL + GUI + instaladores)
├── meddrivemon.dll                 — DLL compilada (artefato de release)
├── MeddrivePrinter-Setup.exe       — instalador Win10/11
└── MeddrivePrinter-Win7-Setup.exe  — instalador Win7
```

---

## 9. Build

### Pré-requisitos (Linux)

```bash
sudo dnf install mingw64-gcc        # cross-compilador Windows x64
sudo dnf install nsis               # NSIS para gerar os instaladores
sudo dnf install imagemagick        # conversão PNG → ICO (ícones da GUI)
```

### Compilar

```bash
bash build.sh
```

Gera:
- `meddrivemon.dll` — DLL cross-compilada (x86_64-w64-mingw32-gcc)
- `installer/win10-11/x64/Debug/MedDriveManager.exe` — GUI
- `MeddrivePrinter-Setup.exe` — instalador Win10/11
- `MeddrivePrinter-Win7-Setup.exe` — instalador Win7

A DLL é linkada com `-static-libgcc` para não depender da runtime GCC no Windows.

---

## 10. Fluxo de instalação

### Win10/11 — via install.ps1

1. Para o Spooler (`Stop-Service Spooler -Force`)
2. Copia `meddrivemon.dll` → `C:\Windows\System32\`
3. Registra o monitor no registry
4. Reinicia o Spooler
5. Registra o driver `Meddrive Printer DRIVER` via registry (PSCRIPT5)
6. Copia `MEDDRIVE.PPD` → `drivers\x64\3\`
7. Reinicia o Spooler para enumerar o driver
8. Copia `MedDriveManager.exe` e scripts `conf\` → `C:\ProgramData\Meddrive Printer\`
9. Cria atalho no menu Iniciar

### Win7 — via install.ps1 + install_helper.exe

Igual ao Win10/11, exceto:
- O registro do driver é feito por `install_helper.exe` com `AddPrinterDriverExW`
- Ghostscript 9.56.1 é usado (versões 10.x não são compatíveis com Win7)
- DLLs do runtime Visual C++ e UCRT são bundladas em `Ghostscript\bin\`

---

## 11. Diagnóstico

### Logs

| Log | Caminho | Conteúdo |
|---|---|---|
| DLL | `C:\Windows\Temp\meddrivemon_init.log` | Chamadas do spooler: InitializePrintMonitor2, OpenPort, WritePort, EndDocPort |
| Instalador Win10/11 | `C:\Windows\Temp\meddrive_ps_install.log` | Execução completa do install.ps1 (Start-Transcript) |
| Instalador Win7 | `C:\Windows\Temp\meddrive_install.log` | Execução do install_helper.exe + etapas PS |

### Script de diagnóstico

```powershell
.\tests\diagnostico.ps1
```

Verifica: DLL, registry do monitor, driver, PPD, spooler, impressora.

### Verificação rápida

```powershell
Get-Printer | Where-Object { $_.PortName -like "Meddrive*" } | Select-Object Name, DriverName, PortName
```

---

## 12. Compatibilidade

| Sistema | Suporte | Instalador |
|---|---|---|
| Windows 11 x64 | ✓ Testado | `MeddrivePrinter-Setup.exe` |
| Windows 10 x64 | ✓ Testado | `MeddrivePrinter-Setup.exe` |
| Windows 7 x64 | ✓ Suportado | `MeddrivePrinter-Win7-Setup.exe` |
| Windows Vista x64 **SP2** | ✓ Suportado (requer PS 2.0) | `MeddrivePrinter-Vista-Setup.exe` |
| Windows Vista x64 RTM/SP1 | ✗ UCRT não disponível | — |
| Windows x86 (32-bit) | ✗ DLL 64-bit incompatível | — |

**Ghostscript:**
- Win10/11: 10.x (bundlado no instalador)
- Win7 / Vista SP2: 9.56.1 + DLLs UCRT bundladas (versões 10.x não são compatíveis com Win7/Vista)

Ver `docs/winvista-particularidades.md` para detalhes do suporte ao Vista.
