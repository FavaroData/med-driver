; Meddrive Printer — instalador NSIS para Windows XP x86 SP2+
; Requer: makensis (NSIS >= 3.0)
; Gera:   MeddrivePrinter-WinXP-Setup.exe
;
; Pre-requisitos verificados em .onInit:
;   - Windows XP x86 (CurrentVersion "5.1") com SP2 ou superior
;
; ANTES DE COMPILAR, fornecer em installer\winxp\:
;   - install_helper_xp.exe  (compilar install_helper_xp.c com i686-w64-mingw32-gcc)
;   - MedDriveManager.exe    (versao XP x86 — ver src\gui-native\)
;
; Em src\agent\: recompilar MeddrivePrinterAgent.exe como i686 com WINVER=0x0501
; Em raiz do projeto: recompilar meddrivemon.dll como i686 com WINVER=0x0501
;
; Ghostscript XP x86: colocar em gs\ghostscript-winxp\bin\ e lib\
;   (ex: GS 9.14 w32 — sem UCRT, sem VC runtime moderno)
;   Se a pasta estiver ausente o NSIS compila mas o GS nao e instalado.
;
; PowerShell 2.0 NAO e necessario para instalacao (install_helper_xp cuida de tudo).
; Para usar o MedDriveManager, PS 2.0 e necessario (KB968930 para XP SP3).

Unicode True

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

; ---------- metadados ----------
Name          "Meddrive Printer"
OutFile       "..\..\MeddrivePrinter-WinXP-Setup.exe"
InstallDir    "$TEMP\MedPDFPrinter"
BrandingText  "Meddrive Printer"

RequestExecutionLevel admin

; ---------- bem-vindo ----------
!define MUI_WELCOMEPAGE_TITLE      "Bem-vindo ao Meddrive Printer"
!define MUI_WELCOMEPAGE_TEXT       "O Meddrive Printer e uma impressora virtual que converte \
documentos para PDF usando Ghostscript.$\r$\n$\r$\n\
Este instalador ira configurar:$\r$\n\
  * Monitor de impressao (meddrivemon.dll)$\r$\n\
  * Driver PSCRIPT5 customizado$\r$\n\
  * Ghostscript (motor de conversao PDF)$\r$\n\
  * Aplicativo de gerenciamento MedDriveManager$\r$\n\
  * Agente de sessao MeddrivePrinterAgent$\r$\n$\r$\n\
Apos a instalacao, use o MedDriveManager para criar perfis e impressoras.$\r$\n$\r$\n\
Clique em Avancar para continuar."

; ---------- paginas ----------
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
Page custom PgPaths PgPathsLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "PortugueseBR"

; ---------- pagina: caminhos de instalacao ----------
Var hDlg
Var hEditPaths

Function PgPaths
    !insertmacro MUI_HEADER_TEXT \
        "Caminhos de instalacao" \
        "Revise os locais onde os componentes serao instalados."

    nsDialogs::Create 1018
    Pop $hDlg

    ; ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_CHILD|WS_VISIBLE|WS_BORDER
    nsDialogs::CreateControl "EDIT" 0x50A00804 0 0 0 100% 130u ""
    Pop $hEditPaths

    ReadEnvStr $R0 "SystemRoot"
    ReadEnvStr $R1 "ALLUSERSPROFILE"
    StrCpy $R2 "$R0\System32"
    ; No XP, equivalente ao ProgramData e %ALLUSERSPROFILE%\Application Data
    StrCpy $R3 "$R1\Application Data\Meddrive Printer"

    StrCpy $R4 "Monitor e DLL do Spooler:$\r$\n"
    StrCpy $R4 "$R4  $R2\meddrivemon.dll$\r$\n"
    StrCpy $R4 "$R4$\r$\nDriver PSCRIPT5 e PPD:$\r$\n"
    StrCpy $R4 "$R4  $R2\spool\drivers\w32x86\3\$\r$\n"
    StrCpy $R4 "$R4$\r$\nAplicativo de gerenciamento:$\r$\n"
    StrCpy $R4 "$R4  $R3\MedDriveManager.exe$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\add-printer.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\create-profile.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\edit-profile.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\edit-printer.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\remove-profile.ps1$\r$\n"
    StrCpy $R4 "$R4$\r$\nAgente de sessao:$\r$\n"
    StrCpy $R4 "$R4  $R3\MeddrivePrinterAgent.exe$\r$\n"
    StrCpy $R4 "$R4$\r$\nGhostscript (motor PDF):$\r$\n"
    StrCpy $R4 "$R4  $R3\Ghostscript\"
    SendMessage $hEditPaths ${WM_SETTEXT} 0 "STR:$R4"

    GetDlgItem $0 $HWNDPARENT 1
    SendMessage $0 ${WM_SETTEXT} 0 "STR:Instalar"

    nsDialogs::Show
FunctionEnd

Function PgPathsLeave
    GetDlgItem $0 $HWNDPARENT 1
    SendMessage $0 ${WM_SETTEXT} 0 "STR:Avancar >"
FunctionEnd

