#Requires -RunAsAdministrator

# Cria um perfil de porta Meddrive no registry e registra a porta no spooler.
# Uma impressora pode ser vinculada ao perfil posteriormente com add-printer.ps1.

param(
    [string]$ProfileName,
    [string]$OutputPath,
    [string]$OutputBaseName,
    [switch]$OpenAfterGenerate,
    [switch]$OverwriteFile
)

$LogFile   = "C:\Windows\Temp\meddrive_ps_createprofile.log"
$LogWriter = [System.IO.StreamWriter]::new($LogFile, $false, [System.Text.Encoding]::Unicode)
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
Trace-Step "LogWriter OK"

if (-not $ProfileName) {
    Log "ERRO: parâmetro -ProfileName é obrigatório."
    $LogWriter.Close()
    exit 1
}
if (-not $OutputPath) {
    Log "ERRO: parâmetro -OutputPath é obrigatório."
    $LogWriter.Close()
    exit 1
}
if (-not $OutputBaseName) {
    Log "ERRO: parâmetro -OutputBaseName é obrigatório."
    $LogWriter.Close()
    exit 1
}

$GhostscriptPath     = "$env:ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe"
$ErrorActionPreference = "Stop"

# ── Pré-requisitos ────────────────────────────────────────────────────────
$DllPath = "$env:SystemRoot\System32\meddrivemon.dll"
if (-not (Test-Path $DllPath)) {
    Log "ERRO: meddrivemon.dll não encontrada em $DllPath. Execute o instalador principal antes de criar perfis."
    $LogWriter.Close()
    exit 1
}
Trace-Step "DLL encontrada em $DllPath"

$MonitorName = "Meddrive Printer MONITOR"
$DriverName  = "Meddrive Printer DRIVER"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"

if (-not (Test-Path $MonitorReg)) {
    Log "ERRO: monitor '$MonitorName' não encontrado no registry. Execute o instalador principal antes de criar perfis."
    $LogWriter.Close()
    exit 1
}
Trace-Step "monitor encontrado no registry"

if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Log "ERRO: driver '$DriverName' não encontrado. Execute o instalador principal antes de criar perfis."
    $LogWriter.Close()
    exit 1
}
Trace-Step "driver encontrado"

# ── Configuração da porta/perfil ──────────────────────────────────────────
$PortName = "Meddrive Printer PORT $ProfileName"
$PortReg  = "$MonitorReg\Ports\$PortName"

Log "Configurando perfil '$ProfileName'..."
Trace-Step "registrando porta em $PortReg"
New-Item -Path $PortReg -Force | Out-Null
Set-ItemProperty -Path $PortReg -Name "OutputPath"        -Value $OutputPath        -Type String
Set-ItemProperty -Path $PortReg -Name "OutputBaseName"    -Value $OutputBaseName    -Type String
Set-ItemProperty -Path $PortReg -Name "GhostscriptPath"   -Value $GhostscriptPath   -Type String
Set-ItemProperty -Path $PortReg -Name "OpenAfterGenerate" -Value ([int][bool]$OpenAfterGenerate) -Type DWord
Set-ItemProperty -Path $PortReg -Name "OverwriteFile"     -Value ([int][bool]$OverwriteFile)     -Type DWord
Trace-Step "porta configurada"

if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    Trace-Step "pasta de destino criada: $OutputPath"
}

# ── Spooler ───────────────────────────────────────────────────────────────
Log "Iniciando o Spooler..."
Start-Service -Name Spooler

Log "Aguardando o Spooler carregar o monitor..."
Start-Sleep -Seconds 5

$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Log "ERRO: Spooler não está em execução (status: $spoolerStatus)"
    $LogWriter.Close()
    exit 1
}
if (-not (Test-Path $PortReg)) {
    Log "ERRO: porta não encontrada no registry ($PortReg)"
    $LogWriter.Close()
    exit 1
}
Log "  OK - Spooler em execução, porta no registry"

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
Log "Registrando porta via AddPortExW..."
$portOk = [PortRegistrar]::AddPortEx($null, 1, [ref]$pi1, $MonitorName)
if (-not $portOk) {
    $portErr = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Log "  AVISO: AddPortExW falhou (Win32 erro $portErr)"
} else {
    Log "  OK - porta registrada via AddPortExW"
}

Log ""
Log "Perfil criado com sucesso!"
Log "  Perfil    : $ProfileName"
Log "  Porta     : $PortName"
Log "  Template  : $OutputBaseName"
Log "  Saída     : $OutputPath\"
Log "  Ghostscript: $GhostscriptPath"
Log "  AbrirApósGerar : $([int][bool]$OpenAfterGenerate)"
Log "  SobrescreverArquivo : $([int][bool]$OverwriteFile)"

$LogWriter.Close()
