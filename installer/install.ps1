#Requires -RunAsAdministrator

# Parâmetros de saída
param(
    [string]$OutputPath      = "C:\Users\favaro\Desktop\PDF\saida.pdf",
    [string]$GhostscriptPath = "C:\Program Files\gs\gs10.07.1\bin\gswin64c.exe"
)

# Configurações para o script de instalação do monitor 
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DllSource = Join-Path $ScriptDir "..\pdfmonitor.dll"
$DllDest   = "$env:SystemRoot\System32\pdfmonitor.dll"

# Configurações do driver, monitor e porta
$MonitorName = "MedMonitor"
$PortName    = "MedPort"
$PrinterName = "MedPrinter"
$DriverName = "Med PDF Printer"

# Regs 
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$PortReg     = "$MonitorReg\Ports\$PortName"

# Verificações
Write-Host "Parando o Spooler..."
Stop-Service -Name Spooler -Force
$p = Get-Process -Name spoolsv -ErrorAction SilentlyContinue
if ($p) { $p.WaitForExit() }

Write-Host "Copiando DLL para System32..."
if (-not (Test-Path $DllSource)) {
    Write-Host "ERRO: DLL não encontrada em $DllSource"
    exit 1
}
Copy-Item $DllSource $DllDest -Force

# Registrando o monitor e a porta no registry
Write-Host "Registrando monitor no registry..."
New-Item -Path $MonitorReg -Force | Out-Null
Set-ItemProperty -Path $MonitorReg -Name "Driver" -Value "pdfmonitor.dll" -Type String

Write-Host "Configurando porta..."
New-Item -Path $PortReg -Force | Out-Null
Set-ItemProperty -Path $PortReg -Name "OutputPath"      -Value $OutputPath      -Type String
Set-ItemProperty -Path $PortReg -Name "GhostscriptPath" -Value $GhostscriptPath -Type String

# Garante que a pasta de destino existe, se não, cria a pasta
$outputDir = Split-Path -Parent $OutputPath
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

# Instalação do driver e impressora virtual
Write-Host "Iniciando o Spooler..."
Start-Service -Name Spooler

Write-Host "Aguardando o Spooler carregar o monitor..."
Start-Sleep -Seconds 5

# Win32 EnumPorts via winspool.drv retorna ERROR_INVALID_DATA (13) para monitores customizados
# no Windows 10/11 — falha de validacao de ponteiro no merge RPC do spooler.
# O Add-Printer usa o caminho interno do spooler que funciona corretamente.
# Verificamos apenas registry + status do servico como pre-condicao minima.
$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Write-Host "ERRO: Spooler nao esta em execucao (status: $spoolerStatus)"
    exit 1
}
if (-not (Test-Path $MonitorReg)) {
    Write-Host "ERRO: monitor nao encontrado no registry ($MonitorReg)"
    exit 1
}
if (-not (Test-Path $PortReg)) {
    Write-Host "ERRO: porta nao encontrada no registry ($PortReg)"
    exit 1
}
Write-Host "  OK - Spooler em execucao, monitor e porta no registry"

# Garante que 'Generic / Text Only' está presente — driver built-in do Windows usado como base.
Write-Host "Verificando driver 'Generic / Text Only'..."
if (-not (Get-PrinterDriver -Name "Generic / Text Only" -ErrorAction SilentlyContinue)) {
    Write-Host "  Instalando 'Generic / Text Only'..."
    Add-PrinterDriver -Name "Generic / Text Only" -ErrorAction Stop
    Write-Host "  OK - instalado"
} else {
    Write-Host "  OK - ja presente"
}

