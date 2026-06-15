# Meddrive Printer — Documentação Técnica
**v1.2 · Junho 2026**

---

## 1. Visão Geral

Impressora virtual PDF para Windows que captura jobs de impressão e os converte automaticamente em arquivos PDF, salvando-os em uma pasta configurável. Não exige interação do usuário após a instalação — basta selecionar "Meddrive Printer" e imprimir normalmente.

**Objetivos:**
- Instalar como impressora nativa no Windows (aparece no Painel de Controle)
- Interceptar o PostScript gerado pelo driver PSCRIPT5
- Converter automaticamente para PDF via Ghostscript
- Salvar na pasta configurada durante a instalação
- Funcionar em máquinas de terceiros sem controle de boot ou assinatura de driver

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
| PowerShell + FileSystemWatcher | Abordagem inicial descartada: FileSystemWatcher não é confiável como serviço de interceptação de spooler |
| Microsoft PS Class Driver | Bloqueado pelo Windows 10/11 quando usado com port monitors de terceiros (Event ID 242 no PrintService) |
| Driver V4 próprio | Exige assinatura digital — inviável para máquinas de terceiros sem test signing |

### Por que uma DLL de monitor de porta

O Windows Print Spooler foi projetado para que monitores de porta customizados sejam DLLs carregadas diretamente no processo do spooler. Isso garante que o `WritePort` receba os bytes do PostScript em tempo real, sem polling de arquivos e sem serviços externos. É a forma oficial e suportada pela Microsoft de interceptar impressão.

---

## 3. Arquitetura Real

### 3.1 Fluxo de impressão

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
chama Ghostscript via CreateProcess():
  gswin64c.exe -dBATCH -dNOPAUSE -sDEVICE=pdfwrite
               -sOutputFile=saida.pdf arquivo.ps
        ↓
PDF salvo na pasta configurada
arquivo .ps temporário removido
```

### 3.2 Componentes

| Camada | Componente | Responsabilidade |
|---|---|---|
| Driver | PSCRIPT5.DLL (Meddrive Printer DRIVER) | Converte o documento em PostScript |
| PPD | MEDDRIVE.PPD | Descreve as capacidades da impressora para o PSCRIPT5 |
| Monitor | meddrivemon.dll (Meddrive Printer MONITOR) | Recebe os bytes PS e aciona o Ghostscript |
| Porta | Meddrive Printer PORT | Ponto de conexão entre o driver e o monitor |
| Impressora | Meddrive Printer | Objeto visível ao usuário no Windows |
| Conversor | Ghostscript (gswin64c.exe) | Converte PostScript → PDF |
| Configuração | Registry (`HKLM\...\Monitors\Meddrive Printer MONITOR\Ports\Meddrive Printer PORT`) | OutputPath e GhostscriptPath por porta |

---

## 4. Nomenclatura e Configuração

| Item | Nome | Localização no Windows |
|---|---|---|
| DLL | `meddrivemon.dll` | `C:\Windows\System32\` |
| Monitor | `Meddrive Printer MONITOR` | `HKLM\...\Print\Monitors\` |
| Porta | `Meddrive Printer PORT` | `HKLM\...\Print\Monitors\...\Ports\` |
| Driver | `Meddrive Printer DRIVER` | `HKLM\...\Print\Environments\Windows x64\Drivers\Version-3\` |
| PPD | `MEDDRIVE.PPD` | `C:\Windows\System32\spool\drivers\x64\3\` |
| Impressora | `Meddrive Printer` | Painel de Controle → Impressoras |
| Log da DLL | `meddrivemon_init.log` | `C:\Windows\Temp\` |

### Registry da porta (configuração por instalação)

```
HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\Meddrive Printer MONITOR\Ports\Meddrive Printer PORT
    OutputPath      = "C:\Users\...\PDF\saida.pdf"
    GhostscriptPath = "C:\Program Files\gs\gs10.07.1\bin\gswin64c.exe"
