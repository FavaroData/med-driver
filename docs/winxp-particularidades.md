# Particularidades do suporte ao Windows XP

## Pré-requisitos

| Requisito | Versão mínima | Como obter |
|---|---|---|
| Windows XP x86 | **SP2** (CurrentVersion 5.1) | Windows Update |
| PowerShell | **2.0** (só para o MedDriveManager) | Windows Update, KB968930 |

O instalador verifica em `.onInit` a `CurrentVersion` igual a `5.1` e o `CSDVersion` igual a `Service Pack 2` ou `Service Pack 3`. Se não bater, aborta com mensagem.

O PowerShell 2.0 não é necessário para a instalação. O `install_helper_xp.exe` faz tudo por API nativa do Windows. O PS 2.0 só é usado depois, pelo MedDriveManager, para criar e editar perfis e impressoras.

---

## Build dedicado x86 (i686)

O XP roda em 32 bits. Todos os binários são compilados como i686 com `i686-w64-mingw32-gcc`, num script separado (`build-xp.sh`) que não toca no build x64:

| Binário | Origem |
|---|---|
| `meddrivemon_xp.dll` | `src/monitor.c` |
| `MeddrivePrinterAgent_xp.exe` | `src/agent/MeddrivePrinterAgent.c` |
| `MedDriveManager.exe` (x86) | `src/gui-native/MedDriveManager/` |
| `install_helper_xp.exe` | `installer/winxp/install_helper_xp.c` |

As diferenças de comportamento entre o XP e as outras versões ficam atrás da macro `MEDDRIVE_XP`, definida só no `build-xp.sh`. Sem ela, o código compila igual ao x64/Win7/Vista.

---

## Funções seguras da CRT ausentes no msvcrt do XP

O `msvcrt.dll` do XP é uma versão antiga que não exporta as funções seguras da CRT (`wcscpy_s`, `wcsncpy_s`, `wcsncat_s`, `_snwprintf_s`). O MinGW as declara como import do `msvcrt.dll`, então o binário nem carrega:

```
Ponto de entrada não encontrado: wcsncpy_s em msvcrt.dll
```

A solução é o header `src/xp_secure.h`, com implementações inline dessas funções, force-incluído (`-include`) nas três compilações i686 (monitor, agente, manager).

Esse header precisa entrar nas três. Se faltar no `meddrivemon_xp.dll`, a DLL do monitor não carrega, o spooler não chama `EnumPorts`, nenhuma porta é reconhecida, e o `AddPrinter` falha com Win32 erro `1796` (`ERROR_UNKNOWN_PORT`).

---

## bcrypt.dll é Vista+

O hash de senha do MedDriveManager usava `bcrypt.dll` (SHA-256), que só existe a partir do Vista. No XP a aplicação nem inicia.

Sob `MEDDRIVE_XP`, o `dlg_password.c` calcula o mesmo SHA-256 pela CryptoAPI antiga (`advapi32`), com `PROV_RSA_AES` e `CALG_SHA_256` (presentes no XP SP3, e no SP2 pelo provedor "Prototype"). O digest é idêntico ao do bcrypt, então a senha continua compatível. No link, `-lbcrypt` vira `-ladvapi32`.

---

## dwmapi.dll é Vista+

O `mainwnd.c` chamava `DwmSetWindowAttribute` e `DwmExtendFrameIntoClientArea`, ligadas estaticamente por `dwmapi.lib`. No XP essa DLL não existe e o EXE não carrega.

Passou a carregar o `dwmapi.dll` em runtime com `LoadLibrary`, e só chama as funções se elas existirem. No Vista+ o efeito visual é o mesmo de antes. No XP o bloco é ignorado. Vale para todos os builds, não só o do XP.

---

## Caminhos: %ProgramData% não existe no XP

O `%ProgramData%` só existe a partir do Vista. No XP o equivalente é `%ALLUSERSPROFILE%\Application Data`. O `ExpandEnvironmentStrings` no XP deixa `%ProgramData%` literal, então a aplicação não acharia o `settings.ini` nem o Ghostscript.

Sob `MEDDRIVE_XP`, os caminhos mudam:

| Item | Win7/Vista/10 | XP |
|---|---|---|
| Pasta de dados | `%ProgramData%\Meddrive Printer` | `%ALLUSERSPROFILE%\Application Data\Meddrive Printer` |
| Ghostscript | `gswin64c.exe` | `gswin32c.exe` |
| Ambiente do driver | `Windows x64` | `Windows NT x86` |
| Pasta do driver no spool | `x64\3\` | `w32x86\3\` |

---

## Auto-start do agente pela Run key

O Task Scheduler 2.0 (usado no Win7/Vista/10 via `schtasks` e `Schedule.Service` COM) é Vista+. No XP o agente sobe pela chave Run:

```
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run
    MeddrivePrinterAgent = "...\Meddrive Printer\MeddrivePrinterAgent.exe"
