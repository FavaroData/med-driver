#Requires -RunAsAdministrator

# Remove a impressora Meddrive do sistema.
# O perfil (porta no registry) e preservado e pode ser reutilizado.

param(
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
Log "=== [$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] remove-printer ==="

$ErrorActionPreference = "Stop"

$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Start-Service -Name Spooler
    Start-Sleep -Seconds 3
}

Log "[INFO] Removendo impressora '$PrinterName'..."
if (Get-Printer -Name $PrinterName -ErrorAction SilentlyContinue) {
    Remove-Printer -Name $PrinterName
    Log "[OK] Impressora '$PrinterName' removida"
} else {
    Log "[AVISO] Impressora '$PrinterName' nao encontrada no sistema"
}

$LogWriter.Close()
