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
$MonitorName = "MedMonitor"
$PortName    = "MedPort"
$PrinterName = "MedPrinter"
$DriverName  = "Microsoft PS Class Driver"

# Regs 
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$PortReg     = "$MonitorReg\Ports\$PortName"

# Verificações
Write-Host "Parando o Spooler..."
Stop-Service -Name Spooler -Force
$p = Get-Process -Name spoolsv -ErrorAction SilentlyContinue
if ($p) { $p.WaitForExit() }

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

# Win32 EnumPorts via winspool.drv retorna ERROR_INVALID_DATA (13) para monitores customizados
# no Windows 10/11 — falha de validacao de ponteiro no merge RPC do spooler.
# O Add-Printer usa o caminho interno do spooler que funciona corretamente.
# Verificamos apenas registry + status do servico como pre-condicao minima.
$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Write-Host "ERRO: Spooler nao esta em execucao (status: $spoolerStatus)"
    exit 1
}
if (-not (Test-Path $MonitorReg)) {
    Write-Host "ERRO: monitor nao encontrado no registry ($MonitorReg)"
    exit 1
}
if (-not (Test-Path $PortReg)) {
    Write-Host "ERRO: porta nao encontrada no registry ($PortReg)"
    exit 1
}
Write-Host "  OK - Spooler em execucao, monitor e porta no registry"

# Instalação do driver
Write-Host "Instalando driver PostScript..."
if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Add-PrinterDriver -Name $DriverName -ErrorAction Stop
}

# P/Invoke direto ao AddPrinterW — bypassa CIM/WMI e expoe o codigo Win32 exato.
# Add-Printer usava o caminho CIM que retornava so a string localizada do erro,
# impedindo diagnostico. Aqui controlamos todos os campos de PRINTER_INFO_2W.
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class PrinterWin32 {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct PRINTER_INFO_2 {
        public string pServerName;
        public string pPrinterName;
        public string pShareName;
        public string pPortName;
        public string pDriverName;
        public string pComment;
        public string pLocation;
        public IntPtr pDevMode;
        public string pSepFile;
        public string pPrintProcessor;
        public string pDatatype;
        public string pParameters;
        public IntPtr pSecurityDescriptor;
        public uint   Attributes;
        public uint   Priority;
        public uint   DefaultPriority;
        public uint   StartTime;
        public uint   UntilTime;
        public uint   Status;
        public uint   cJobs;
        public uint   AveragePPM;
    }
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern IntPtr AddPrinter(string pName, uint Level, ref PRINTER_INFO_2 pPrinter);
    [DllImport(""winspool.drv"", SetLastError=true)]
    public static extern bool ClosePrinter(IntPtr hPrinter);
}
" -ErrorAction SilentlyContinue

$pi2                 = New-Object PrinterWin32+PRINTER_INFO_2
$pi2.pPrinterName    = $PrinterName
$pi2.pPortName       = $PortName
$pi2.pDriverName     = $DriverName
$pi2.pPrintProcessor = "winprint"
$pi2.pDatatype       = "RAW"
$pi2.Attributes      = 64   # PRINTER_ATTRIBUTE_LOCAL

Write-Host "Registrando impressora via AddPrinterW..."
Write-Host "  pPrinterName   : $($pi2.pPrinterName)"
Write-Host "  pPortName      : $($pi2.pPortName)"
Write-Host "  pDriverName    : $($pi2.pDriverName)"
Write-Host "  pPrintProcessor: $($pi2.pPrintProcessor)"
Write-Host "  pDatatype      : $($pi2.pDatatype)"

if (Get-Printer -Name $PrinterName -ErrorAction SilentlyContinue) {
    Remove-Printer -Name $PrinterName
}

$maxAttempts = 5
$attempt     = 0
$success     = $false
$erroMsg     = ""
while ($attempt -lt $maxAttempts -and -not $success) {
    $hPrinter = [PrinterWin32]::AddPrinter($null, 2, [ref]$pi2)
    if ($hPrinter -ne [IntPtr]::Zero) {
        [PrinterWin32]::ClosePrinter($hPrinter) | Out-Null
        $success = $true
    } else {
        $attempt++
        $win32Err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
        $erroMsg  = "Win32 erro $win32Err"
        Write-Host "  Tentativa $attempt/$maxAttempts falhou: $erroMsg"
        if ($attempt -lt $maxAttempts) { Start-Sleep -Seconds 3 }
    }
}

# Saída final: só exibe sucesso se todos os passos anteriores deram certo
Write-Host ""
if ($success) {
    Write-Host "Instalacao concluida!"
    Write-Host "  Impressora : $PrinterName"
    Write-Host "  Porta      : $PortName"
    Write-Host "  Saida      : $OutputPath"
    Write-Host "  Ghostscript: $GhostscriptPath"
} else {
    Write-Host "ERRO: instalacao falhou ao registrar a impressora '$PrinterName'."
    Write-Host "  Motivo     : $erroMsg"
    Write-Host "  Log da DLL : C:\Windows\Temp\pdfmonitor_init.log"
    exit 1
}
