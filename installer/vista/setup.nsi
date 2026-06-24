; Meddrive Printer — instalador NSIS para Windows Vista x64 SP2
; Requer: makensis (NSIS >= 3.0)
; Gera:   MeddrivePrinter-Vista-Setup.exe
;
; Pré-requisitos verificados em .onInit:
;   - Windows Vista SP2 ou superior (build >= 6002)
;   - PowerShell 2.0 ou superior

Target amd64-unicode
Unicode True

!include "MUI2.nsh"
!include "LogicLib.nsh"

; ---------- metadados ----------
Name          "Meddrive Printer"
OutFile       "..\..\MeddrivePrinter-Vista-Setup.exe"
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

; ---------- verificações de pré-requisito ----------
Function .onInit

    ; --- Vista SP2: build >= 6002 ---
    ; Win7 = 7600+, Vista SP2 = 6002, Vista SP1 = 6001, Vista RTM = 6000
    ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" "CurrentBuildNumber"
    IntCmp $0 6002 +4 0 +4
        MessageBox MB_OK|MB_ICONSTOP \
            "Este instalador requer Windows Vista com Service Pack 2 (ou superior).$\r$\n$\r$\n\
Versão detectada: build $0.$\r$\n$\r$\n\
Instale o Windows Vista SP2 e tente novamente."
        Abort

    ; --- PowerShell 2.0 ---
    ; Chave presente desde o PS 1.0; valor "PowerShellVersion" indica a versão instalada.
    ReadRegStr $1 HKLM "SOFTWARE\Microsoft\PowerShell\1" "PowerShellVersion"

    ${If} $1 == ""
        MessageBox MB_OK|MB_ICONSTOP \
            "PowerShell não foi encontrado.$\r$\n$\r$\n\
Este aplicativo requer PowerShell 2.0 para funcionar.$\r$\n$\r$\n\
Instale o PowerShell 2.0 via Windows Update (KB968930) e tente novamente."
        Abort
    ${EndIf}

    ; Compara apenas o dígito principal da versão
    StrCpy $2 $1 1
    ${If} $2 == "1"
        MessageBox MB_OK|MB_ICONSTOP \
            "PowerShell 2.0 ou superior é necessário.$\r$\n$\r$\n\
Versão detectada: $1$\r$\n$\r$\n\
Instale o PowerShell 2.0 via Windows Update (KB968930) e tente novamente."
        Abort
    ${EndIf}

FunctionEnd

; ---------- instalação ----------
Section "Instalar Meddrive Printer" SecInstall

    ; extrai DLL e scripts para diretório temporário
    SetOutPath "$INSTDIR"
    File "..\..\meddrivemon.dll"
    SetOutPath "$INSTDIR\installer"
    File "..\win7\MEDDRIVE.PPD"
    File "..\win7\install_helper.exe"
    File "..\win10-11\x64\Debug\MedDriveManager.exe"
    SetOutPath "$INSTDIR\installer\conf"
    File "..\win7\conf\add-printer.ps1"
    File "..\win7\conf\create-profile.ps1"
    File "..\win7\conf\edit-profile.ps1"
    File "..\win7\conf\edit-printer.ps1"
    File "..\win7\conf\remove-printer.ps1"
    File "..\win7\conf\remove-profile.ps1"

    ; lê ProgramData do ambiente Windows em tempo de execução
    ReadEnvStr $R0 "ProgramData"

    ; extrai Ghostscript 9.56.1 para ProgramData
    SetOutPath "$R0\Meddrive Printer\Ghostscript\bin"
    File /r "..\..\gs\ghostscript-win7\bin\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\lib"
    File /r "..\..\gs\ghostscript-win7\lib\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\Resource"
    File /r "..\..\gs\ghostscript-win7\Resource\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\iccprofiles"
    File /r "..\..\gs\ghostscript-win7\iccprofiles\*"

    ; instala DLLs de runtime ausentes (VC++ Runtime + Universal CRT)
    ; O UCRT é suportado no Vista SP2 via KB2999226.
    DetailPrint "Verificando DLLs de runtime..."
    SetOutPath "$INSTDIR\DLL"
    File "..\win7\DLL\vcruntime140.dll"
    File "..\win7\DLL\vcruntime140_1.dll"
    File "..\win7\DLL\ucrtbase.dll"
    File "..\win7\DLL\api-ms-win-crt-convert-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-environment-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-filesystem-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-heap-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-locale-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-math-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-runtime-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-stdio-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-string-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-time-l1-1-0.dll"
    File "..\win7\DLL\api-ms-win-crt-utility-l1-1-0.dll"

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

    ; limpa temporários (arquivos já copiados para ProgramData pelo install_helper.exe)
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
            "A instalação falhou.$\n$\nConsulte o log em:$\nC:\Windows\Temp\meddrive_install.log"
        SetErrorLevel 1
        Quit
    ${EndIf}

    DetailPrint "Instalação concluída."
    DetailPrint "  Aplicativo : $R0\Meddrive Printer\MedDriveManager.exe"
    DetailPrint "  Ghostscript: $R0\Meddrive Printer\Ghostscript\bin\gswin64c.exe"

SectionEnd
