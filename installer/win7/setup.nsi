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

; ---------- variáveis ----------
Var OutputFolder
Var PrinterName

; ---------- páginas MUI ----------
!define MUI_ABORTWARNING

Page custom PgOutputFolder PgOutputFolderLeave

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "PortugueseBR"

; ---------- página: configuração ----------
!include "nsDialogs.nsh"
Var hDlg
Var hPrinterLabel
Var hPrinterText
Var hLabel
Var hFolderText
Var hBrowse

Function PgOutputFolder
    !insertmacro MUI_HEADER_TEXT "Configuração" "Nome da impressora e pasta de destino dos PDFs"

    nsDialogs::Create 1018
    Pop $hDlg

    ${NSD_CreateLabel} 0 0u 100% 12u "Nome da impressora:"
    Pop $hPrinterLabel

    ${NSD_CreateText} 0 14u 300u 14u "Meddrive Printer"
    Pop $hPrinterText

    ${NSD_CreateLabel} 0 36u 100% 12u "Pasta de destino dos arquivos PDF:"
    Pop $hLabel

    ${NSD_CreateDirRequest} 0 52u 248u 14u "$DOCUMENTS\PDF"
    Pop $hFolderText

    ${NSD_CreateBrowseButton} 252u 51u 48u 15u "Procurar..."
    Pop $hBrowse
    GetFunctionAddress $0 OnBrowseFolder
    nsDialogs::OnClick $hBrowse $0

    nsDialogs::Show
FunctionEnd

Function OnBrowseFolder
    nsDialogs::SelectFolderDialog "Selecione a pasta de saída" "$DOCUMENTS\PDF"
    Pop $0
    ${If} $0 != error
        ${NSD_SetText} $hFolderText $0
    ${EndIf}
FunctionEnd

Function PgOutputFolderLeave
    ${NSD_GetText} $hPrinterText $PrinterName
    ${If} $PrinterName == ""
        MessageBox MB_OK|MB_ICONEXCLAMATION "Informe o nome da impressora."
        Abort
    ${EndIf}
    ${NSD_GetText} $hFolderText $OutputFolder
    ${If} $OutputFolder == ""
        MessageBox MB_OK|MB_ICONEXCLAMATION "Selecione uma pasta de destino."
        Abort
    ${EndIf}
FunctionEnd

; ---------- macro: copia DLL para System32 se ausente ----------
!macro InstallDllIfMissing DLL_NAME
    IfFileExists "$WINDIR\System32\${DLL_NAME}" +2
        CopyFiles /SILENT "$INSTDIR\DLL\${DLL_NAME}" "$WINDIR\System32\"
!macroend

; ---------- instalação ----------
Section "Instalar Meddrive Printer" SecInstall

    ; extrai DLL e scripts de instalação
    SetOutPath "$INSTDIR"
    File "..\..\meddrivemon.dll"
    SetOutPath "$INSTDIR\installer"
    File "MEDDRIVE.PPD"
    File "install.ps1"

    ; lê ProgramData do ambiente Windows em tempo de execução
    ReadEnvStr $R0 "ProgramData"

    ; extrai Ghostscript bundled para ProgramData
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

    StrCpy $1 "$OutputFolder\saida.pdf"

    DetailPrint "Executando instalador PowerShell..."
    nsExec::ExecToLog 'powershell.exe -ExecutionPolicy Bypass -NonInteractive -File "$INSTDIR\installer\install.ps1" -OutputPath "$1" -PrinterName "$PrinterName"'
    Pop $0

    Delete "$INSTDIR\installer\install.ps1"
    Delete "$INSTDIR\installer\MEDDRIVE.PPD"
    RMDir  "$INSTDIR\installer"
    Delete "$INSTDIR\meddrivemon.dll"
    RMDir  "$INSTDIR"

    ${If} $0 != 0
        DetailPrint "ERRO: instalação falhou (código $0)"
        MessageBox MB_OK|MB_ICONSTOP \
            "A instalação falhou.$\n$\nConsulte o log em:$\nC:\Windows\Temp\meddrivemon_init.log"
        SetErrorLevel 1
        Quit
    ${EndIf}

    DetailPrint "Instalação concluída."
    DetailPrint "  Impressora : $PrinterName"
    DetailPrint "  Saída      : $1"
    DetailPrint "  Ghostscript: $R0\Meddrive Printer\Ghostscript\bin\gswin64c.exe"

SectionEnd
