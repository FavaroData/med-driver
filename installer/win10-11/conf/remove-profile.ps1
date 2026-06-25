#Requires -RunAsAdministrator

# Remove um perfil de porta Meddrive do registry e do spooler.

param(
    [string]$ProfileName
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
function Trace-Step($msg) { Log "CHECKPOINT: $msg" }

Log ""
Log "=== [$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] remove-profile ==="
Trace-Step "inicio do script"

if (-not $ProfileName) {
    Log "ERRO: parametro -ProfileName e obrigatorio."
    $LogWriter.Close()
    exit 1
}

$ErrorActionPreference = "Stop"

$MonitorName = "Meddrive Printer MONITOR"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$PortName    = "Meddrive Printer PORT $ProfileName"
$PortReg     = "$MonitorReg\Ports\$PortName"

if (-not (Test-Path $PortReg)) {
    Log "ERRO: perfil '$ProfileName' nao encontrado no registry ($PortReg)."
    $LogWriter.Close()
    exit 1
}
Trace-Step "perfil encontrado em $PortReg"

# ── Spooler ───────────────────────────────────────────────────────────────
Log "Iniciando o Spooler..."
Start-Service -Name Spooler
Start-Sleep -Seconds 2

$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Log "ERRO: Spooler nao esta em execucao (status: $spoolerStatus)"
    $LogWriter.Close()
    exit 1
}
Trace-Step "Spooler em execucao"

# ── DeletePortW via P/Invoke ──────────────────────────────────────────────
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class PortRemover {
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool DeletePort(string pName, IntPtr hWnd, string pPortName);
}
" -ErrorAction SilentlyContinue

Log "Removendo porta '$PortName' via DeletePortW..."
$ok = [PortRemover]::DeletePort($null, [IntPtr]::Zero, $PortName)
if (-not $ok) {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Log "  AVISO: DeletePortW falhou (Win32 erro $err) -- removendo apenas do registry"
} else {
    Trace-Step "porta removida via DeletePortW"
}

# ── Remove chave do registry ──────────────────────────────────────────────
Remove-Item -Path $PortReg -Recurse -Force
Trace-Step "chave do registry removida"

Log ""
Log "Perfil removido com sucesso!"
Log "  Perfil : $ProfileName"
Log "  Porta  : $PortName"

$LogWriter.Close()
