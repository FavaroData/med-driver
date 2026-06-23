#Requires -RunAsAdministrator

# Remove a impressora Meddrive do sistema.
# O perfil (porta no registry) é preservado e pode ser reutilizado.

param(
    [string]$PrinterName = "Meddrive Printer"
)

$LogFile   = "C:\Windows\Temp\meddrive_ps_removeprinter.log"
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

$ErrorActionPreference = "Stop"

Log "Impressora : $PrinterName"
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

Log ""
Log "Remoção concluída!"
Log "  Impressora : $PrinterName"

$LogWriter.Close()
