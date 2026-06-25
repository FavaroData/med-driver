# Compativel com Windows 7 x64 e PowerShell 2.0+
# Nao usa modulo PrintManagement (ausente no Windows 7 sem RSAT)
# Rename via WMI Win32_Printer.Rename(); troca de porta via SetPrinterW P/Invoke

param(
    [Parameter(Mandatory=$true)] [string]$OldPrinterName,
    [Parameter(Mandatory=$true)] [string]$NewPrinterName,
    [Parameter(Mandatory=$true)] [string]$ProfileName
)

$LogFile   = "C:\Windows\Temp\meddrive_manager.log"
$LogWriter = New-Object System.IO.StreamWriter($LogFile, $true, [System.Text.Encoding]::Unicode)
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

$ErrorActionPreference = "Stop"

$MonitorName = "Meddrive Printer MONITOR"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$PortName    = "Meddrive Printer PORT $ProfileName"
$PortReg     = "$MonitorReg\Ports\$PortName"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win32EditPrinter {
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
    public static extern bool OpenPrinter(string pPrinterName, out IntPtr phPrinter, IntPtr pDefault);
    [DllImport("winspool.drv", SetLastError=true)]
    public static extern bool ClosePrinter(IntPtr hPrinter);
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool SetPrinter(IntPtr hPrinter, uint Level, ref PRINTER_INFO_2 pPrinter, uint Command);
}
"@ -ErrorAction SilentlyContinue

# -- Verifica impressora via WMI -------------------------------------------
$filter     = "Name='" + $OldPrinterName + "'"
$wmiPrinter = Get-WmiObject Win32_Printer -Filter $filter -ErrorAction SilentlyContinue
if (-not $wmiPrinter) {
    Log "[ERRO] Impressora '$OldPrinterName' nao encontrada."
    $LogWriter.Close()
    exit 1
}

# -- Verifica porta/perfil no registry ------------------------------------
if (-not (Test-Path $PortReg)) {
    Log "[ERRO] Porta '$PortName' nao encontrada no registry. Verifique se o perfil existe."
    $LogWriter.Close()
    exit 1
}

# -- Renomeia se necessario via WMI ---------------------------------------
if ($OldPrinterName -ne $NewPrinterName) {
    Log "[INFO] Renomeando '$OldPrinterName' para '$NewPrinterName'..."
    $wmiPrinter.Rename($NewPrinterName) | Out-Null
    Log "[INFO] Renomear concluido."
}

# -- Troca porta se necessario via SetPrinterW ----------------------------
if ($wmiPrinter.PortName -ne $PortName) {
    Log "[INFO] Alterando perfil para '$ProfileName'..."
    $hPrinter = [IntPtr]::Zero
    if ([Win32EditPrinter]::OpenPrinter($NewPrinterName, [ref]$hPrinter, [IntPtr]::Zero)) {
        $pi2                 = New-Object Win32EditPrinter+PRINTER_INFO_2
        $pi2.pPrinterName    = $NewPrinterName
        $pi2.pPortName       = $PortName
        $pi2.pDriverName     = "Meddrive Printer DRIVER"
        $pi2.pPrintProcessor = "winprint"
        $pi2.pDatatype       = "RAW"
        $pi2.Attributes      = 0x40
        $ok = [Win32EditPrinter]::SetPrinter($hPrinter, 2, [ref]$pi2, 0)
        [Win32EditPrinter]::ClosePrinter($hPrinter) | Out-Null
        if (-not $ok) {
            $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
            Log "[ERRO] SetPrinter falhou (Win32 erro $err)."
            $LogWriter.Close()
            exit 1
        }
        Log "[INFO] Porta atualizada com sucesso."
    } else {
        $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
        Log "[ERRO] OpenPrinter falhou para '$NewPrinterName' (Win32 erro $err)."
        $LogWriter.Close()
        exit 1
    }
} else {
    Log "[INFO] Perfil ja esta definido como '$ProfileName', sem alteracao."
}

Log "[OK] Impressora '$NewPrinterName' atualizada com sucesso."
$LogWriter.Close()