; ---------- verificacoes de pre-requisito ----------
Function .onInit

    ; --- Verifica Windows XP x86 (CurrentVersion = "5.1") ---
    ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" "CurrentVersion"
    StrCmp $0 "5.1" xp_ok
        MessageBox MB_OK|MB_ICONSTOP \
            "Este instalador requer Windows XP x86 (32-bit).$\r$\n$\r$\n\
Versao detectada: $0.$\r$\n$\r$\n\
Para Windows Vista/7/10/11, use o instalador correspondente."
        Abort
    xp_ok:

    ; --- Verifica SP2 ou SP3 ---
    ReadRegStr $1 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" "CSDVersion"
    StrCmp $1 "Service Pack 2" sp_ok
    StrCmp $1 "Service Pack 3" sp_ok
        MessageBox MB_OK|MB_ICONSTOP \
            "Este instalador requer Windows XP Service Pack 2 ou superior.$\r$\n$\r$\n\
Service Pack detectado: '$1'.$\r$\n$\r$\n\
Instale o XP SP2 ou SP3 via Windows Update e tente novamente."
        Abort
    sp_ok:

FunctionEnd

; ---------- instalacao ----------
Section "Instalar Meddrive Printer" SecInstall

    ; extrai DLL e arquivos para diretorio temporario
    SetOutPath "$INSTDIR"
    File /oname=meddrivemon.dll "meddrivemon_xp.dll"
    SetOutPath "$INSTDIR\installer"
    File "..\win7\MEDDRIVE.PPD"
    File "install_helper_xp.exe"
    File /oname=MeddrivePrinterAgent.exe "MeddrivePrinterAgent_xp.exe"
    ; MedDriveManager.exe: versao XP x86 em installer\winxp\ (nonfatal ate ser compilada)
    File /nonfatal "MedDriveManager.exe"

    ; Arquivos do driver PScript5 (XP i386) -- nao existem em XP limpo
    SetOutPath "$INSTDIR\installer\pscript5"
    File "deps\pscript5\PSCRIPT5.DLL"
    File "deps\pscript5\PS5UI.DLL"
    File "deps\pscript5\PSCRIPT.NTF"
    File /nonfatal "deps\pscript5\PSCRIPT.HLP"

    ; todos os scripts XP sao autossuficientes em winxp\conf
    ; (env Windows NT x86, ALLUSERSPROFILE, gswin32c, trap compativel com PS 2.0):
    SetOutPath "$INSTDIR\installer\conf"
    File "conf\add-printer.ps1"
    File "conf\create-profile.ps1"
    File "conf\edit-profile.ps1"
    File "conf\edit-printer.ps1"
    File "conf\remove-profile.ps1"

    ; Ghostscript XP x86 (nonfatal -- usuario fornece o binario separadamente)
    ReadEnvStr $R0 "ALLUSERSPROFILE"
    SetOutPath "$R0\Application Data\Meddrive Printer\Ghostscript\bin"
    File /nonfatal /r "..\..\gs\ghostscript-winxp\bin\*"
    SetOutPath "$R0\Application Data\Meddrive Printer\Ghostscript\lib"
    File /nonfatal /r "..\..\gs\ghostscript-winxp\lib\*"

    DetailPrint "Executando instalador XP..."
    nsExec::ExecToLog '"$INSTDIR\installer\install_helper_xp.exe" "$INSTDIR\installer"'
    Pop $0

    ; limpa temporarios
    Delete "$INSTDIR\installer\conf\add-printer.ps1"
    Delete "$INSTDIR\installer\conf\create-profile.ps1"
    Delete "$INSTDIR\installer\conf\edit-profile.ps1"
    Delete "$INSTDIR\installer\conf\edit-printer.ps1"
    Delete "$INSTDIR\installer\conf\remove-profile.ps1"
    RMDir  "$INSTDIR\installer\conf"
    Delete "$INSTDIR\installer\pscript5\PSCRIPT5.DLL"
    Delete "$INSTDIR\installer\pscript5\PS5UI.DLL"
    Delete "$INSTDIR\installer\pscript5\PSCRIPT.NTF"
    Delete "$INSTDIR\installer\pscript5\PSCRIPT.HLP"
    RMDir  "$INSTDIR\installer\pscript5"
    Delete "$INSTDIR\installer\MeddrivePrinterAgent.exe"
    Delete "$INSTDIR\installer\MedDriveManager.exe"
    Delete "$INSTDIR\installer\install_helper_xp.exe"
    Delete "$INSTDIR\installer\MEDDRIVE.PPD"
    RMDir  "$INSTDIR\installer"
    Delete "$INSTDIR\meddrivemon.dll"
    RMDir  "$INSTDIR"

    ${If} $0 != 0
        DetailPrint "ERRO: instalacao falhou (codigo $0)"
        MessageBox MB_OK|MB_ICONSTOP \
            "A instalacao falhou.$\n$\nConsulte o log em:$\nC:\Windows\Temp\meddrive_install.log"
        SetErrorLevel 1
        Quit
    ${EndIf}

    DetailPrint "Instalacao concluida."
    ReadEnvStr $R0 "ALLUSERSPROFILE"
    DetailPrint "  Aplicativo : $R0\Application Data\Meddrive Printer\MedDriveManager.exe"
    DetailPrint "  Ghostscript: $R0\Application Data\Meddrive Printer\Ghostscript\bin\gswin32c.exe"

SectionEnd
