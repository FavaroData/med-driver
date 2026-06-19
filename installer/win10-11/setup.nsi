; Meddrive Printer — instalador NSIS
; Requer: makensis (NSIS >= 3.0)
; Gera:   MeddrivePrinter-Setup.exe

Target amd64-unicode
Unicode True

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

; ---------- metadados ----------
Name          "Meddrive Printer"
OutFile       "..\..\MeddrivePrinter-Setup.exe"
InstallDir    "$TEMP\MedPDFPrinter"
BrandingText  "Meddrive Printer"

; solicita admin
RequestExecutionLevel admin

; ---------- bem-vindo ----------
!define MUI_WELCOMEPAGE_TITLE      "Bem-vindo ao Meddrive Printer"
!define MUI_WELCOMEPAGE_TEXT       "O Meddrive Printer é uma impressora virtual que converte \
documentos para PDF usando Ghostscript.$\r$\n$\r$\n\
Este instalador irá configurar:$\r$\n\
  • Monitor de impressão (meddrivemon.dll)$\r$\n\
  • Driver PSCRIPT5 customizado$\r$\n\
  • Ghostscript (motor de conversão PDF)$\r$\n\
  • Aplicativo de gerenciamento MedDriveManager$\r$\n$\r$\n\
Clique em Avançar para continuar."

; ---------- páginas ----------
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
Page custom PgPaths PgPathsLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "PortugueseBR"

; ---------- página: caminhos de instalação ----------
Var hDlg
Var hEditPaths

Function PgPaths
    !insertmacro MUI_HEADER_TEXT \
        "Caminhos de instalação" \
        "Revise os locais onde os componentes serão instalados."

    nsDialogs::Create 1018
    Pop $hDlg

    ; Caixa de texto read-only com os caminhos
    ReadEnvStr $R0 "SystemRoot"
    ReadEnvStr $R1 "ProgramData"
    StrCpy $R2 "$R0\System32"
    StrCpy $R3 "$R1\Meddrive Printer"

    ; ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_CHILD|WS_VISIBLE|WS_BORDER
    ; 0x00000004|0x00000800|0x00200000|0x40000000|0x10000000|0x00800000 = 0x50A00804
    nsDialogs::CreateControl "EDIT" 0x50A00804 0 0 0 100% 130u ""
    Pop $hEditPaths

    StrCpy $R4 "Monitor e DLL do Spooler:$\r$\n"
    StrCpy $R4 "$R4  $R2\meddrivemon.dll$\r$\n"
    StrCpy $R4 "$R4$\r$\nDriver PSCRIPT5 e PPD:$\r$\n"
    StrCpy $R4 "$R4  $R2\spool\drivers\x64\3\$\r$\n"
    StrCpy $R4 "$R4$\r$\nAplicativo de gerenciamento:$\r$\n"
    StrCpy $R4 "$R4  $R3\MedDriveManager.exe$\r$\n"
    StrCpy $R4 "$R4  $R3\add-printer.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\create-profile.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\remove-printer.ps1$\r$\n"
    StrCpy $R4 "$R4$\r$\nGhostscript (motor PDF):$\r$\n"
    StrCpy $R4 "$R4  $R3\Ghostscript\"
    SendMessage $hEditPaths ${WM_SETTEXT} 0 "STR:$R4"

    ; Renomeia o botão Próximo para "Instalar"
    GetDlgItem $0 $HWNDPARENT 1
    SendMessage $0 ${WM_SETTEXT} 0 "STR:Instalar"

    nsDialogs::Show
FunctionEnd

Function PgPathsLeave
    ; Restaura o texto padrão do botão (não é necessário, mas mantém consistência)
    GetDlgItem $0 $HWNDPARENT 1
    SendMessage $0 ${WM_SETTEXT} 0 "STR:Avançar >"
FunctionEnd

; ---------- instalação ----------
Section "Instalar Meddrive Printer" SecInstall

    ; extrai DLL e scripts para diretório temporário
    SetOutPath "$INSTDIR"
    File "..\..\meddrivemon.dll"
    SetOutPath "$INSTDIR\installer"
    File "MEDDRIVE.PPD"
    File "install.ps1"
    File "add-printer.ps1"
    File "create-profile.ps1"
    File "remove-printer.ps1"
    File "x64\Debug\MedDriveManager.exe"

    ; lê ProgramData do ambiente Windows em tempo de execução
    ReadEnvStr $R0 "ProgramData"

    ; extrai Ghostscript bundled para ProgramData
    SetOutPath "$R0\Meddrive Printer\Ghostscript\bin"
    File /r "..\..\gs\ghostscript\bin\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\lib"
    File /r "..\..\gs\ghostscript\lib\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\Resource"
    File /r "..\..\gs\ghostscript\Resource\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\iccprofiles"
    File /r "..\..\gs\ghostscript\iccprofiles\*"

    DetailPrint "Executando instalador PowerShell..."
    nsExec::ExecToLog 'powershell.exe -ExecutionPolicy Bypass -NonInteractive -File "$INSTDIR\installer\install.ps1"'
    Pop $0

    ; limpa temporários (arquivos já copiados para ProgramData pelo install.ps1)
    Delete "$INSTDIR\installer\install.ps1"
    Delete "$INSTDIR\installer\add-printer.ps1"
    Delete "$INSTDIR\installer\create-profile.ps1"
    Delete "$INSTDIR\installer\remove-printer.ps1"
    Delete "$INSTDIR\installer\MedDriveManager.exe"
    Delete "$INSTDIR\installer\MEDDRIVE.PPD"
    RMDir  "$INSTDIR\installer"
    Delete "$INSTDIR\meddrivemon.dll"
    RMDir  "$INSTDIR"

    ${If} $0 != 0
        DetailPrint "ERRO: instalação falhou (código $0)"
        MessageBox MB_OK|MB_ICONSTOP \
            "A instalação falhou.$\n$\nConsulte o log em:$\nC:\Windows\Temp\meddrive_ps_install.log"
        SetErrorLevel 1
        Quit
    ${EndIf}

    DetailPrint "Instalação concluída."
    DetailPrint "  Aplicativo : $R0\Meddrive Printer\MedDriveManager.exe"
    DetailPrint "  Ghostscript: $R0\Meddrive Printer\Ghostscript\bin\gswin64c.exe"

SectionEnd
