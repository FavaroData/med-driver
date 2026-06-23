; Meddrive Printer — instalador NSIS para Windows 7 x64
; Requer: makensis (NSIS >= 3.0)
; Gera:   MeddrivePrinter-Win7-Setup.exe

Target amd64-unicode
Unicode True

!include "MUI2.nsh"
!include "LogicLib.nsh"

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
  • Aplicativo de gerenciamento MedDriveManager$\r$\n$\r$\n\
Após a instalação, use o MedDriveManager para criar perfis e impressoras.$\r$\n$\r$\n\
Clique em Avançar para continuar."

; ---------- páginas ----------
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "PortugueseBR"

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
    File "install.ps1"
    File "add-printer.ps1"
    File "create-profile.ps1"
    File "edit-profile.ps1"
    File "edit-printer.ps1"
    File "remove-printer.ps1"
    File "remove-profile.ps1"
    File "..\win10-11\x64\Debug\MedDriveManager.exe"

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

    DetailPrint "Executando instalador PowerShell..."
    nsExec::ExecToLog 'powershell.exe -ExecutionPolicy Bypass -NonInteractive -File "$INSTDIR\installer\install.ps1"'
    Pop $0

    ; limpa temporários (arquivos já copiados para ProgramData pelo install.ps1)
    Delete "$INSTDIR\installer\install.ps1"
    Delete "$INSTDIR\installer\add-printer.ps1"
    Delete "$INSTDIR\installer\create-profile.ps1"
    Delete "$INSTDIR\installer\edit-profile.ps1"
    Delete "$INSTDIR\installer\edit-printer.ps1"
    Delete "$INSTDIR\installer\remove-printer.ps1"
    Delete "$INSTDIR\installer\remove-profile.ps1"
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
