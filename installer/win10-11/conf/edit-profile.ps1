#Requires -RunAsAdministrator

# Atualiza as configuracoes de um perfil de porta Meddrive.
# Se -NewName for diferente de -ProfileName, renomeia a porta e migra impressoras vinculadas.

param(
    [string]$ProfileName,
    [string]$NewName,
    [string]$OutputPath,
    [string]$OutputBaseName,
    [switch]$OpenAfterGenerate,
    [switch]$OverwriteFile,
    [switch]$ChoosePath
)

$LogFile   = "C:\Windows\Temp\meddrive_printer_manager.log"
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
Log "=== [$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] edit-profile ==="

if (-not $ProfileName)    { Log "[ERRO] Parametro -ProfileName e obrigatorio.";    $LogWriter.Close(); exit 1 }
if (-not $OutputPath)     { Log "[ERRO] Parametro -OutputPath e obrigatorio.";     $LogWriter.Close(); exit 1 }
if (-not $OutputBaseName) { Log "[ERRO] Parametro -OutputBaseName e obrigatorio."; $LogWriter.Close(); exit 1 }

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
    Log "[ERRO] Perfil '$ProfileName' nao encontrado no registry ($OldPortReg)."
    $LogWriter.Close()
    exit 1
}

Log "[INFO] Iniciando Spooler..."
Start-Service -Name Spooler
Start-Sleep -Seconds 2

$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Log "[ERRO] Spooler nao esta em execucao (status: $spoolerStatus)"
    $LogWriter.Close()
    exit 1
}

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
    Log "[INFO] Renomeando '$ProfileName' -> '$NewName'..."

    New-Item -Path $NewPortReg -Force | Out-Null
    Set-ItemProperty -Path $NewPortReg -Name "OutputPath"        -Value $OutputPath                    -Type String
    Set-ItemProperty -Path $NewPortReg -Name "OutputBaseName"    -Value $OutputBaseName                -Type String
    Set-ItemProperty -Path $NewPortReg -Name "GhostscriptPath"   -Value $GhostscriptPath               -Type String
    Set-ItemProperty -Path $NewPortReg -Name "OpenAfterGenerate" -Value ([int][bool]$OpenAfterGenerate) -Type DWord
    Set-ItemProperty -Path $NewPortReg -Name "OverwriteFile"     -Value ([int][bool]$OverwriteFile)     -Type DWord
    Set-ItemProperty -Path $NewPortReg -Name "ChoosePath"        -Value ([int][bool]$ChoosePath)        -Type DWord

    $pi1 = New-Object PortManager+PORT_INFO_1
    $pi1.pName = $NewPortName
    Log "[INFO] Registrando nova porta '$NewPortName'..."
    $addOk = [PortManager]::AddPortEx($null, 1, [ref]$pi1, $MonitorName)
    if (-not $addOk) {
        Log "[AVISO] AddPortExW falhou (Win32 erro $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))"
    }

    $linked = Get-Printer | Where-Object { $_.PortName -eq $OldPortName }
    foreach ($p in $linked) {
        Log "[INFO] Migrando impressora '$($p.Name)'..."
        Set-Printer -Name $p.Name -PortName $NewPortName
    }

    Log "[INFO] Removendo porta antiga '$OldPortName'..."
    $delOk = [PortManager]::DeletePort($null, [IntPtr]::Zero, $OldPortName)
    if (-not $delOk) {
        Log "[AVISO] DeletePortW falhou (Win32 erro $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))"
    }

    Remove-Item -Path $OldPortReg -Recurse -Force

} else {
    Log "[INFO] Atualizando configuracoes do perfil '$ProfileName'..."
    Set-ItemProperty -Path $OldPortReg -Name "OutputPath"        -Value $OutputPath                    -Type String
    Set-ItemProperty -Path $OldPortReg -Name "OutputBaseName"    -Value $OutputBaseName                -Type String
    Set-ItemProperty -Path $OldPortReg -Name "GhostscriptPath"   -Value $GhostscriptPath               -Type String
    Set-ItemProperty -Path $OldPortReg -Name "OpenAfterGenerate" -Value ([int][bool]$OpenAfterGenerate) -Type DWord
    Set-ItemProperty -Path $OldPortReg -Name "OverwriteFile"     -Value ([int][bool]$OverwriteFile)     -Type DWord
    Set-ItemProperty -Path $OldPortReg -Name "ChoosePath"        -Value ([int][bool]$ChoosePath)        -Type DWord
}

if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
}

Log "[OK] Perfil editado com sucesso!"
Log "     Perfil  : $NewName"
Log "     Porta   : $NewPortName"
Log "     Padrao  : $OutputBaseName"
Log "     Saida   : $OutputPath\"
Log "     AbrirAposGerar      : $([int][bool]$OpenAfterGenerate)"
Log "     SobrescreverArquivo : $([int][bool]$OverwriteFile)"
Log "     EscolherDestino     : $([int][bool]$ChoosePath)"

$LogWriter.Close()