```

### Registry do driver

```
HKLM\SYSTEM\CurrentControlSet\Control\Print\Environments\Windows x64\Drivers\Version-3\Meddrive Printer DRIVER
    Driver                  = "PSCRIPT5.DLL"
    Configuration File      = "PS5UI.DLL"
    Data File               = "PSCRIPT.NTF"
    Help File               = "PSCRIPT.HLP"
    Dependent Files         = ["MEDDRIVE.PPD", ""]
    PrinterDriverAttributes = 2   (PRINTER_DRIVER_XPS)
    Version                 = 3
    Driver Version          = 3
```

---

## 5. Decisões Técnicas

### 5.1 Print Monitor 2, não Monitor 1

A DLL exporta `InitializePrintMonitor2` e implementa a struct `MONITOR2` (winsplp.h). A versão 1 (`InitializePrintMonitor`) é legada e não suporta o contexto de monitor (`hMonitor`) necessário para múltiplas instâncias simultâneas. O Monitor 2 é o padrão desde Windows 2000 e é o que o spooler moderno espera.

### 5.2 PSCRIPT5 como driver, não MS PS Class Driver

O Windows 10/11 bloqueia drivers "inbox" da Microsoft (Microsoft PS Class Driver, Microsoft Print to PDF, Microsoft XPS Document Writer) quando usados com port monitors de terceiros. O bloqueio aparece como Event ID 242 no log `Microsoft-Windows-PrintService/Admin` e o spooler recusa o job silenciosamente.

A solução foi registrar um driver **próprio** (`Meddrive Printer DRIVER`) que aponta para os arquivos do PSCRIPT5 já presentes em System32 (`PSCRIPT5.DLL`, `PS5UI.DLL`, `PSCRIPT.NTF`, `PSCRIPT.HLP`). Como o nome do driver não é "Microsoft PS Class Driver", o bloqueio não se aplica. O registro é feito diretamente no registry, sem INF ou assinatura digital.

### 5.3 Registro do driver via registry, não via AddPrinterDriver

`AddPrinterDriver` com um INF sem assinatura é bloqueado pelo Windows 10/11 (política de assinatura de driver). Escrever as chaves diretamente em `HKLM\...\Drivers\Version-3\` contorna essa validação — o spooler lê as chaves no próximo restart sem exigir que o INF seja assinado. O PPD precisa estar copiado para `drivers\x64\3\` **antes** do restart para que o PSCRIPT5 o encontre ao inicializar.

### 5.4 AddPortExW em vez de AddPort

`AddPort` falha para monitores customizados no Windows 10/11 porque o cliente RPC não consegue validar ponteiros de estruturas do monitor de terceiros (retorna `ERROR_INVALID_DATA = 13`). `AddPortExW` usa um caminho diferente no spooler — chama `pfnAddPortEx` diretamente no monitor — e não passa por essa validação. O `install.ps1` usa P/Invoke direto em `winspool.drv` para chamar `AddPortExW`. A DLL implementa `Monitor_AddPortEx` (retorna `TRUE`) para satisfazer o spooler.

### 5.5 Restart-Service Spooler, não Start-Sleep

O spooler enumera drivers registrados via registry **somente na inicialização**. Gravar as chaves com o spooler em execução não faz o driver aparecer — é preciso reiniciar o serviço. Um simples `Start-Sleep` não resolve. O `install.ps1` usa `Restart-Service -Name Spooler -Force` e só continua após confirmar que o driver está visível via `Get-PrinterDriver`.

### 5.6 PPD sem *Protocols: PJL e sem *?TTRasterizer query

O PSCRIPT5 usa o PPD para saber as capacidades da impressora. Duas entradas causavam travamento completo:

- `*Protocols: PJL` — declarava suporte a comunicação bidirecional PJL. O PSCRIPT5 (e o caminho Print Ticket do Edge) tentava consultar o dispositivo e ficava aguardando resposta via `ReadPort`. Como a impressora é virtual, `ReadPort` sempre retorna 0 bytes → travamento indefinido.
- `*?TTRasterizer` com código PostScript — enviava PS ao dispositivo para descobrir o tipo de rasterizador. Mesmo problema: `ReadPort` bloqueava esperando a resposta.

Ambas foram removidas. O `*TTRasterizer: Type42` permanece como declaração estática (sem envio ao dispositivo).

### 5.7 Instalador .exe com NSIS e elevação UAC

As máquinas alvo são de **terceiros** — não há controle sobre configurações de boot (`bcdedit /set testsigning on` é inviável). A instalação exige admin para escrever em `HKLM` e copiar a DLL para `System32`. A solução padrão para esse cenário é um instalador `.exe` com manifesto de UAC (`RequestExecutionLevel admin`), que exibe o prompt "Deseja permitir alterações?" e ganha elevação. O usuário só precisa de uma conta com privilégio de administrador local — padrão em PCs domésticos. O NSIS gera o executável a partir do Linux via `makensis`.

A estrutura de extração do instalador espelha o repositório propositalmente:
```
$TEMP\MeddrivePrinter\
    meddrivemon.dll          ← raiz
    installer\
        install.ps1          ← subpasta
        MEDDRIVE.PPD
