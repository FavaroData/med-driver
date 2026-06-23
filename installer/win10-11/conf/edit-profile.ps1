#Requires -RunAsAdministrator

# Atualiza as configuracoes de um perfil de porta Meddrive.
# Se -NewName for diferente de -ProfileName, renomeia a porta e migra impressoras vinculadas.

param(
    [string]$ProfileName,
    [string]$NewName,
    [string]$OutputPath,
    [string]$OutputBaseName,
    [switch]$OpenAfterGenerate,
    [switch]$OverwriteFile
)

$LogFile   = "C:\Windows\Temp\meddrive_ps_editprofile.log"
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

if (-not $ProfileName) {
    Log "ERRO: parametro -ProfileName e obrigatorio."
    $LogWriter.Close()
    exit 1
}
if (-not $OutputPath) {
    Log "ERRO: parametro -OutputPath e obrigatorio."
    $LogWriter.Close()
    exit 1
}
if (-not $OutputBaseName) {
    Log "ERRO: parametro -OutputBaseName e obrigatorio."
    $LogWriter.Close()
    exit 1
}

if (-not $NewName) { $NewName = $ProfileName }

$GhostscriptPath       = "$env:ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe"
$ErrorActionPreference = "Stop"

$MonitorName = "Meddrive Printer MONITOR"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$OldPortName = "Meddrive Printer PORT $ProfileName"
$NewPortName = "Meddrive Printer PORT $NewName"
$OldPortReg  = "$MonitorReg\Ports\$OldPortName"
$NewPortReg  = "$MonitorReg\Ports\$NewPortName"

if (-not (Test-Path $OldPortReg)) {
    Log "ERRO: perfil '$ProfileName' nao encontrado no registry ($OldPortReg)."
    $LogWriter.Close()
    exit 1
}
Trace-Step "perfil original encontrado"

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

# ── P/Invoke para AddPortExW e DeletePortW ────────────────────────────────
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class PortManager {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct PORT_INFO_1 {
        public string pName;
    }
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool AddPortEx(string pName, uint Level, ref PORT_INFO_1 lpBuffer, string lpMonitorName);
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool DeletePort(string pName, IntPtr hWnd, string pPortName);
}
" -ErrorAction SilentlyContinue

$Renaming = ($NewName -ne $ProfileName)

if ($Renaming) {
    Log "Renomeando perfil '$ProfileName' -> '$NewName'..."

    # 1. Cria nova chave no registry com os novos valores
    Trace-Step "criando nova chave do registry: $NewPortReg"
    New-Item -Path $NewPortReg -Force | Out-Null
    Set-ItemProperty -Path $NewPortReg -Name "OutputPath"        -Value $OutputPath        -Type String
    Set-ItemProperty -Path $NewPortReg -Name "OutputBaseName"    -Value $OutputBaseName    -Type String
    Set-ItemProperty -Path $NewPortReg -Name "GhostscriptPath"   -Value $GhostscriptPath   -Type String
    Set-ItemProperty -Path $NewPortReg -Name "OpenAfterGenerate" -Value ([int][bool]$OpenAfterGenerate) -Type DWord
    Set-ItemProperty -Path $NewPortReg -Name "OverwriteFile"     -Value ([int][bool]$OverwriteFile)     -Type DWord
    Trace-Step "nova chave do registry criada"

    # 2. Registra a nova porta no spooler via AddPortExW
    $pi1 = New-Object PortManager+PORT_INFO_1
    $pi1.pName = $NewPortName
    Log "Registrando nova porta '$NewPortName' via AddPortExW..."
    $addOk = [PortManager]::AddPortEx($null, 1, [ref]$pi1, $MonitorName)
    if (-not $addOk) {
        $addErr = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
        Log "  AVISO: AddPortExW falhou (Win32 erro $addErr)"
    } else {
        Trace-Step "nova porta registrada no spooler"
    }

    # 3. Migra impressoras vinculadas para a nova porta
    $linked = Get-Printer | Where-Object { $_.PortName -eq $OldPortName }
    foreach ($p in $linked) {
        Log "  Migrando impressora '$($p.Name)' para porta '$NewPortName'..."
        Set-Printer -Name $p.Name -PortName $NewPortName
        Trace-Step "impressora '$($p.Name)' migrada"
    }

    # 4. Remove a porta antiga do spooler via DeletePortW
    Log "Removendo porta antiga '$OldPortName' via DeletePortW..."
    $delOk = [PortManager]::DeletePort($null, [IntPtr]::Zero, $OldPortName)
    if (-not $delOk) {
        $delErr = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
        Log "  AVISO: DeletePortW falhou (Win32 erro $delErr)"
    } else {
        Trace-Step "porta antiga removida do spooler"
    }

    # 5. Remove chave antiga do registry
    Remove-Item -Path $OldPortReg -Recurse -Force
    Trace-Step "chave antiga do registry removida"

} else {
    Log "Atualizando configuracoes do perfil '$ProfileName'..."

    Set-ItemProperty -Path $OldPortReg -Name "OutputPath"        -Value $OutputPath        -Type String
    Set-ItemProperty -Path $OldPortReg -Name "OutputBaseName"    -Value $OutputBaseName    -Type String
    Set-ItemProperty -Path $OldPortReg -Name "GhostscriptPath"   -Value $GhostscriptPath   -Type String
    Set-ItemProperty -Path $OldPortReg -Name "OpenAfterGenerate" -Value ([int][bool]$OpenAfterGenerate) -Type DWord
    Set-ItemProperty -Path $OldPortReg -Name "OverwriteFile"     -Value ([int][bool]$OverwriteFile)     -Type DWord
    Trace-Step "valores do registry atualizados"
}

if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    Trace-Step "pasta de destino criada: $OutputPath"
}

Log ""
Log "Perfil editado com sucesso!"
Log "  Perfil  : $NewName"
Log "  Porta   : $NewPortName"
Log "  Padrao  : $OutputBaseName"
Log "  Saida   : $OutputPath\"
Log "  AbrirAposGerar      : $([int][bool]$OpenAfterGenerate)"
Log "  SobrescreverArquivo : $([int][bool]$OverwriteFile)"

$LogWriter.Close()
