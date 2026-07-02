; Meddrive Printer — instalador NSIS para Windows Vista x64 SP2
; Requer: makensis (NSIS >= 3.0)
; Gera:   MeddrivePrinter-Vista-Setup.exe
;
; Pré-requisitos verificados em .onInit:
;   - Windows Vista SP2 ou superior (build >= 6002)
;   - PowerShell 2.0 — instalado automaticamente via deps\Windows6.0-KB968930-x64.msu se ausente
; Ghostscript: 9.18 (sem dependência de UCRT — apenas DLLs nativas do Vista)

Target amd64-unicode
Unicode True

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

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
  • Ghostscript 9.18 (motor de conversão PDF)$\r$\n\
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
    StrCpy $R4 "$R4$\r$\nGhostscript 9.18 (motor PDF):$\r$\n"
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
    ; Chave presente desde o PS 1.0; dígito principal "1" = PS 1.x, ausente = não instalado.
    ; A instalação em si ocorre no Section (onde a tela de progresso já está visível).
    ; Aqui apenas detectamos e pedimos confirmação ao usuário, guardando o resultado em $R9.
    StrCpy $R9 "0"   ; flag: 0 = PS ok, 1 = precisa instalar KB968930
    ReadRegStr $1 HKLM "SOFTWARE\Microsoft\PowerShell\1\PowerShellEngine" "PowerShellVersion"
    StrCpy $2 $1 1

    ${If} $2 == "1"
    ${OrIf} $2 == ""
        MessageBox MB_YESNO|MB_ICONQUESTION \
            "PowerShell 2.0 não está instalado (versão detectada: '$1').$\r$\n$\r$\n\
O instalador irá instalar o PowerShell 2.0 automaticamente.$\r$\n\
Uma reinicialização pode ser necessária após a instalação.$\r$\n$\r$\n\
Deseja continuar?" IDYES +2
            Abort
        StrCpy $R9 "1"
    ${EndIf}

FunctionEnd

; ---------- instalação ----------
Section "Instalar Meddrive Printer" SecInstall

    ; --- instala PowerShell 2.0 se necessário (flag definido em .onInit) ---
    ${If} $R9 == "1"
        DetailPrint "Preparando instalação do PowerShell 2.0..."
        InitPluginsDir
        File "/oname=$PLUGINSDIR\KB968930-x64.msu" "deps\Windows6.0-KB968930-x64.msu"
        DetailPrint "Instalando PowerShell 2.0 (KB968930) — aguarde, isso pode levar alguns minutos..."
        ExecWait '"$WINDIR\System32\wusa.exe" "$PLUGINSDIR\KB968930-x64.msu" /quiet /norestart' $3
        Delete "$PLUGINSDIR\KB968930-x64.msu"

        ${If} $3 = 3010
            DetailPrint "PowerShell 2.0 instalado — reinicialização necessária para concluir."
            MessageBox MB_OK|MB_ICONINFORMATION \
                "PowerShell 2.0 foi instalado com sucesso.$\r$\n$\r$\n\
Uma reinicialização é necessária para concluir.$\r$\n\
Reinicie o computador e execute o instalador novamente."
            Quit
        ${ElseIf} $3 <> 0
            DetailPrint "ERRO: falha ao instalar PowerShell 2.0 (código $3)."
            MessageBox MB_OK|MB_ICONSTOP \
                "Falha ao instalar PowerShell 2.0 (código $3).$\r$\n$\r$\n\
Instale manualmente via Windows Update (KB968930) e tente novamente."
            Abort
        ${Else}
            DetailPrint "PowerShell 2.0 instalado com sucesso."
        ${EndIf}
    ${EndIf}

    ; extrai DLL e scripts para diretório temporário
    SetOutPath "$INSTDIR"
    File "..\..\meddrivemon.dll"
    SetOutPath "$INSTDIR\installer"
    File "..\win7\MEDDRIVE.PPD"
    File "..\win7\install_helper.exe"
    File "..\win10-11\x64\Debug\MedDriveManager.exe"
    File "..\..\src\agent\MeddrivePrinterAgent.exe"
    SetOutPath "$INSTDIR\installer\conf"
    File "..\win7\conf\add-printer.ps1"
    File "..\win7\conf\create-profile.ps1"
    File "..\win7\conf\edit-profile.ps1"
    File "..\win7\conf\edit-printer.ps1"
    File "..\win7\conf\remove-printer.ps1"
    File "..\win7\conf\remove-profile.ps1"

    ; lê ProgramData do ambiente Windows em tempo de execução
    ReadEnvStr $R0 "ProgramData"

    ; extrai Ghostscript 9.18 para ProgramData
    ; GS 9.18 depende apenas de DLLs do sistema — sem UCRT ou runtime externo
    SetOutPath "$R0\Meddrive Printer\Ghostscript\bin"
    File /r "..\..\gs\ghostscript-vista\bin\*"
    SetOutPath "$R0\Meddrive Printer\Ghostscript\lib"
    File /r "..\..\gs\ghostscript-vista\lib\*"

    DetailPrint "Executando instalador..."
    nsExec::ExecToLog '"$INSTDIR\installer\install_helper.exe" "$INSTDIR\installer"'
    Pop $0

    DetailPrint "Registrando agente no Task Scheduler (schtasks)..."
    nsExec::ExecToLog 'schtasks /Create /TN "MeddrivePrinterAgent" /TR "\"$R0\Meddrive Printer\MeddrivePrinterAgent.exe\"" /SC ONLOGON /F'
    Pop $1
    ${If} $1 != 0
        DetailPrint "AVISO: falha ao criar a tarefa MeddrivePrinterAgent (codigo $1)"
    ${EndIf}
    ; inicia o agente agora, sem exigir logout/login
    nsExec::ExecToLog 'schtasks /Run /TN "MeddrivePrinterAgent"'

    ; limpa temporários (arquivos já copiados para ProgramData pelo install_helper.exe)
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
            "A instalação falhou.$\n$\nConsulte o log em:$\nC:\Windows\Temp\meddrive_install.log"
        SetErrorLevel 1
        Quit
    ${EndIf}

    DetailPrint "Instalação concluída."
    DetailPrint "  Aplicativo : $R0\Meddrive Printer\MedDriveManager.exe"
    DetailPrint "  Ghostscript: $R0\Meddrive Printer\Ghostscript\bin\gswin64c.exe (v9.18)"

SectionEnd