```
Isso é necessário porque `install.ps1` localiza a DLL com `$ScriptDir\..\meddrivemon.dll`. Se todos os arquivos fossem extraídos no mesmo diretório, o script buscaria a DLL um nível acima do `$TEMP` e falharia.

### 5.8 Limitação aceita: Edge Ctrl+P não mostra preview

O diálogo nativo do Edge (`Ctrl+P`) usa a Print Ticket API (XPS) e chama `PTGetPrintCapabilities` antes de mostrar o preview. Para o nosso driver, essa chamada retorna `E_FAIL (0x80004005)`. O PSCRIPT5 com PPD não gera o XML de PrintCapabilities para o caminho XPS sem mapeamentos `MSPrintSchemaKeywordMap` — e mesmo com esses mapeamentos, o resultado pode não mudar porque o PSCRIPT5 com PPD está na fronteira do suporte ao Print Schema.

A correção definitiva seria um driver V4 próprio com pipeline XPS→PS (`MSxpsPS`), que tem suporte nativo a Print Schema. Isso exige assinatura digital (WHQL ou EV Code Sign) — inviável para máquinas de terceiros.

**Decisão:** aceitar a limitação. O `Ctrl+Shift+P` (diálogo Win32 do sistema) funciona normalmente, assim como qualquer outro aplicativo Win32.

---

## 6. Bugs Resolvidos

| Erro | Causa raiz | Correção |
|---|---|---|
| `ERROR_INVALID_PRINTER_NAME (1801)` em `AddPrinterW` | Port não estava na lista validada do spooler | `AddPortExW` no install.ps1 + `pfnAddPortEx` na DLL |
| `ERROR_INVALID_DATA (13)` em `EnumPorts` | Falha de validação RPC em monitors customizados | Removida verificação de `EnumPorts` do install.ps1 |
| Driver não reconhecido após registro | Spooler não recarrega drivers sem restart | `Restart-Service Spooler -Force` após gravar as chaves |
| PSCRIPT5 abandona job sem PPD | PPD não estava copiado quando o spooler reiniciava | PPD copiado para `drivers\x64\3\` **antes** do restart |
| Edge `Ctrl+P` travava indefinidamente | `*Protocols: PJL` e `*?TTRasterizer` query faziam `ReadPort` bloquear | Removidos do MEDPDF.PPD |

---

## 7. Estrutura do Projeto

```
med-driver/
├── src/
│   ├── monitor.c          — implementação completa do port monitor (MONITOR2)
│   ├── monitor.h          — defines: MONITOR_NAME, PORT_NAME, PORT_CONTEXT
│   └── monitor.def        — exports da DLL: InitializePrintMonitor2
├── installer/
│   ├── install.ps1        — script de instalação (requer admin)
│   ├── MEDDRIVE.PPD       — PostScript Printer Description para o PSCRIPT5
│   ├── MEDDRIVE.INF       — INF do driver (referência, não usado na instalação atual)
│   └── setup.nsi          — script NSIS para gerar o MeddrivePrinter-Setup.exe
├── tests/
│   ├── diagnostico.ps1    — diagnóstico completo: DLL, registry, spooler, driver, impressora
│   └── test-ptcap.ps1     — testa PTGetPrintCapabilities via prntvpt.dll
├── docs/
│   └── impressora_virtual_documentacao.md  — este arquivo
├── meddrivemon.dll        — DLL compilada (artefato de release)
├── MeddrivePrinter-Setup.exe  — instalador Windows (artefato de release)
└── Makefile               — build: `make` (DLL) · `make installer` (EXE)
```

---

## 8. Build

### Pré-requisitos (Linux)

```bash
sudo dnf install mingw64-gcc       # cross-compilador Windows x64
sudo dnf install mingw-nsis-base mingw32-nsis  # NSIS para gerar o EXE
```

### Compilar

```bash
make            # gera meddrivemon.dll
make installer  # gera MeddrivePrinter-Setup.exe
make clean      # remove objetos e DLL
```

A DLL é cross-compilada no Linux para Windows x64:
```
x86_64-w64-mingw32-gcc -shared -static-libgcc -lkernel32 -ladvapi32
```

`-static-libgcc` embute a runtime GCC na DLL, eliminando dependência de libgcc no Windows.

---

## 9. Instalação (fluxo real)

O `MeddrivePrinter-Setup.exe` executa os seguintes passos via `install.ps1`:

1. **Para o Spooler** (`Stop-Service Spooler -Force`) para liberar arquivos
2. **Copia** `meddrivemon.dll` para `C:\Windows\System32\`
3. **Registra o monitor** em `HKLM\...\Print\Monitors\Meddrive Printer MONITOR`
4. **Configura a porta** com `OutputPath` e `GhostscriptPath` no registry
5. **Inicia o Spooler** e aguarda carregamento
6. **Instala "Generic / Text Only"** se ausente (confirma que o módulo PrintManagement está disponível)
7. **Registra o driver** `Meddrive Printer DRIVER` via chaves de registry (PSCRIPT5)
8. **Copia o PPD** `MEDDRIVE.PPD` para `drivers\x64\3\`
9. **Reinicia o Spooler** para que enumere o novo driver
10. **Registra a porta** via `AddPortExW` (P/Invoke em `winspool.drv`)
11. **Cria a impressora** via `AddPrinterW` (P/Invoke em `winspool.drv`) com `pPrintProcessor="winprint"` e `pDatatype="RAW"`

---

## 10. Diagnóstico

### Log da DLL

```
C:\Windows\Temp\meddrivemon_init.log
```

Contém todas as chamadas do spooler: `InitializePrintMonitor2`, `OpenPort`, `StartDocPort`, `WritePort`, `EndDocPort`.

### Script de diagnóstico

```powershell
.\tests\diagnostico.ps1
```

Verifica: presença da DLL, arquitetura (x64), bloqueio de zona, permissões, registry do monitor e porta, status do spooler, resultado de `AddPrinter`, registry do driver com `PrinterDriverAttributes` e PPD, presença da impressora.

### Verificação rápida

```powershell
Get-Printer -Name "Meddrive Printer" | Select-Object Name, DriverName, PortName
```

---

## 11. Compatibilidade

| Sistema | Suporte |
|---|---|
| Windows 10 x64 | ✓ Testado |
| Windows 11 x64 | ✓ Testado |
| Windows 7 x64 | ⚠ DLL compatível, mas `install.ps1` falha sem RSAT (módulo PrintManagement ausente) |
| Windows x86 (32-bit) | ✗ DLL 64-bit incompatível |

**Ghostscript:** versões 9.50 a 10.07.x detectadas automaticamente pelo instalador.
