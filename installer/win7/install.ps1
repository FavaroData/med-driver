# Compativel com Windows 7 x64 e PowerShell 2.0+
# Nao usa modulo PrintManagement (ausente no Windows 7 sem RSAT)
param(
    [string]$OutputPath  = "C:\Users\favaro\Desktop\PDF\saida.pdf",
    [string]$PrinterName = "Meddrive Printer"
)

$GhostscriptPath = "$env:ProgramFiles\Meddrive Printer\Ghostscript\bin\gswin64c.exe"

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DllSource = Join-Path $ScriptDir "..\meddrivemon.dll"
$DllDest   = "$env:SystemRoot\System32\meddrivemon.dll"

$MonitorName = "Meddrive Printer MONITOR"
$DriverName  = "Meddrive Printer DRIVER"

$portSuffix = $PrinterName -replace 'Meddrive Printer', '' -replace '-', '' -replace '\s', ''
$PortName   = if ($portSuffix) { "Meddrive Printer PORT $portSuffix" } else { "Meddrive Printer PORT" }

$MonitorReg = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$PortReg    = "$MonitorReg\Ports\$PortName"
$DriverKey  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Environments\Windows x64\Drivers\Version-3\$DriverName"

# P/Invoke direto em winspool.drv — substitui os cmdlets do modulo PrintManagement
# que nao existem no Windows 7 sem RSAT (Get-PrinterDriver, Remove-Printer, etc.)
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class Win32Print {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct PORT_INFO_1 {
        public string pName;
    }
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
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool AddPortEx(string pName, uint Level, ref PORT_INFO_1 lpBuffer, string lpMonitorName);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern IntPtr AddPrinter(string pName, uint Level, ref PRINTER_INFO_2 pPrinter);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool OpenPrinter(string pPrinterName, out IntPtr phPrinter, IntPtr pDefault);
    [DllImport("winspool.drv", SetLastError=true)]
    public static extern bool ClosePrinter(IntPtr hPrinter);
    [DllImport("winspool.drv", SetLastError=true)]
    public static extern bool DeletePrinter(IntPtr hPrinter);
}
"@ -ErrorAction SilentlyContinue

Write-Host "Parando o Spooler..."
Stop-Service -Name Spooler -Force
$p = Get-Process -Name spoolsv -ErrorAction SilentlyContinue
if ($p) { $p.WaitForExit() }

Write-Host "Copiando DLL para System32..."
if (-not (Test-Path $DllSource)) {
    Write-Host "ERRO: DLL nao encontrada em $DllSource"
    exit 1
}
Copy-Item $DllSource $DllDest -Force

Write-Host "Registrando monitor no registry..."
if (-not (Test-Path $MonitorReg)) {
    New-Item -Path $MonitorReg | Out-Null
}
Set-ItemProperty -Path $MonitorReg -Name "Driver" -Value "meddrivemon.dll" -Type String

Write-Host "Configurando porta..."
New-Item -Path $PortReg -Force | Out-Null
Set-ItemProperty -Path $PortReg -Name "OutputPath"      -Value $OutputPath      -Type String
Set-ItemProperty -Path $PortReg -Name "GhostscriptPath" -Value $GhostscriptPath -Type String

$outputDir = Split-Path -Parent $OutputPath
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

Write-Host "Iniciando o Spooler..."
Start-Service -Name Spooler
Start-Sleep -Seconds 5

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

Write-Host "Instalando driver PSCRIPT5 customizado..."
if (-not (Test-Path $DriverKey)) {
    New-Item -Path $DriverKey -Force | Out-Null
    Set-ItemProperty $DriverKey -Name "Configuration File"      -Value "PS5UI.DLL"
    Set-ItemProperty $DriverKey -Name "Data File"               -Value "PSCRIPT.NTF"
    Set-ItemProperty $DriverKey -Name "Driver"                  -Value "PSCRIPT5.DLL"
    Set-ItemProperty $DriverKey -Name "Help File"               -Value "PSCRIPT.HLP"
    Set-ItemProperty $DriverKey -Name "Driver Version"          -Value 3 -Type DWord
    Set-ItemProperty $DriverKey -Name "Version"                 -Value 3 -Type DWord
    # PRINTER_DRIVER_XPS (0x2) — habilita o Print Ticket Provider do PSCRIPT5.
    Set-ItemProperty $DriverKey -Name "PrinterDriverAttributes" -Value 2 -Type DWord
    Write-Host "  OK - driver '$DriverName' registrado via registry"
} else {
    Write-Host "  OK - driver '$DriverName' ja instalado"
}

