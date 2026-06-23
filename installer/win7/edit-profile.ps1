# Compativel com Windows 7 x64 e PowerShell 2.0+
# Nao usa modulo PrintManagement (ausente no Windows 7 sem RSAT)

param(
    [string]$ProfileName,
    [string]$NewName,
    [string]$OutputPath,
    [string]$OutputBaseName,
    [switch]$OpenAfterGenerate,
    [switch]$OverwriteFile
)

$LogFile   = "C:\Windows\Temp\meddrive_ps_editprofile.log"
$LogWriter = New-Object System.IO.StreamWriter($LogFile, $false, [System.Text.Encoding]::Unicode)
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

if (-not $ProfileName)    { Log "ERRO: parametro -ProfileName e obrigatorio.";    $LogWriter.Close(); exit 1 }
if (-not $OutputPath)     { Log "ERRO: parametro -OutputPath e obrigatorio.";     $LogWriter.Close(); exit 1 }
if (-not $OutputBaseName) { Log "ERRO: parametro -OutputBaseName e obrigatorio."; $LogWriter.Close(); exit 1 }

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
    $LogWriter.Close(); exit 1
}
Trace-Step "perfil original encontrado"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PortMgrWin7 {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct PORT_INFO_1 { public string pName; }
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
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool AddPortEx(string pName, uint Level, ref PORT_INFO_1 lpBuffer, string lpMonitorName);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool DeletePort(string pName, IntPtr hWnd, string pPortName);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool OpenPrinter(string pPrinterName, out IntPtr phPrinter, IntPtr pDefault);
    [DllImport("winspool.drv", SetLastError=true)]
    public static extern bool ClosePrinter(IntPtr hPrinter);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool SetPrinter(IntPtr hPrinter, uint Level, ref PRINTER_INFO_2 pPrinter, uint Command);
}
"@ -ErrorAction SilentlyContinue

# ── Spooler ───────────────────────────────────────────────────────────────
Log "Iniciando o Spooler..."
Start-Service -Name Spooler
Start-Sleep -Seconds 2

$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Log "ERRO: Spooler nao esta em execucao (status: $spoolerStatus)"
    $LogWriter.Close(); exit 1
}
Trace-Step "Spooler em execucao"

$Renaming = ($NewName -ne $ProfileName)

if ($Renaming) {
    Log "Renomeando perfil '$ProfileName' -> '$NewName'..."

    # 1. Cria nova chave no registry com os novos valores
    Trace-Step "criando nova chave do registry: $NewPortReg"
    New-Item -Path $NewPortReg -Force | Out-Null
    Set-ItemProperty -Path $NewPortReg -Name "OutputPath"        -Value $OutputPath                    -Type String
    Set-ItemProperty -Path $NewPortReg -Name "OutputBaseName"    -Value $OutputBaseName                -Type String
    Set-ItemProperty -Path $NewPortReg -Name "GhostscriptPath"   -Value $GhostscriptPath               -Type String
    Set-ItemProperty -Path $NewPortReg -Name "OpenAfterGenerate" -Value ([int][bool]$OpenAfterGenerate) -Type DWord
    Set-ItemProperty -Path $NewPortReg -Name "OverwriteFile"     -Value ([int][bool]$OverwriteFile)     -Type DWord
    Trace-Step "nova chave do registry criada"

    # 2. Registra nova porta no spooler via AddPortExW
    $pi1 = New-Object PortMgrWin7+PORT_INFO_1
    $pi1.pName = $NewPortName
    Log "Registrando nova porta '$NewPortName' via AddPortExW..."
    $addOk = [PortMgrWin7]::AddPortEx($null, 1, [ref]$pi1, $MonitorName)
    if (-not $addOk) {
        Log "  AVISO: AddPortExW falhou (Win32 erro $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))"
    } else {
        Trace-Step "nova porta registrada no spooler"
    }

    # 3. Migra impressoras vinculadas via WMI + SetPrinter P/Invoke
    $filter = "PortName='" + $OldPortName + "'"
    $linked = Get-WmiObject Win32_Printer -Filter $filter -ErrorAction SilentlyContinue
    foreach ($wmiP in $linked) {
        Log "  Migrando impressora '$($wmiP.Name)' para porta '$NewPortName'..."
        $hP = [IntPtr]::Zero
        if ([PortMgrWin7]::OpenPrinter($wmiP.Name, [ref]$hP, [IntPtr]::Zero)) {
            $pi2                 = New-Object PortMgrWin7+PRINTER_INFO_2
            $pi2.pPrinterName    = $wmiP.Name
            $pi2.pPortName       = $NewPortName
            $pi2.pDriverName     = "Meddrive Printer DRIVER"
            $pi2.pPrintProcessor = "winprint"
            $pi2.pDatatype       = "RAW"
            $pi2.Attributes      = 0x40
            [PortMgrWin7]::SetPrinter($hP, 2, [ref]$pi2, 0) | Out-Null
            [PortMgrWin7]::ClosePrinter($hP) | Out-Null
            Trace-Step "impressora '$($wmiP.Name)' migrada"
        }
    }

    # 4. Remove a porta antiga do spooler via DeletePortW
    Log "Removendo porta antiga '$OldPortName' via DeletePortW..."
    $delOk = [PortMgrWin7]::DeletePort($null, [IntPtr]::Zero, $OldPortName)
    if (-not $delOk) {
        Log "  AVISO: DeletePortW falhou (Win32 erro $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))"
    } else {
        Trace-Step "porta antiga removida do spooler"
    }

    # 5. Remove chave antiga do registry
    Remove-Item -Path $OldPortReg -Recurse -Force
    Trace-Step "chave antiga do registry removida"

} else {
    Log "Atualizando configuracoes do perfil '$ProfileName'..."
    Set-ItemProperty -Path $OldPortReg -Name "OutputPath"        -Value $OutputPath                    -Type String
    Set-ItemProperty -Path $OldPortReg -Name "OutputBaseName"    -Value $OutputBaseName                -Type String
    Set-ItemProperty -Path $OldPortReg -Name "GhostscriptPath"   -Value $GhostscriptPath               -Type String
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
