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

# Verifica se o Spooler registrou a porta via Win32 EnumPorts Level=1.
# Get-PrinterPort nao e usado pois nao enxerga portas de monitores customizados.
# Um sleep curto e necessario porque o Spooler carrega monitores de forma
# assincrona apos subir — o Start-Service retorna antes dessa carga terminar.
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class SpoolerPorts {
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool EnumPorts(string pName, uint Level, IntPtr pPort,
        uint cbBuf, ref uint pcbNeeded, ref uint pcReturned);
}" -ErrorAction SilentlyContinue

Write-Host "Verificando porta '$PortName' no Spooler..."
Start-Sleep -Seconds 5   # tempo para o Spooler terminar de carregar os monitores do registry

$portFound = $false

# Funcao auxiliar que chama EnumPorts com retry automatico:
# entre o probe (buffer=0) e a chamada real, outros monitores podem terminar
# de carregar e aumentar o tamanho necessario, fazendo a chamada real retornar
# ERROR_INSUFFICIENT_BUFFER (false). Nesse caso refaz o probe com o novo tamanho.
function Get-SpoolerPortNames {
    for ($r = 0; $r -lt 3; $r++) {
        $needed   = [uint32]0
        $returned = [uint32]0

        # probe: descobre quantos bytes sao necessarios para todas as portas
        [SpoolerPorts]::EnumPorts($null, 1, [IntPtr]::Zero, 0, [ref]$needed, [ref]$returned) | Out-Null
        if ($needed -eq 0) { return @() }

        $buf   = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([int]$needed)
        $names = $null
        try {
            $ok = [SpoolerPorts]::EnumPorts($null, 1, $buf, $needed, [ref]$needed, [ref]$returned)
            if ($ok) {
                # percorre cada PORT_INFO_1W: estrutura com 1 ponteiro (pName) de 8 bytes no x64
                $names = @()
                for ($i = 0; $i -lt [int]$returned; $i++) {
                    $ptr = [System.Runtime.InteropServices.Marshal]::ReadIntPtr(
                               [IntPtr]::Add($buf, $i * [IntPtr]::Size))
                    if ($ptr -ne [IntPtr]::Zero) {
                        $names += [System.Runtime.InteropServices.Marshal]::PtrToStringUni($ptr)
                    }
                }
            } else {
                # buffer insuficiente entre probe e chamada real (race condition de carga de monitores)
                $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
                Write-Host "  EnumPorts tentativa $($r+1)/3 falhou: erro Win32 $err (buffer=$needed bytes)"
            }
        } finally {
            [System.Runtime.InteropServices.Marshal]::FreeHGlobal($buf)
        }

        if ($null -ne $names) { return $names }
        if ($r -lt 2) { Start-Sleep -Milliseconds 300 }
    }
    return @()
}

$portas = Get-SpoolerPortNames
Write-Host "  Portas encontradas no Spooler: $($portas.Count)"
foreach ($p in $portas) { Write-Host "    '$p'" }

$portFound = $portas -contains $PortName

if (-not $portFound) {
    # diagnostica o motivo para ajudar na depuracao
    $spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
    if ($spoolerStatus -ne 'Running') {
        Write-Host "ERRO: Spooler nao esta em execucao (status: $spoolerStatus)"
    } elseif (-not (Test-Path $DllDest)) {
        Write-Host "ERRO: DLL nao encontrada em $DllDest"
    } elseif (-not (Test-Path $MonitorReg)) {
        Write-Host "ERRO: monitor nao esta no registry ($MonitorReg)"
    } elseif (-not (Test-Path $PortReg)) {
        Write-Host "ERRO: porta nao esta no registry ($PortReg)"
    } else {
        Write-Host "ERRO: monitor registrado mas porta nao apareceu no Spooler"
        Write-Host "      Verifique o log da DLL: C:\Windows\Temp\pdfmonitor_init.log"
    }
    exit 1
}
Write-Host "  OK - porta disponivel"

# Instalação do driver e impressora virtual
Write-Host "Instalando driver PostScript..."
if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Add-PrinterDriver -Name $DriverName
}

Write-Host "Registrando impressora..."
if (Get-Printer -Name $PrinterName -ErrorAction SilentlyContinue) {
    Remove-Printer -Name $PrinterName
}
$maxAttempts  = 5
$attempt      = 0
$success      = $false
$erroMsg      = ""
while ($attempt -lt $maxAttempts -and -not $success) {
    try {
        # -ErrorAction Stop garante que erros de CIM viram excecoes capturadas pelo catch
        Add-Printer -Name $PrinterName -DriverName $DriverName -PortName $PortName -ErrorAction Stop
        $success = $true
    } catch {
        $attempt++
        $erroMsg = $_.Exception.Message
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
