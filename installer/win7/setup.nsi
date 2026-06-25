; Meddrive Printer — instalador NSIS para Windows 7 x64
; Requer: makensis (NSIS >= 3.0)
; Gera:   MeddrivePrinter-Win7-Setup.exe

Target amd64-unicode
Unicode True

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

; ---------- metadados ----------
Name          "Meddrive Printer"
OutFile       "..\..\MeddrivePrinter-Win7-Setup.exe"
InstallDir    "$TEMP\MedPDFPrinter"
BrandingText  "Meddrive Printer"

RequestExecutionLevel admin

; ---------- bem-vindo ----------
!define MUI_WELCOMEPAGE_TITLE      "Bem-vindo ao Meddrive Printer"
!define MUI_WELCOMEPAGE_TEXT       "O Meddrive Printer é uma impressora virtual que converte \
documentos para PDF usando Ghostscript.$\r$\n$\r$\n\
Este instalador irá configurar:$\r$\n\
  • Monitor de impressão (meddrivemon.dll)$\r$\n\
  • Driver PSCRIPT5 customizado$\r$\n\
  • Ghostscript 9.56.1 (motor de conversão PDF)$\r$\n\
  • Aplicativo de gerenciamento MedDriveManager$\r$\n\
  • Agente de sessão MeddrivePrinterAgent$\r$\n$\r$\n\
Após a instalação, use o MedDriveManager para criar perfis e impressoras.$\r$\n$\r$\n\
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
    StrCpy $R4 "$R4  $R3\conf\add-printer.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\create-profile.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\edit-profile.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\edit-printer.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\remove-printer.ps1$\r$\n"
    StrCpy $R4 "$R4  $R3\conf\remove-profile.ps1$\r$\n"
    StrCpy $R4 "$R4$\r$\nAgente de sessao:$\r$\n"
    StrCpy $R4 "$R4  $R3\MeddrivePrinterAgent.exe$\r$\n"
    StrCpy $R4 "$R4$\r$\nGhostscript 9.56.1 (motor PDF):$\r$\n"
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

; ---------- macro: copia DLL para System32 se ausente ----------
!macro InstallDllIfMissing DLL_NAME
    IfFileExists "$WINDIR\System32\${DLL_NAME}" +2
        CopyFiles /SILENT "$INSTDIR\DLL\${DLL_NAME}" "$WINDIR\System32\"
!macroend

; ---------- instalação ----------
Section "Instalar Meddrive Printer" SecInstall

    ; extrai DLL e scripts para diretório temporário
    SetOutPath "$INSTDIR"
    File "..\..\meddrivemon.dll"
    SetOutPath "$INSTDIR\installer"
    File "MEDDRIVE.PPD"
    File "install_helper.exe"
    File "..\win10-11\x64\Debug\MedDriveManager.exe"
    File "..\..\src\agent\MeddrivePrinterAgent.exe"
    SetOutPath "$INSTDIR\installer\agent"
    File "..\..\src\agent\register-agent.ps1"
    SetOutPath "$INSTDIR\installer\conf"
    File "conf\add-printer.ps1"
    File "conf\create-profile.ps1"
    File "conf\edit-profile.ps1"
    File "conf\edit-printer.ps1"
    File "conf\remove-printer.ps1"
    File "conf\remove-profile.ps1"

    ; lê ProgramData do ambiente Windows em tempo de execução
    ReadEnvStr $R0 "ProgramData"

    ; extrai Ghostscript 9.56.1 (compatível com Win7) para ProgramData
    SetOutPath "$R0\Meddrive Printer\Ghostscript\bin"
    File /r "..\..\gs\ghostscript-win7\bin\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\lib"
    File /r "..\..\gs\ghostscript-win7\lib\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\Resource"
    File /r "..\..\gs\ghostscript-win7\Resource\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\iccprofiles"
    File /r "..\..\gs\ghostscript-win7\iccprofiles\*"

    ; instala DLLs de runtime ausentes no Win7 (VC++ Runtime + Universal CRT)
    DetailPrint "Verificando DLLs de runtime para Win7..."
    SetOutPath "$INSTDIR\DLL"
    File "DLL\vcruntime140.dll"
    File "DLL\vcruntime140_1.dll"
    File "DLL\ucrtbase.dll"
    File "DLL\api-ms-win-crt-convert-l1-1-0.dll"
    File "DLL\api-ms-win-crt-environment-l1-1-0.dll"
    File "DLL\api-ms-win-crt-filesystem-l1-1-0.dll"
    File "DLL\api-ms-win-crt-heap-l1-1-0.dll"
    File "DLL\api-ms-win-crt-locale-l1-1-0.dll"
    File "DLL\api-ms-win-crt-math-l1-1-0.dll"
    File "DLL\api-ms-win-crt-runtime-l1-1-0.dll"
    File "DLL\api-ms-win-crt-stdio-l1-1-0.dll"
    File "DLL\api-ms-win-crt-string-l1-1-0.dll"
    File "DLL\api-ms-win-crt-time-l1-1-0.dll"
    File "DLL\api-ms-win-crt-utility-l1-1-0.dll"

    !insertmacro InstallDllIfMissing "vcruntime140.dll"
    !insertmacro InstallDllIfMissing "vcruntime140_1.dll"
    !insertmacro InstallDllIfMissing "ucrtbase.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-convert-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-environment-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-filesystem-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-heap-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-locale-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-math-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-runtime-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-stdio-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-string-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-time-l1-1-0.dll"
    !insertmacro InstallDllIfMissing "api-ms-win-crt-utility-l1-1-0.dll"

    RMDir /r "$INSTDIR\DLL"

    DetailPrint "Executando instalador..."
    nsExec::ExecToLog '"$INSTDIR\installer\install_helper.exe" "$INSTDIR\installer"'
    Pop $0

    DetailPrint "Registrando agente no Task Scheduler..."
    nsExec::ExecToLog 'powershell.exe -ExecutionPolicy Bypass -NonInteractive -File "$INSTDIR\installer\agent\register-agent.ps1"'
    Pop $1
    ${If} $1 != 0
        DetailPrint "AVISO: falha ao registrar MeddrivePrinterAgent (codigo $1)"
    ${EndIf}

    ; limpa temporários (arquivos já copiados para ProgramData pelo install_helper.exe)
    Delete "$INSTDIR\installer\agent\register-agent.ps1"
    RMDir  "$INSTDIR\installer\agent"
    Delete "$INSTDIR\installer\MeddrivePrinterAgent.exe"
    Delete "$INSTDIR\installer\install_helper.exe"
    Delete "$INSTDIR\installer\conf\add-printer.ps1"
    Delete "$INSTDIR\installer\conf\create-profile.ps1"
    Delete "$INSTDIR\installer\conf\edit-profile.ps1"
    Delete "$INSTDIR\installer\conf\edit-printer.ps1"
    Delete "$INSTDIR\installer\conf\remove-printer.ps1"
    Delete "$INSTDIR\installer\conf\remove-profile.ps1"
    RMDir  "$INSTDIR\installer\conf"
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