# Instalação do driver PSCRIPT5 via registry — sem INF próprio e sem assinatura digital.
# Não usa drivers inbox (PS Class Driver, Print To PDF) pois o Windows 10/11 bloqueia
# drivers inbox com port monitors de terceiros (erro ID=242 no PrintService).
# PSCRIPT5 já está instalado e assinado pela Microsoft em System32\spool\drivers\x64\3\.
# Registramos um driver customizado apontando para os arquivos existentes do PSCRIPT5
# diretamente no registry do spooler — método que não exige assinatura adicional.
Write-Host "Instalando driver PSCRIPT5 customizado..."
$driverKey = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Environments\Windows x64\Drivers\Version-3\$DriverName"
if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    New-Item -Path $driverKey -Force | Out-Null
    Set-ItemProperty $driverKey -Name "Configuration File"      -Value "PS5UI.DLL"
    Set-ItemProperty $driverKey -Name "Data File"               -Value "PSCRIPT.NTF"
    Set-ItemProperty $driverKey -Name "Driver"                  -Value "PSCRIPT5.DLL"
    Set-ItemProperty $driverKey -Name "Help File"               -Value "PSCRIPT.HLP"
    Set-ItemProperty $driverKey -Name "Driver Version"          -Value 3 -Type DWord
    Set-ItemProperty $driverKey -Name "Version"                 -Value 3 -Type DWord
    # PRINTER_DRIVER_XPS (0x2) — habilita o Print Ticket Provider do PSCRIPT5.
    # Sem esse flag PTGetPrintCapabilities retorna E_FAIL e o Edge nao carrega o preview.
    Set-ItemProperty $driverKey -Name "PrinterDriverAttributes" -Value 2 -Type DWord
    Write-Host "  OK - driver '$DriverName' registrado via registry"
} else {
    Write-Host "  OK - driver '$DriverName' ja instalado"
}

# Copia o PPD e registra em Dependent Files ANTES de reiniciar o spooler —
# PSCRIPT5 abandona o job silenciosamente sem PPD.
Write-Host "Instalando PPD do driver..."
$PpdSource = Join-Path $ScriptDir "MEDPDF.PPD"
$PpdDest   = "C:\Windows\System32\spool\drivers\x64\3\MEDPDF.PPD"
if (-not (Test-Path $PpdSource)) {
    Write-Host "ERRO: MEDPDF.PPD nao encontrado em $PpdSource"
    exit 1
}
Copy-Item $PpdSource $PpdDest -Force
Set-ItemProperty $driverKey -Name "Dependent Files" -Value @("MEDPDF.PPD", "") -Type MultiString
Write-Host "  OK - PPD copiado e registrado em Dependent Files"

# O spooler so enumera drivers injetados via registry no startup. Gravar as chaves
# com ele em execucao nao basta — e preciso reiniciar para que ele leia a chave do
# driver (com o PPD ja no lugar) e o reconheca. Um Start-Sleep nao dispara isso.
Write-Host "Reiniciando o Spooler para enumerar o driver..."
Restart-Service -Name Spooler -Force
Start-Sleep -Seconds 3
if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Write-Host "ERRO: driver '$DriverName' nao reconhecido pelo spooler apos registro"
    exit 1
}
Write-Host "  OK - driver '$DriverName' reconhecido pelo spooler"

# Registra a porta no spooler via AddPortExW antes de chamar AddPrinterW.
# AddPrinterW valida a porta chamando EnumPorts cliente, que retorna ERROR_INVALID_DATA (13)
# para monitores customizados devido a falha de ponteiro no merge RPC.
# AddPortExW usa caminho diferente no spooler (chama pfnAddPortEx no monitor) e contorna o problema.
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class PortRegistrar {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct PORT_INFO_1 {
        public string pName;
    }
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool AddPortEx(string pName, uint Level, ref PORT_INFO_1 lpBuffer, string lpMonitorName);
}
" -ErrorAction SilentlyContinue

$pi1       = New-Object PortRegistrar+PORT_INFO_1
$pi1.pName = $PortName
Write-Host "Registrando porta via AddPortExW..."
$portOk = [PortRegistrar]::AddPortEx($null, 1, [ref]$pi1, $MonitorName)
if (-not $portOk) {
    $portErr = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Write-Host "  AVISO: AddPortExW falhou (Win32 erro $portErr)"
} else {
    Write-Host "  OK - porta registrada via AddPortExW"
}

