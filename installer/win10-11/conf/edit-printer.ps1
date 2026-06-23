param(
    [Parameter(Mandatory=$true)] [string]$OldPrinterName,
    [Parameter(Mandatory=$true)] [string]$NewPrinterName,
    [Parameter(Mandatory=$true)] [string]$ProfileName
)

$ErrorActionPreference = "Stop"
$PortName = "Meddrive Printer PORT $ProfileName"

$printer = Get-Printer -Name $OldPrinterName -ErrorAction SilentlyContinue
if (-not $printer) {
    Write-Output "[ERRO] Impressora '$OldPrinterName' nao encontrada."
    exit 1
}

$port = Get-PrinterPort -Name $PortName -ErrorAction SilentlyContinue
if (-not $port) {
    Write-Output "[ERRO] Porta '$PortName' nao encontrada. Verifique se o perfil existe."
    exit 1
}

if ($OldPrinterName -ne $NewPrinterName) {
    Write-Output "[INFO] Renomeando '$OldPrinterName' para '$NewPrinterName'..."
    Rename-Printer -Name $OldPrinterName -NewName $NewPrinterName
}

if ($printer.PortName -ne $PortName) {
    Write-Output "[INFO] Alterando perfil para '$ProfileName'..."
    Set-Printer -Name $NewPrinterName -PortName $PortName
} else {
    Write-Output "[INFO] Perfil ja esta definido como '$ProfileName', sem alteracao."
}

Write-Output "[OK] Impressora '$NewPrinterName' atualizada com sucesso."
exit 0
