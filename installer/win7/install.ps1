# Compativel com Windows 7 x64 e PowerShell 2.0+
# Instala monitor, driver PSCRIPT5 e copia o aplicativo de gerenciamento.
# NAO cria impressoras -- use MedDriveManager.exe apos a instalacao.

trap {
    Write-Output "EXCEPTION TYPE: $($_.Exception.GetType().FullName)"
    Write-Output "EXCEPTION MSG : $($_.Exception.Message)"
    Write-Output "LINE          : $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
    Write-Output "STACK         : $($_.ScriptStackTrace)"
    exit 1
}
function Trace-Step($msg) { Write-Output "CHECKPOINT: $msg" }

Trace-Step "inicio do script"

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Trace-Step "processo nao elevado, relancando via RunAs"
    $scriptPath = $MyInvocation.MyCommand.Path
    $arguments  = "-ExecutionPolicy Bypass -File `"$scriptPath`""
    Start-Process powershell -Verb RunAs -ArgumentList $arguments -Wait
    exit $LASTEXITCODE
}
Trace-Step "processo elevado"

Start-Transcript -Path "C:\Windows\Temp\meddrive_ps_install.log" -Force
Trace-Step "Start-Transcript OK"

$GhostscriptPath = "$env:ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe"
$AppDir          = "$env:ProgramData\Meddrive Printer"

$ErrorActionPreference = "Stop"
Trace-Step "resolvendo ScriptDir a partir de $($MyInvocation.MyCommand.Path)"
$ScriptDir = Split-Path -Parent (Resolve-Path $MyInvocation.MyCommand.Path)
Trace-Step "ScriptDir resolvido: $ScriptDir"

$DllSource = Join-Path $ScriptDir "..\meddrivemon.dll"
if (-not (Test-Path $DllSource)) {
    $DllSource = Join-Path $ScriptDir "..\..\meddrivemon.dll"
}
Trace-Step "DllSource: $DllSource"
$DllDest = "$env:SystemRoot\System32\meddrivemon.dll"

$MonitorName = "Meddrive Printer MONITOR"
$DriverName  = "Meddrive Printer DRIVER"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$DriverKey   = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Environments\Windows x64\Drivers\Version-3\$DriverName"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win32PrintInstall {
    [StructLayout(LayoutKind.Sequential)]
    public struct DRIVER_INFO_1 { public IntPtr pName; }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct DRIVER_INFO_2 {
        public uint   cVersion;
        public string pName;
        public string pEnvironment;
        public string pDriverPath;
        public string pDataFile;
        public string pConfigFile;
    }
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool EnumPrinterDrivers(string pName, string pEnvironment, uint Level, IntPtr pDriverInfo, uint cbBuf, ref uint pcbNeeded, ref uint pcReturned);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool AddPrinterDriverEx(string pName, uint Level, ref DRIVER_INFO_2 pDriverInfo, uint dwFileCopyFlags);
}
"@ -ErrorAction SilentlyContinue

# ── 1. Para o Spooler ─────────────────────────────────────────────────────
Write-Host "Parando o Spooler..."
Stop-Service -Name Spooler -Force
$p = Get-Process -Name spoolsv -ErrorAction SilentlyContinue
if ($p) { $p.WaitForExit() }
Trace-Step "Spooler parado"

# ── 2. Copia DLL para System32 ────────────────────────────────────────────
Write-Host "Copiando DLL para System32..."
if (-not (Test-Path $DllSource)) {
    Write-Host "ERRO: DLL nao encontrada em $DllSource"
    exit 1
}
Trace-Step "copiando $DllSource para $DllDest"
Copy-Item $DllSource $DllDest -Force
Trace-Step "DLL copiada"

# ── 3. Registra monitor no registry ──────────────────────────────────────
Write-Host "Registrando monitor no registry..."
Trace-Step "registrando monitor em $MonitorReg"
if (-not (Test-Path $MonitorReg)) { New-Item -Path $MonitorReg | Out-Null }
Set-ItemProperty -Path $MonitorReg -Name "Driver" -Value "meddrivemon.dll" -Type String
Trace-Step "monitor registrado"

# ── 4. Inicia Spooler ─────────────────────────────────────────────────────
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
Write-Host "  OK - Spooler em execucao, monitor no registry"

# ── 5. Instala driver PSCRIPT5 via AddPrinterDriverEx ────────────────────
Write-Host "Instalando driver PSCRIPT5 customizado via AddPrinterDriverEx..."
$ps5 = Get-ChildItem "$env:SystemRoot\System32\DriverStore\FileRepository" `
    -Recurse -Filter "PSCRIPT5.DLL" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $ps5) {
    Write-Host "ERRO: PSCRIPT5.DLL nao encontrado no DriverStore"
    exit 1
}
$driverDir = $ps5.DirectoryName
Write-Host "  DriverStore: $driverDir"

