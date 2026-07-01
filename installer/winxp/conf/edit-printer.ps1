# Compativel com Windows XP x86 e PowerShell 2.0+
# Nao usa modulo PrintManagement (ausente no Windows XP)
# Rename + troca de porta num unico SetPrinterW (XP nao tem Win32_Printer.Rename)

param(
    [Parameter(Mandatory=$true)] [string]$OldPrinterName,
    [Parameter(Mandatory=$true)] [string]$NewPrinterName,
    [Parameter(Mandatory=$true)] [string]$ProfileName
)

$LogFile   = "C:\Windows\Temp\meddrive_printer_manager.log"
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
    $stack = $_.ScriptStackTrace
    if (-not $stack) { $stack = "(indisponivel no PowerShell 2.0)" }
    Log "STACK         : $stack"
    $LogWriter.Close()
    exit 1
}

Log ""
Log "=== [$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] edit-printer ==="

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
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct PRINTER_DEFAULTS { public string pDatatype; public IntPtr pDevMode; public uint DesiredAccess; }
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool OpenPrinter(string pPrinterName, out IntPtr phPrinter, ref PRINTER_DEFAULTS pDefault);
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

# -- Renomeia e/ou troca porta via SetPrinterW ----------------------------
# XP: Win32_Printer nao tem metodo Rename (so Vista+). Um unico SetPrinter
# define o nome final e a porta do perfil. Handle aberto na impressora
# ANTIGA com PRINTER_ALL_ACCESS (0x000F000C) -- rename exige acesso admin.
Log "[INFO] Atualizando '$OldPrinterName' -> '$NewPrinterName' (perfil '$ProfileName')..."
$pd = New-Object Win32EditPrinter+PRINTER_DEFAULTS
$pd.DesiredAccess = 0x000F000C
$hPrinter = [IntPtr]::Zero
if ([Win32EditPrinter]::OpenPrinter($OldPrinterName, [ref]$hPrinter, [ref]$pd)) {
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
    Log "[INFO] Impressora atualizada com sucesso."
} else {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Log "[ERRO] OpenPrinter falhou para '$OldPrinterName' (Win32 erro $err)."
    $LogWriter.Close()
    exit 1
}

Log "[OK] Impressora atualizada com sucesso!"
Log "     Impressora : $NewPrinterName"
Log "     Perfil     : $ProfileName"
$LogWriter.Close()
