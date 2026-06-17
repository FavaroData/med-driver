#Requires -RunAsAdministrator

# Remove completamente uma impressora Meddrive do sistema:
# impressora, porta e registry da porta.

param(
    [string]$PrinterName = "Meddrive Printer"
)

$LogFile   = "C:\Windows\Temp\meddrive_ps_removeprinter.log"
$LogWriter = [System.IO.StreamWriter]::new($LogFile, $false, [System.Text.Encoding]::Unicode)

function Log($msg) {
    Write-Host $msg
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

$ErrorActionPreference = "Stop"

$MonitorName = "Meddrive Printer MONITOR"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"

$portSuffix = $PrinterName -replace 'Meddrive Printer', '' -replace '-', '' -replace '\s', ''
$PortName   = if ($portSuffix) { "Meddrive Printer PORT $portSuffix" } else { "Meddrive Printer PORT" }
$PortReg    = "$MonitorReg\Ports\$PortName"

Log "Impressora : $PrinterName"
Log "Porta      : $PortName"
Log ""

# ── Garante Spooler em execução ──────────────────────────────────────────
Log "Verificando Spooler..."
$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Start-Service -Name Spooler
    Start-Sleep -Seconds 3
}
Trace-Step "Spooler em execução"

# ── Remove a impressora ───────────────────────────────────────────────────
Log "Removendo impressora '$PrinterName'..."
if (Get-Printer -Name $PrinterName -ErrorAction SilentlyContinue) {
    Remove-Printer -Name $PrinterName
    Log "  OK - impressora removida do Windows"
} else {
    Log "  AVISO: impressora '$PrinterName' não encontrada no sistema"
}
Trace-Step "impressora removida"

# ── Remove a porta via Spooler ────────────────────────────────────────────
Log "Removendo porta '$PortName'..."
if (Get-PrinterPort -Name $PortName -ErrorAction SilentlyContinue) {
    Remove-PrinterPort -Name $PortName
    Log "  OK - porta removida"
} else {
    Log "  AVISO: porta '$PortName' não encontrada no Spooler"
}
Trace-Step "porta removida"

# ── Remove registry da porta ─────────────────────────────────────────────
Log "Limpando registry..."
Stop-Service -Name Spooler -Force
$p = Get-Process -Name spoolsv -ErrorAction SilentlyContinue
if ($p) { $p.WaitForExit() }

if (Test-Path $PortReg) {
    Remove-Item -Path $PortReg -Recurse -Force
    Log "  OK - chave de registry da porta removida"
} else {
    Log "  OK - registry da porta já inexistente"
}
Trace-Step "registry limpo"

# ── Reinicia Spooler ──────────────────────────────────────────────────────
Log "Reiniciando Spooler..."
Start-Service -Name Spooler
Trace-Step "Spooler reiniciado"

Log ""
Log "Remoção concluída!"
Log "  Impressora : $PrinterName"
Log "  Porta      : $PortName"

$LogWriter.Close()