$di2              = New-Object Win32PrintInstall+DRIVER_INFO_2
$di2.cVersion     = 3
$di2.pName        = $DriverName
$di2.pEnvironment = "Windows x64"
$di2.pDriverPath  = "$driverDir\PSCRIPT5.DLL"
$di2.pDataFile    = "$driverDir\PSCRIPT.NTF"
$di2.pConfigFile  = "$driverDir\PS5UI.DLL"
$drvOk = [Win32PrintInstall]::AddPrinterDriverEx($null, 2, [ref]$di2, 20)
if (-not $drvOk) {
    $drvErr = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Write-Host "ERRO: AddPrinterDriverEx falhou (Win32 erro $drvErr)"
    exit 1
}
Set-ItemProperty $DriverKey -Name "PrinterDriverAttributes" -Value 2 -Type DWord
Write-Host "  OK - driver '$DriverName' registrado via AddPrinterDriverEx"

# ── 6. Instala PPD ────────────────────────────────────────────────────────
Write-Host "Instalando PPD do driver..."
$PpdSource = Join-Path $ScriptDir "MEDDRIVE.PPD"
$PpdDest   = "$env:SystemRoot\System32\spool\drivers\x64\3\MEDDRIVE.PPD"
if (-not (Test-Path $PpdSource)) {
    Write-Host "ERRO: MEDDRIVE.PPD nao encontrado em $PpdSource"
    exit 1
}
Copy-Item $PpdSource $PpdDest -Force
Set-ItemProperty $DriverKey -Name "Dependent Files" -Value @("MEDDRIVE.PPD", "") -Type MultiString
Write-Host "  OK - PPD copiado e registrado em Dependent Files"

# ── 7. Reinicia Spooler para enumerar o driver ────────────────────────────
Write-Host "Reiniciando o Spooler para enumerar o driver..."
Restart-Service -Name Spooler -Force
Start-Sleep -Seconds 3

$needed   = [uint32]0
$returned = [uint32]0
[Win32PrintInstall]::EnumPrinterDrivers($null, "Windows x64", 1, [IntPtr]::Zero, 0, [ref]$needed, [ref]$returned) | Out-Null
$buf = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([int]$needed)
$driverFound = $false
try {
    if ([Win32PrintInstall]::EnumPrinterDrivers($null, "Windows x64", 1, $buf, $needed, [ref]$needed, [ref]$returned)) {
        $sz = [System.Runtime.InteropServices.Marshal]::SizeOf([type][Win32PrintInstall+DRIVER_INFO_1])
        for ($i = 0; $i -lt [int]$returned; $i++) {
            $ptr  = [IntPtr]($buf.ToInt64() + $i * $sz)
            $info = [System.Runtime.InteropServices.Marshal]::PtrToStructure($ptr, [type][Win32PrintInstall+DRIVER_INFO_1])
            $name = [System.Runtime.InteropServices.Marshal]::PtrToStringUni($info.pName)
            if ($name -eq $DriverName) { $driverFound = $true; break }
        }
    }
} finally {
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($buf)
}
if (-not $driverFound) {
    Write-Host "ERRO: driver '$DriverName' nao reconhecido pelo spooler apos reinicio"
    exit 1
}
Write-Host "  OK - driver '$DriverName' reconhecido pelo spooler"

# ── 8. Distribui aplicativo e scripts para ProgramData ───────────────────
Write-Host "Copiando aplicativo para $AppDir..."
New-Item -ItemType Directory -Path $AppDir -Force | Out-Null
Copy-Item (Join-Path $ScriptDir "add-printer.ps1")     "$AppDir\add-printer.ps1"     -Force
Copy-Item (Join-Path $ScriptDir "create-profile.ps1")  "$AppDir\create-profile.ps1"  -Force
Copy-Item (Join-Path $ScriptDir "edit-profile.ps1")    "$AppDir\edit-profile.ps1"    -Force
Copy-Item (Join-Path $ScriptDir "edit-printer.ps1")    "$AppDir\edit-printer.ps1"    -Force
Copy-Item (Join-Path $ScriptDir "remove-printer.ps1")  "$AppDir\remove-printer.ps1"  -Force
Copy-Item (Join-Path $ScriptDir "remove-profile.ps1")  "$AppDir\remove-profile.ps1"  -Force
Copy-Item (Join-Path $ScriptDir "MEDDRIVE.PPD")        "$AppDir\MEDDRIVE.PPD"        -Force
Copy-Item (Join-Path $ScriptDir "MedDriveManager.exe") "$AppDir\MedDriveManager.exe" -Force
Trace-Step "aplicativo e scripts distribuidos"

Write-Host ""
Write-Host "Instalacao concluida!"
Write-Host "  Monitor    : $MonitorName"
Write-Host "  Driver     : $DriverName"
Write-Host "  Aplicativo : $AppDir\MedDriveManager.exe"
Write-Host "  Ghostscript: $GhostscriptPath"
