#Requires -RunAsAdministrator

# Adiciona uma nova impressora Meddrive numa máquina onde install.ps1 já rodou
# (DLL em System32, monitor e driver registrados). Responsável apenas pela
# criação da instância de impressora: porta, AddPortExW e AddPrinterW.

param(
    [string]$OutputPath  = "C:\Users\favaro\Desktop\PDF\saida.pdf",
    [string]$PrinterName = "Meddrive Printer"
)

trap {
    Write-Output "EXCEPTION TYPE: $($_.Exception.GetType().FullName)"
    Write-Output "EXCEPTION MSG : $($_.Exception.Message)"
    Write-Output "LINE          : $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
    Write-Output "STACK         : $($_.ScriptStackTrace)"
    exit 1
}
function Trace-Step($msg) { Write-Output "CHECKPOINT: $msg" }

Trace-Step "inicio do script"
Start-Transcript -Path "C:\Windows\Temp\meddrive_ps_addprinter.log" -Force
Trace-Step "Start-Transcript OK"

$GhostscriptPath = "$env:ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe"
$ErrorActionPreference = "Stop"

# ── Pré-requisitos (devem ter sido instalados pelo install.ps1) ───────────
$DllPath = "$env:SystemRoot\System32\meddrivemon.dll"
if (-not (Test-Path $DllPath)) {
    Write-Host "ERRO: meddrivemon.dll não encontrada em $DllPath. Execute o instalador principal antes de adicionar impressoras."
    exit 1
}
Trace-Step "DLL encontrada em $DllPath"

$MonitorName = "Meddrive Printer MONITOR"
$DriverName  = "Meddrive Printer DRIVER"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"

if (-not (Test-Path $MonitorReg)) {
    Write-Host "ERRO: monitor '$MonitorName' não encontrado no registry. Execute o instalador principal antes de adicionar impressoras."
    exit 1
}
Trace-Step "monitor encontrado no registry"

if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Write-Host "ERRO: driver '$DriverName' não encontrado. Execute o instalador principal antes de adicionar impressoras."
    exit 1
}
Trace-Step "driver encontrado"

# ── Configuração da porta ─────────────────────────────────────────────────
$portSuffix = $PrinterName -replace 'Meddrive Printer', '' -replace '-', '' -replace '\s', ''
$PortName   = if ($portSuffix) { "Meddrive Printer PORT $portSuffix" } else { "Meddrive Printer PORT" }
$PortReg    = "$MonitorReg\Ports\$PortName"

Write-Host "Configurando porta..."
Trace-Step "registrando porta em $PortReg"
New-Item -Path $PortReg -Force | Out-Null
Set-ItemProperty -Path $PortReg -Name "OutputPath"      -Value $OutputPath      -Type String
Set-ItemProperty -Path $PortReg -Name "GhostscriptPath" -Value $GhostscriptPath -Type String
Trace-Step "porta configurada"

# Garante que a pasta de destino existe
$outputDir = Split-Path -Parent $OutputPath
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

# ── Spooler ───────────────────────────────────────────────────────────────
Write-Host "Iniciando o Spooler..."
Start-Service -Name Spooler

Write-Host "Aguardando o Spooler carregar o monitor..."
Start-Sleep -Seconds 5

$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Write-Host "ERRO: Spooler não está em execução (status: $spoolerStatus)"
    exit 1
}
if (-not (Test-Path $PortReg)) {
    Write-Host "ERRO: porta não encontrada no registry ($PortReg)"
    exit 1
}
Write-Host "  OK - Spooler em execução, porta no registry"

# ── Registra a porta via AddPortExW ──────────────────────────────────────
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

# ── Registra a impressora via AddPrinterW ─────────────────────────────────
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
$pi2.pDatatype       = "RAW"
$pi2.Attributes      = 0x40  # PRINTER_ATTRIBUTE_LOCAL

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

Write-Host ""
if ($success) {
    Write-Host "Instalação concluída!"
    Write-Host "  Impressora : $PrinterName"
    Write-Host "  Porta      : $PortName"
    Write-Host "  Saída      : $OutputPath"
    Write-Host "  Ghostscript: $GhostscriptPath"
} else {
    Write-Host "ERRO: falha ao registrar a impressora '$PrinterName'."
    Write-Host "  Motivo     : $erroMsg"
    Write-Host "  Log da DLL : C:\Windows\Temp\meddrivemon_init.log"
    exit 1
}