Write-Host "Instalando PPD do driver..."
$PpdSource = Join-Path $ScriptDir "MEDDRIVE.PPD"
$PpdDest   = "C:\Windows\System32\spool\drivers\x64\3\MEDDRIVE.PPD"
if (-not (Test-Path $PpdSource)) {
    Write-Host "ERRO: MEDDRIVE.PPD nao encontrado em $PpdSource"
    exit 1
}
Copy-Item $PpdSource $PpdDest -Force
Set-ItemProperty $DriverKey -Name "Dependent Files" -Value @("MEDDRIVE.PPD", "") -Type MultiString
Write-Host "  OK - PPD copiado e registrado em Dependent Files"

Write-Host "Reiniciando o Spooler para enumerar o driver..."
Restart-Service -Name Spooler -Force
Start-Sleep -Seconds 3

# Verifica driver via registry (sem Get-PrinterDriver que exige PrintManagement)
if (-not (Test-Path $DriverKey)) {
    Write-Host "ERRO: chave do driver nao encontrada no registry apos reinicio"
    exit 1
}
Write-Host "  OK - driver '$DriverName' reconhecido no registry"

Write-Host "Registrando porta via AddPortExW..."
$pi1       = New-Object Win32Print+PORT_INFO_1
$pi1.pName = $PortName
$portOk = [Win32Print]::AddPortEx($null, 1, [ref]$pi1, $MonitorName)
if (-not $portOk) {
    $portErr = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Write-Host "  AVISO: AddPortExW falhou (Win32 erro $portErr)"
} else {
    Write-Host "  OK - porta registrada via AddPortExW"
}

# Remove impressora existente com mesmo nome via OpenPrinter + DeletePrinter
# Substitui Remove-Printer que nao existe no Windows 7 sem PrintManagement
$hExisting = [IntPtr]::Zero
if ([Win32Print]::OpenPrinter($PrinterName, [ref]$hExisting, [IntPtr]::Zero)) {
    [Win32Print]::DeletePrinter($hExisting) | Out-Null
    [Win32Print]::ClosePrinter($hExisting) | Out-Null
    Write-Host "  Impressora '$PrinterName' removida para reinstalacao"
}

$pi2                 = New-Object Win32Print+PRINTER_INFO_2
$pi2.pPrinterName    = $PrinterName
$pi2.pPortName       = $PortName
$pi2.pDriverName     = $DriverName
$pi2.pPrintProcessor = "winprint"
$pi2.pDatatype       = "RAW"
$pi2.Attributes      = 0x40

Write-Host "Registrando impressora via AddPrinterW..."
Write-Host "  pPrinterName   : $($pi2.pPrinterName)"
Write-Host "  pPortName      : $($pi2.pPortName)"
Write-Host "  pDriverName    : $($pi2.pDriverName)"
Write-Host "  pPrintProcessor: $($pi2.pPrintProcessor)"
Write-Host "  pDatatype      : $($pi2.pDatatype)"

$maxAttempts = 5
$attempt     = 0
$success     = $false
$erroMsg     = ""
while ($attempt -lt $maxAttempts -and -not $success) {
    $hPrinter = [Win32Print]::AddPrinter($null, 2, [ref]$pi2)
    if ($hPrinter -ne [IntPtr]::Zero) {
        [Win32Print]::ClosePrinter($hPrinter) | Out-Null
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
    Write-Host "Instalacao concluida!"
    Write-Host "  Impressora : $PrinterName"
    Write-Host "  Porta      : $PortName"
    Write-Host "  Saida      : $OutputPath"
    Write-Host "  Ghostscript: $GhostscriptPath"
} else {
    Write-Host "ERRO: instalacao falhou ao registrar a impressora '$PrinterName'."
    Write-Host "  Motivo     : $erroMsg"
    Write-Host "  Log da DLL : C:\Windows\Temp\meddrivemon_init.log"
    exit 1
}
