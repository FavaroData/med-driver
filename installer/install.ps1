#Requires -RunAsAdministrator

# Parâmetros de saída
param(
    [string]$OutputPath      = "C:\Users\favaro\Desktop\PDF\saida.pdf",
    [string]$GhostscriptPath = "C:\Program Files\gs\gs10.02.1\bin\gswin64c.exe"
)

# Configurações para o script de instalação do monitor 
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DllSource = Join-Path $ScriptDir "..\pdfmonitor.dll"
$DllDest   = "$env:SystemRoot\System32\pdfmonitor.dll"

# Configurações do driver, monitor e porta
$MonitorName = "Med-driver Monitor"
$PortName    = "Med-driver Port"
$PrinterName = "Med-driver Printer"
$DriverName  = "Microsoft PS Class Driver"

# Regs 
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$PortReg     = "$MonitorReg\Ports\$PortName"

# Verificações
Write-Host "Parando o Spooler..."
Stop-Service -Name Spooler -Force

Write-Host "Copiando DLL para System32..."
if (-not (Test-Path $DllSource)) {
    Write-Host "ERRO: DLL não encontrada em $DllSource"
    exit 1
}
Copy-Item $DllSource $DllDest -Force

# Registrando o monitor e a porta no registry
Write-Host "Registrando monitor no registry..."
New-Item -Path $MonitorReg -Force | Out-Null
Set-ItemProperty -Path $MonitorReg -Name "Driver" -Value "pdfmonitor.dll" -Type String

Write-Host "Configurando porta..."
New-Item -Path $PortReg -Force | Out-Null
Set-ItemProperty -Path $PortReg -Name "OutputPath"      -Value $OutputPath      -Type String
Set-ItemProperty -Path $PortReg -Name "GhostscriptPath" -Value $GhostscriptPath -Type String

# Garante que a pasta de destino existe, se não, cria a pasta
$outputDir = Split-Path -Parent $OutputPath
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

# Instalação do driver e impressora virtual
Write-Host "Iniciando o Spooler..."
Start-Service -Name Spooler

Write-Host "Aguardando o Spooler carregar o monitor..."
Start-Sleep -Seconds 5

# Instalação do driver e impressora virtual
Write-Host "Instalando driver PostScript..."
if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Add-PrinterDriver -Name $DriverName
}

Write-Host "Registrando impressora..."
if (Get-Printer -Name $PrinterName -ErrorAction SilentlyContinue) {
    Remove-Printer -Name $PrinterName
}
$maxAttempts = 5
$attempt = 0
$success = $false
while ($attempt -lt $maxAttempts -and -not $success) {
    try {
        Add-Printer -Name $PrinterName -DriverName $DriverName -PortName $PortName
        $success = $true
    } catch {
        $attempt++
        Write-Host "Tentativa $attempt/$maxAttempts falhou, aguardando..."
        Start-Sleep -Seconds 3
    }
}
if (-not $success) {
    Write-Host "ERRO: Não foi possível registrar a impressora. Verifique se a DLL carregou corretamente."
    exit 1
}

# Saída console
Write-Host ""
Write-Host "Instalacao concluida!"
Write-Host "  Impressora : $PrinterName"
Write-Host "  Saída : $OutputPath"
Write-Host "  Pós: Ghostscript: $GhostscriptPath"
