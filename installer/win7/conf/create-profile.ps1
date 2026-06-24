# Compativel com Windows 7 x64 e PowerShell 2.0+
# Nao usa modulo PrintManagement (ausente no Windows 7 sem RSAT)

param(
    [string]$ProfileName,
    [string]$OutputPath,
    [string]$OutputBaseName,
    [switch]$OpenAfterGenerate,
    [switch]$OverwriteFile
)

$LogFile   = "C:\Windows\Temp\meddrive_ps_createprofile.log"
$LogWriter = New-Object System.IO.StreamWriter($LogFile, $false, [System.Text.Encoding]::Unicode)
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
function Trace-Step($msg) { Log "CHECKPOINT: $msg" }

Trace-Step "inicio do script"

if (-not $ProfileName)    { Log "ERRO: parametro -ProfileName e obrigatorio.";    $LogWriter.Close(); exit 1 }
if (-not $OutputPath)     { Log "ERRO: parametro -OutputPath e obrigatorio.";     $LogWriter.Close(); exit 1 }
if (-not $OutputBaseName) { Log "ERRO: parametro -OutputBaseName e obrigatorio."; $LogWriter.Close(); exit 1 }

$GhostscriptPath       = "$env:ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe"
$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win32CreateProfile {
    [StructLayout(LayoutKind.Sequential)]
    public struct DRIVER_INFO_1 { public IntPtr pName; }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct PORT_INFO_1   { public string pName; }
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool EnumPrinterDrivers(string pName, string pEnv, uint Level, IntPtr pBuf, uint cbBuf, ref uint pcbNeeded, ref uint pcReturned);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool AddPortEx(string pName, uint Level, ref PORT_INFO_1 lpBuffer, string lpMonitorName);
}
"@ -ErrorAction SilentlyContinue

# -- Pre-requisitos --------------------------------------------------------
$DllPath = "$env:SystemRoot\System32\meddrivemon.dll"
if (-not (Test-Path $DllPath)) {
    Log "ERRO: meddrivemon.dll nao encontrada em $DllPath."
    $LogWriter.Close(); exit 1
}
Trace-Step "DLL encontrada"

$MonitorName = "Meddrive Printer MONITOR"
$DriverName  = "Meddrive Printer DRIVER"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"

if (-not (Test-Path $MonitorReg)) {
    Log "ERRO: monitor '$MonitorName' nao encontrado no registry."
    $LogWriter.Close(); exit 1
}
Trace-Step "monitor encontrado"

# Verifica driver via EnumPrinterDrivers
$needed = [uint32]0; $returned = [uint32]0
[Win32CreateProfile]::EnumPrinterDrivers($null, "Windows x64", 1, [IntPtr]::Zero, 0, [ref]$needed, [ref]$returned) | Out-Null
$driverFound = $false
if ($needed -gt 0) {
    $buf = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([int]$needed)
    try {
        if ([Win32CreateProfile]::EnumPrinterDrivers($null, "Windows x64", 1, $buf, $needed, [ref]$needed, [ref]$returned)) {
            $sz = [System.Runtime.InteropServices.Marshal]::SizeOf([type][Win32CreateProfile+DRIVER_INFO_1])
            for ($i = 0; $i -lt [int]$returned; $i++) {
                $ptr  = [IntPtr]($buf.ToInt64() + $i * $sz)
                $info = [System.Runtime.InteropServices.Marshal]::PtrToStructure($ptr, [type][Win32CreateProfile+DRIVER_INFO_1])
                if ([System.Runtime.InteropServices.Marshal]::PtrToStringUni($info.pName) -eq $DriverName) { $driverFound = $true; break }
            }
        }
    } finally { [System.Runtime.InteropServices.Marshal]::FreeHGlobal($buf) }
}
if (-not $driverFound) {
    Log "ERRO: driver '$DriverName' nao encontrado."
    $LogWriter.Close(); exit 1
}
Trace-Step "driver encontrado"

# -- Configuracao da porta/perfil ------------------------------------------
$PortName = "Meddrive Printer PORT $ProfileName"
$PortReg  = "$MonitorReg\Ports\$PortName"

Log "Configurando perfil '$ProfileName'..."
Trace-Step "registrando porta em $PortReg"
New-Item -Path $PortReg -Force | Out-Null
Set-ItemProperty -Path $PortReg -Name "OutputPath"        -Value $OutputPath                    -Type String
Set-ItemProperty -Path $PortReg -Name "OutputBaseName"    -Value $OutputBaseName                -Type String
Set-ItemProperty -Path $PortReg -Name "GhostscriptPath"   -Value $GhostscriptPath               -Type String
Set-ItemProperty -Path $PortReg -Name "OpenAfterGenerate" -Value ([int][bool]$OpenAfterGenerate) -Type DWord
Set-ItemProperty -Path $PortReg -Name "OverwriteFile"     -Value ([int][bool]$OverwriteFile)     -Type DWord
Trace-Step "porta configurada"

if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    Trace-Step "pasta de destino criada: $OutputPath"
}

# -- Spooler ---------------------------------------------------------------
Log "Iniciando o Spooler..."
Start-Service -Name Spooler
Start-Sleep -Seconds 5

$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Log "ERRO: Spooler nao esta em execucao (status: $spoolerStatus)"
    $LogWriter.Close(); exit 1
}
if (-not (Test-Path $PortReg)) {
    Log "ERRO: porta nao encontrada no registry ($PortReg)"
    $LogWriter.Close(); exit 1
}
Log "  OK - Spooler em execucao, porta no registry"

# -- Registra a porta via AddPortExW --------------------------------------
$pi1       = New-Object Win32CreateProfile+PORT_INFO_1
$pi1.pName = $PortName
Log "Registrando porta via AddPortExW..."
$portOk = [Win32CreateProfile]::AddPortEx($null, 1, [ref]$pi1, $MonitorName)
if (-not $portOk) {
    Log "  AVISO: AddPortExW falhou (Win32 erro $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))"
} else {
    Log "  OK - porta registrada via AddPortExW"
}

Log ""
Log "Perfil criado com sucesso!"
Log "  Perfil      : $ProfileName"
Log "  Porta       : $PortName"
Log "  Template    : $OutputBaseName"
Log "  Saida       : $OutputPath\"
Log "  Ghostscript : $GhostscriptPath"
Log "  AbrirAposGerar      : $([int][bool]$OpenAfterGenerate)"
Log "  SobrescreverArquivo : $([int][bool]$OverwriteFile)"

$LogWriter.Close()