```

O `install_helper_xp.exe` grava essa chave e ainda inicia o agente na hora, sem exigir logout. No MedDriveManager, sob `MEDDRIVE_XP`, o liga/desliga do auto-start lê e grava essa mesma chave em vez do Task Scheduler.

---

## Driver PSCRIPT5 empacotado

No XP não existe DriverStore. Num XP recém-instalado os arquivos do PScript5 ficam compactados no `sp3.cab` e só aparecem em `w32x86\3\` depois que alguma impressora PostScript é instalada.

Para funcionar em qualquer XP, os arquivos vão empacotados no instalador, em `installer/winxp/deps/pscript5/`:

- `PSCRIPT5.DLL`
- `PS5UI.DLL`
- `PSCRIPT.NTF`

Precisam ser a versão i386, subsistema 5.00 (do XP SP3). Dá para obtê-los instalando qualquer impressora PostScript num XP e copiando de `C:\WINDOWS\system32\spool\drivers\w32x86\3\`, ou extraindo do `sp3.cab` do CD com `expand`.

O `install_helper_xp.exe` aponta o `AddPrinterDriverExW` (flag `0x14 = APD_COPY_ALL_FILES | APD_COPY_FROM_DIRECTORY`) para essa pasta, e o spooler copia para `w32x86\3\`.

---

## Ghostscript 32-bit

O XP usa uma build de 32 bits do Ghostscript, em `gs/ghostscript-winxp/`:

- `bin/gswin32c.exe` (i386)
- `bin/gsdll32.dll` (i386)

Uma build de 2015 funciona no XP sem depender de UCRT nem de VC runtime moderno. O executável é `gswin32c.exe`, não `gswin64c.exe`.

---

## Portas e impressoras via API do spooler

### AddPortEx retorna erro 87

Ao criar um perfil, o `AddPortExW` falha no XP com Win32 erro `87` (`ERROR_INVALID_PARAMETER`). É benigno: o script grava a porta direto no registry, e o monitor descobre as portas lendo o registry no `EnumPorts`. A porta funciona mesmo com o aviso.

### DeletePrinter e SetPrinter exigem acesso de administrador

`DeletePrinter` e `SetPrinter` precisam de um handle aberto com `PRINTER_ACCESS_ADMINISTER`. Abrir com `OpenPrinter(..., NULL)` dá só `PRINTER_ACCESS_USE`, e a operação falha com Win32 erro `5` (`ERROR_ACCESS_DENIED`).

Os scripts que removem, recriam ou editam impressora abrem com `PRINTER_DEFAULTS` e `DesiredAccess = 0x000F000C` (`PRINTER_ALL_ACCESS`).

### Renomear impressora sem WMI

O `Win32_Printer` do WMI não tem o método `Rename` no XP (só a partir do Vista). O `edit-printer.ps1` renomeia por `SetPrinter`, gravando o novo nome em `pPrinterName` num único chamado que também ajusta a porta.

---

## PowerShell 2.0 nos scripts conf

Os seis scripts `conf\*.ps1` são autossuficientes em `installer/winxp/conf/` e usam só P/Invoke Win32, WMI clássico e registry. Nada do módulo `PrintManagement` (que é Win8+) nem `Get-CimInstance` (que é PS 3+).

O `$_.ScriptStackTrace` no bloco `trap` é PS 3+. No XP (PS 2.0) ele volta vazio, então os scripts têm um fallback que registra "(indisponivel no PowerShell 2.0)".

---

## Diferenças em relação ao Win7/Vista

| Item | Win7/Vista | XP |
|---|---|---|
| Arquitetura | x64 | x86 (i686) |
| Ghostscript | 9.56.1 x64 | 32-bit (ghostscript-winxp) |
| Pasta de dados | %ProgramData% | %ALLUSERSPROFILE%\Application Data |
| Início do agente | Task Scheduler | Run key (HKLM) |
| Driver PSCRIPT5 | DriverStore | empacotado (deps/pscript5) |
| Hash de senha | bcrypt | CryptoAPI (advapi32) |
| DWM | link estático | LoadLibrary em runtime |
| Arquivo gerado | `MeddrivePrinter-Win7/Vista-Setup.exe` | `MeddrivePrinter-WinXP-Setup.exe` |

---

## Como compilar

```
./build-xp.sh
```

Requer `i686-w64-mingw32-gcc` e `makensis`. Gera as três DLLs/EXEs i686 (mais o `install_helper_xp.exe`, compilado à parte) e o `MeddrivePrinter-WinXP-Setup.exe`.
