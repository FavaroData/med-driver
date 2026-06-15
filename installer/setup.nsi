; Meddrive Printer — instalador NSIS
; Requer: makensis (NSIS >= 3.0)
; Gera:   MedPDFPrinter-Setup.exe

Target amd64-unicode
Unicode True

!include "MUI2.nsh"
!include "LogicLib.nsh"

; ---------- metadados ----------
Name          "Meddrive Printer"
OutFile       "..\MeddrivePrinter-Setup.exe"
InstallDir    "$TEMP\MedPDFPrinter"
BrandingText  "Meddrive Printer"

; solicita elevação UAC — o Windows exibe o prompt ao abrir o instalador
RequestExecutionLevel admin

; ---------- variáveis ----------
Var OutputFolder
Var GhostscriptPath
Var PrinterName

; ---------- páginas MUI ----------
!define MUI_ABORTWARNING

; substitui a página de diretório padrão por uma customizada
Page custom PgOutputFolder PgOutputFolderLeave

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "PortugueseBR"

; ---------- página: pasta de saída ----------
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

; ---------- detecção do Ghostscript ----------
Function DetectGhostscript
    StrCpy $GhostscriptPath ""

    ; tenta versões comuns em ordem decrescente
    !macro TryGS ver
        ${If} $GhostscriptPath == ""
            ${If} ${FileExists} "C:\Program Files\gs\gs${ver}\bin\gswin64c.exe"
                StrCpy $GhostscriptPath "C:\Program Files\gs\gs${ver}\bin\gswin64c.exe"
            ${EndIf}
        ${EndIf}
    !macroend

    !insertmacro TryGS "10.07.1"
    !insertmacro TryGS "10.07.0"
    !insertmacro TryGS "10.06.0"
    !insertmacro TryGS "10.05.1"
    !insertmacro TryGS "10.05.0"
    !insertmacro TryGS "10.04.0"
    !insertmacro TryGS "10.03.1"
    !insertmacro TryGS "10.03.0"
    !insertmacro TryGS "10.02.1"
    !insertmacro TryGS "10.02.0"
    !insertmacro TryGS "10.01.2"
    !insertmacro TryGS "10.01.1"
    !insertmacro TryGS "10.01.0"
    !insertmacro TryGS "10.00.0"
    !insertmacro TryGS "9.56.1"
    !insertmacro TryGS "9.56.0"
    !insertmacro TryGS "9.55.0"
    !insertmacro TryGS "9.54.0"
    !insertmacro TryGS "9.53.3"
    !insertmacro TryGS "9.52"
    !insertmacro TryGS "9.50"

    ${If} $GhostscriptPath == ""
        MessageBox MB_OK|MB_ICONEXCLAMATION \
            "Ghostscript nao foi encontrado em 'C:\Program Files\gs\'.$\n$\nBaixe e instale o Ghostscript antes de prosseguir:$\nhttps://www.ghostscript.com/releases/gsdnld.html"
        Abort
    ${EndIf}
FunctionEnd

; ---------- instalação ----------
Section "Instalar Meddrive Printer" SecInstall

    Call DetectGhostscript

    ; espelha estrutura do repositório: DLL na raiz, installer/ com os scripts
    ; install.ps1 usa "$ScriptDir\..\meddrivemon.dll" para localizar a DLL
    SetOutPath "$INSTDIR"
    File "..\meddrivemon.dll"
    SetOutPath "$INSTDIR\installer"
    File "MEDDRIVE.PPD"
    File "install.ps1"

    ; monta o caminho completo do PDF de saída
    StrCpy $1 "$OutputFolder\saida.pdf"

    ; executa o instalador PowerShell com os parâmetros configurados
    DetailPrint "Executando instalador PowerShell..."
    nsExec::ExecToLog 'powershell.exe -ExecutionPolicy Bypass -NonInteractive -File "$INSTDIR\installer\install.ps1" -OutputPath "$1" -GhostscriptPath "$GhostscriptPath" -PrinterName "$PrinterName"'
    Pop $0

    ; limpa temporários
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
    DetailPrint "  Ghostscript: $GhostscriptPath"

SectionEnd
