# Meddrive Printer v2.5
> Desenvolvido para a empresa **StachIt**.

Impressora virtual PDF para Windows que captura jobs de impressão e os converte automaticamente em arquivos PDF, salvando-os em uma pasta configurável.

Não exige interação do usuário após a instalação — basta instalar, criar um perfil de saída e adicionar uma impressora pelo **MedDriveManager**.

---

## Componentes

| Arquivo | Descrição |
|---|---|
| `meddrivemon.dll` | DLL do monitor de impressão — toda a lógica de interação com o Spooler e o Ghostscript |
| `MedDriveManager.exe` | Aplicativo gráfico de gerenciamento de perfis e impressoras |
| `MeddrivePrinter-Setup.exe` | Instalador para Windows 10/11 |
| `MeddrivePrinter-Win7-Setup.exe` | Instalador para Windows 7 x64 |

### Código-fonte da DLL

| Arquivo | Descrição |
|---|---|
| `src/monitor.c` | Implementação do monitor de impressão (MONITOR2 API) |
| `src/monitor.h` | Variáveis e estruturas compartilhadas |
| `src/monitor.def` | Exportações lidas pelo Spooler |

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

O driver é baseado na arquitetura **PSCRIPT5** (PostScript) do Windows. Ao imprimir, o Spooler converte o documento para PostScript e entrega os bytes ao monitor. O monitor então chama o **Ghostscript** para converter o PostScript em PDF, gravando o resultado no caminho configurado no perfil.

Todas as configurações (caminho de saída, nome de arquivo, estratégia de conflito) são armazenadas no **Registry do Windows** e lidas pelo monitor em tempo de execução a cada job.

### Fluxo de um job de impressão

1. O Spooler carrega `meddrivemon.dll` e chama `InitializePrintMonitor2`, preenchendo a estrutura `MONITOR2` com os ponteiros de função.
2. O Spooler consulta as portas disponíveis — o monitor lê as subchaves de `Ports\` no registry e retorna cada uma como porta virtual.
3. Ao receber um job, o Spooler chama `OpenPort`. O monitor aloca um `PORT_CONTEXT` e lê `OutputPath`, `OutputBaseName`, `GhostscriptPath` e `OverwriteFile` do registry.
4. O monitor cria um arquivo `.ps` temporário em `%TEMP%` e acumula os bytes PostScript entregues pelo Spooler.
5. Ao finalizar o job, o monitor fecha o arquivo, invoca o Ghostscript, aguarda a conclusão e deleta o temporário.
6. `Monitor_ClosePort` libera o `PORT_CONTEXT`.

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

---

## Instalação

### Windows 10/11
Execute `MeddrivePrinter-Setup.exe` como administrador.

### Windows 7
Execute `MeddrivePrinter-Win7-Setup.exe` como administrador.

O instalador Win7 usa `install_helper.exe` (binário nativo C) em vez de PowerShell para registrar o monitor e o driver, contornando limitações do PS 2.0.

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
| `OutputBaseName` | Padrão do nome de arquivo | `documento_{DATE}` |
| `GhostscriptPath` | Caminho do executável do Ghostscript | `C:\ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe` |
| `OverwriteFile` | `1` = sobrescrever · `0` = incrementar cópias | `0` |

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