# P/Invoke direto ao AddPrinterW — bypassa CIM/WMI e expoe o codigo Win32 exato.
# Add-Printer usava o caminho CIM que retornava so a string localizada do erro,
# impedindo diagnostico. Aqui controlamos todos os campos de PRINTER_INFO_2W.
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class PrinterWin32 {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct PRINTER_INFO_2 {
        public string pServerName;
        public string pPrinterName;
        public string pShareName;
        public string pPortName;
        public string pDriverName;
        public string pComment;
        public string pLocation;
        public IntPtr pDevMode;
        public string pSepFile;
        public string pPrintProcessor;
        public string pDatatype;
        public string pParameters;
        public IntPtr pSecurityDescriptor;
        public uint   Attributes;
        public uint   Priority;
        public uint   DefaultPriority;
        public uint   StartTime;
        public uint   UntilTime;
        public uint   Status;
        public uint   cJobs;
        public uint   AveragePPM;
    }
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern IntPtr AddPrinter(string pName, uint Level, ref PRINTER_INFO_2 pPrinter);
    [DllImport(""winspool.drv"", SetLastError=true)]
    public static extern bool ClosePrinter(IntPtr hPrinter);
}
" -ErrorAction SilentlyContinue

$pi2                 = New-Object PrinterWin32+PRINTER_INFO_2
$pi2.pPrinterName    = $PrinterName
$pi2.pPortName       = $PortName
$pi2.pDriverName     = $DriverName
$pi2.pPrintProcessor = "winprint"
$pi2.pDatatype       = "RAW"    # PSCRIPT5 entrega PS como RAW ao port monitor
$pi2.Attributes      = 0x40  # PRINTER_ATTRIBUTE_LOCAL = 64

Write-Host "Registrando impressora via AddPrinterW..."
Write-Host "  pPrinterName   : $($pi2.pPrinterName)"
Write-Host "  pPortName      : $($pi2.pPortName)"
Write-Host "  pDriverName    : $($pi2.pDriverName)"
Write-Host "  pPrintProcessor: $($pi2.pPrintProcessor)"
Write-Host "  pDatatype      : $($pi2.pDatatype)"

if (Get-Printer -Name $PrinterName -ErrorAction SilentlyContinue) {
    Remove-Printer -Name $PrinterName
}

$maxAttempts = 5
$attempt     = 0
$success     = $false
$erroMsg     = ""
while ($attempt -lt $maxAttempts -and -not $success) {
    $hPrinter = [PrinterWin32]::AddPrinter($null, 2, [ref]$pi2)
    if ($hPrinter -ne [IntPtr]::Zero) {
        [PrinterWin32]::ClosePrinter($hPrinter) | Out-Null
        $success = $true
    } else {
        $attempt++
        $win32Err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
        $erroMsg  = "Win32 erro $win32Err"
        Write-Host "  Tentativa $attempt/$maxAttempts falhou: $erroMsg"
        if ($attempt -lt $maxAttempts) { Start-Sleep -Seconds 3 }
    }
}

# Saída final: só exibe sucesso se todos os passos anteriores deram certo
Write-Host ""
if ($success) {
    Write-Host "Instalacao concluida!"
    Write-Host "  Impressora : $PrinterName"
    Write-Host "  Porta      : $PortName"
    Write-Host "  Saida      : $OutputPath"
    Write-Host "  Ghostscript: $GhostscriptPath"
} else {
    Write-Host "ERRO: instalacao falhou ao registrar a impressora '$PrinterName'."
    Write-Host "  Motivo     : $erroMsg"
    Write-Host "  Log da DLL : C:\Windows\Temp\pdfmonitor_init.log"
    exit 1
}
