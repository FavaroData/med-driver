param(
    [Parameter(Mandatory=$true)] [string]$OldPrinterName,
    [Parameter(Mandatory=$true)] [string]$NewPrinterName,
    [Parameter(Mandatory=$true)] [string]$ProfileName
)

$ErrorActionPreference = "Stop"
$PortName   = "Meddrive Printer PORT $ProfileName"
$MonitorReg = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\Meddrive Printer MONITOR"
$PortReg    = "$MonitorReg\Ports\$PortName"

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
Log "=== [$(Get-Date -Format 'HH:mm:ss')] edit-printer ==="

$printer = Get-Printer -Name $OldPrinterName -ErrorAction SilentlyContinue
if (-not $printer) {
    Log "[ERRO] Impressora '$OldPrinterName' nao encontrada."
    $LogWriter.Close()
    exit 1
}

if (-not (Test-Path $PortReg)) {
    Log "[ERRO] Porta '$PortName' nao encontrada. Verifique se o perfil existe."
    $LogWriter.Close()
    exit 1
}

if ($OldPrinterName -ne $NewPrinterName) {
    Log "[INFO] Renomeando '$OldPrinterName' para '$NewPrinterName'..."
    Rename-Printer -Name $OldPrinterName -NewName $NewPrinterName
}

if ($printer.PortName -ne $PortName) {
    Log "[INFO] Alterando perfil para '$ProfileName'..."
    Set-Printer -Name $NewPrinterName -PortName $PortName
} else {
    Log "[INFO] Perfil ja esta definido como '$ProfileName', sem alteracao."
}

Log "[OK] Impressora '$NewPrinterName' atualizada com sucesso."
$LogWriter.Close()
exit 0
