#Requires -RunAsAdministrator

# Adiciona uma nova impressora Meddrive vinculada a um perfil existente.
# O perfil deve ter sido criado previamente com create-profile.ps1.

param(
    [string]$ProfileName,
    [string]$PrinterName = "Meddrive Printer"
)

$LogFile   = "C:\Windows\Temp\meddrive_manager.log"
$LogWriter = [System.IO.StreamWriter]::new($LogFile, $true, [System.Text.Encoding]::Unicode)
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Log($msg) {
    [Console]::Out.WriteLine($msg)
    $LogWriter.WriteLine($msg)
    $LogWriter.Flush()
}

trap {
    Log "EXCEPTION TYPE: $($_.Exception.GetType().FullName)"
    Log "EXCEPTION MSG : $($_.Exception.Message)"
    Log "LINE          : $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
    Log "STACK         : $($_.ScriptStackTrace)"
    $LogWriter.Close()
    exit 1
}

Log ""
Log "=== [$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] add-printer ==="

if (-not $ProfileName) {
    Log "[ERRO] Parametro -ProfileName e obrigatorio."
    $LogWriter.Close()
    exit 1
}

$ErrorActionPreference = "Stop"

$DllPath = "$env:SystemRoot\System32\meddrivemon.dll"
if (-not (Test-Path $DllPath)) {
    Log "[ERRO] meddrivemon.dll nao encontrada em $DllPath. Execute o instalador principal antes de adicionar impressoras."
    $LogWriter.Close()
    exit 1
}

$MonitorName = "Meddrive Printer MONITOR"
$DriverName  = "Meddrive Printer DRIVER"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"

if (-not (Test-Path $MonitorReg)) {
    Log "[ERRO] Monitor '$MonitorName' nao encontrado no registry. Execute o instalador principal antes de adicionar impressoras."
    $LogWriter.Close()
    exit 1
}

if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Log "[ERRO] Driver '$DriverName' nao encontrado. Execute o instalador principal antes de adicionar impressoras."
    $LogWriter.Close()
    exit 1
}

$PortName = "Meddrive Printer PORT $ProfileName"
$PortReg  = "$MonitorReg\Ports\$PortName"

if (-not (Test-Path $PortReg)) {
    Log "[ERRO] Perfil '$ProfileName' nao encontrado no registry ($PortReg). Crie o perfil com create-profile.ps1 antes de adicionar impressoras."
    $LogWriter.Close()
    exit 1
}

Log "[INFO] Iniciando Spooler..."
Start-Service -Name Spooler
Start-Sleep -Seconds 5

$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Log "[ERRO] Spooler nao esta em execucao (status: $spoolerStatus)"
    $LogWriter.Close()
    exit 1
}

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
$pi2.Attributes      = 0x40

Log "[INFO] Registrando impressora via AddPrinterW..."

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
        Log "[INFO] Tentativa $attempt/$maxAttempts falhou: $erroMsg"
        if ($attempt -lt $maxAttempts) { Start-Sleep -Seconds 3 }
    }
}

if ($success) {
    Log "[OK] Impressora adicionada: $PrinterName | perfil: $ProfileName"
} else {
    Log "[ERRO] Falha ao registrar a impressora '$PrinterName': $erroMsg"
    $LogWriter.Close()
    exit 1
}

$LogWriter.Close()
