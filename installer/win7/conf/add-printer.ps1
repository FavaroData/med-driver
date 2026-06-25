# Compativel com Windows 7 x64 e PowerShell 2.0+
# Nao usa modulo PrintManagement (ausente no Windows 7 sem RSAT)

param(
    [string]$ProfileName,
    [string]$PrinterName = "Meddrive Printer"
)

$LogFile   = "C:\Windows\Temp\meddrive_manager.log"
$LogWriter = New-Object System.IO.StreamWriter($LogFile, $true, [System.Text.Encoding]::Unicode)
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

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win32AddPrinter {
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
    [StructLayout(LayoutKind.Sequential)]
    public struct DRIVER_INFO_1 { public IntPtr pName; }
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool EnumPrinterDrivers(string pName, string pEnv, uint Level, IntPtr pBuf, uint cbBuf, ref uint pcbNeeded, ref uint pcReturned);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool OpenPrinter(string pPrinterName, out IntPtr phPrinter, IntPtr pDefault);
    [DllImport("winspool.drv", SetLastError=true)]
    public static extern bool ClosePrinter(IntPtr hPrinter);
    [DllImport("winspool.drv", SetLastError=true)]
    public static extern bool DeletePrinter(IntPtr hPrinter);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern IntPtr AddPrinter(string pName, uint Level, ref PRINTER_INFO_2 pPrinter);
}
"@ -ErrorAction SilentlyContinue

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

$needed = [uint32]0; $returned = [uint32]0
[Win32AddPrinter]::EnumPrinterDrivers($null, "Windows x64", 1, [IntPtr]::Zero, 0, [ref]$needed, [ref]$returned) | Out-Null
$driverFound = $false
if ($needed -gt 0) {
    $buf = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([int]$needed)
    try {
        if ([Win32AddPrinter]::EnumPrinterDrivers($null, "Windows x64", 1, $buf, $needed, [ref]$needed, [ref]$returned)) {
            $sz = [System.Runtime.InteropServices.Marshal]::SizeOf([type][Win32AddPrinter+DRIVER_INFO_1])
            for ($i = 0; $i -lt [int]$returned; $i++) {
                $ptr  = [IntPtr]($buf.ToInt64() + $i * $sz)
                $info = [System.Runtime.InteropServices.Marshal]::PtrToStructure($ptr, [type][Win32AddPrinter+DRIVER_INFO_1])
                if ([System.Runtime.InteropServices.Marshal]::PtrToStringUni($info.pName) -eq $DriverName) { $driverFound = $true; break }
            }
        }
    } finally { [System.Runtime.InteropServices.Marshal]::FreeHGlobal($buf) }
}
if (-not $driverFound) {
    Log "[ERRO] Driver '$DriverName' nao encontrado. Execute o instalador principal antes de adicionar impressoras."
    $LogWriter.Close()
    exit 1
}

$PortName = "Meddrive Printer PORT $ProfileName"
$PortReg  = "$MonitorReg\Ports\$PortName"

if (-not (Test-Path $PortReg)) {
    Log "[ERRO] Perfil '$ProfileName' nao encontrado no registry ($PortReg)."
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

$hExisting = [IntPtr]::Zero
if ([Win32AddPrinter]::OpenPrinter($PrinterName, [ref]$hExisting, [IntPtr]::Zero)) {
    Log "[INFO] Impressora '$PrinterName' ja existe, removendo para recriar..."
    [Win32AddPrinter]::DeletePrinter($hExisting) | Out-Null
    [Win32AddPrinter]::ClosePrinter($hExisting) | Out-Null
    Start-Sleep -Seconds 2
}

$pi2                 = New-Object Win32AddPrinter+PRINTER_INFO_2
$pi2.pPrinterName    = $PrinterName
$pi2.pPortName       = $PortName
$pi2.pDriverName     = $DriverName
$pi2.pPrintProcessor = "winprint"
$pi2.pDatatype       = "RAW"
$pi2.Attributes      = 0x40

Log "[INFO] Registrando impressora via AddPrinterW..."

$maxAttempts = 5; $attempt = 0; $success = $false; $erroMsg = ""
while ($attempt -lt $maxAttempts -and -not $success) {
    $hPrinter = [Win32AddPrinter]::AddPrinter($null, 2, [ref]$pi2)
    if ($hPrinter -ne [IntPtr]::Zero) {
        [Win32AddPrinter]::ClosePrinter($hPrinter) | Out-Null
        $success = $true
    } else {
        $attempt++
        $erroMsg = "Win32 erro $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error())"
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
