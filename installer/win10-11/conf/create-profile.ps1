#Requires -RunAsAdministrator

# Cria um perfil de porta Meddrive no registry e registra a porta no spooler.
# Uma impressora pode ser vinculada ao perfil posteriormente com add-printer.ps1.

param(
    [string]$ProfileName,
    [string]$OutputPath,
    [string]$OutputBaseName,
    [switch]$OpenAfterGenerate,
    [switch]$OverwriteFile,
    [switch]$ChoosePath
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
Log "=== [$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] create-profile ==="

if (-not $ProfileName)    { Log "[ERRO] Parametro -ProfileName e obrigatorio.";    $LogWriter.Close(); exit 1 }
if (-not $OutputPath)     { Log "[ERRO] Parametro -OutputPath e obrigatorio.";     $LogWriter.Close(); exit 1 }
if (-not $OutputBaseName) { Log "[ERRO] Parametro -OutputBaseName e obrigatorio."; $LogWriter.Close(); exit 1 }

$GhostscriptPath       = "$env:ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe"
$ErrorActionPreference = "Stop"

$DllPath = "$env:SystemRoot\System32\meddrivemon.dll"
if (-not (Test-Path $DllPath)) {
    Log "[ERRO] meddrivemon.dll nao encontrada em $DllPath. Execute o instalador principal antes de criar perfis."
    $LogWriter.Close()
    exit 1
}

$MonitorName = "Meddrive Printer MONITOR"
$DriverName  = "Meddrive Printer DRIVER"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"

if (-not (Test-Path $MonitorReg)) {
    Log "[ERRO] Monitor '$MonitorName' nao encontrado no registry. Execute o instalador principal antes de criar perfis."
    $LogWriter.Close()
    exit 1
}

if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Log "[ERRO] Driver '$DriverName' nao encontrado. Execute o instalador principal antes de criar perfis."
    $LogWriter.Close()
    exit 1
}

$PortName = "Meddrive Printer PORT $ProfileName"
$PortReg  = "$MonitorReg\Ports\$PortName"

Log "[INFO] Configurando perfil '$ProfileName'..."
New-Item -Path $PortReg -Force | Out-Null
Set-ItemProperty -Path $PortReg -Name "OutputPath"        -Value $OutputPath                    -Type String
Set-ItemProperty -Path $PortReg -Name "OutputBaseName"    -Value $OutputBaseName                -Type String
Set-ItemProperty -Path $PortReg -Name "GhostscriptPath"   -Value $GhostscriptPath               -Type String
Set-ItemProperty -Path $PortReg -Name "OpenAfterGenerate" -Value ([int][bool]$OpenAfterGenerate) -Type DWord
Set-ItemProperty -Path $PortReg -Name "OverwriteFile"     -Value ([int][bool]$OverwriteFile)     -Type DWord
Set-ItemProperty -Path $PortReg -Name "ChoosePath"        -Value ([int][bool]$ChoosePath)        -Type DWord

if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
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
if (-not (Test-Path $PortReg)) {
    Log "[ERRO] Porta nao encontrada no registry ($PortReg)"
    $LogWriter.Close()
    exit 1
}

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
Log "[INFO] Registrando porta via AddPortExW..."
$portOk = [PortRegistrar]::AddPortEx($null, 1, [ref]$pi1, $MonitorName)
if (-not $portOk) {
    $portErr = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Log "[AVISO] AddPortExW falhou (Win32 erro $portErr)"
}

Log "[OK] Perfil criado com sucesso!"
Log "     Perfil      : $ProfileName"
Log "     Porta       : $PortName"
Log "     Template    : $OutputBaseName"
Log "     Saida       : $OutputPath\"
Log "     AbrirAposGerar      : $([int][bool]$OpenAfterGenerate)"
Log "     SobrescreverArquivo : $([int][bool]$OverwriteFile)"
Log "     EscolherDestino     : $([int][bool]$ChoosePath)"

$LogWriter.Close()
