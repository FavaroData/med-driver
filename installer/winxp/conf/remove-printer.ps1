# Compativel com Windows XP x86 e PowerShell 2.0+
# Nao usa modulo PrintManagement (ausente no Windows XP)

param(
    [string]$PrinterName = "Meddrive Printer"
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
Log "=== [$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] remove-printer ==="

$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win32RemovePrinter {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct PRINTER_DEFAULTS { public string pDatatype; public IntPtr pDevMode; public uint DesiredAccess; }
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool OpenPrinter(string pPrinterName, out IntPtr phPrinter, ref PRINTER_DEFAULTS pDefault);
    [DllImport("winspool.drv", SetLastError=true)]
    public static extern bool ClosePrinter(IntPtr hPrinter);
    [DllImport("winspool.drv", SetLastError=true)]
    public static extern bool DeletePrinter(IntPtr hPrinter);
}
"@ -ErrorAction SilentlyContinue

$spoolerStatus = (Get-Service Spooler -ErrorAction SilentlyContinue).Status
if ($spoolerStatus -ne 'Running') {
    Start-Service -Name Spooler
    Start-Sleep -Seconds 3
}

Log "[INFO] Removendo impressora '$PrinterName'..."
# DeletePrinter exige handle com PRINTER_ALL_ACCESS (0x000F000C);
# sem PRINTER_DEFAULTS o OpenPrinter da so PRINTER_ACCESS_USE -> DeletePrinter erro 5
$pd = New-Object Win32RemovePrinter+PRINTER_DEFAULTS
$pd.DesiredAccess = 0x000F000C
$hPrinter = [IntPtr]::Zero
if ([Win32RemovePrinter]::OpenPrinter($PrinterName, [ref]$hPrinter, [ref]$pd)) {
    $ok = [Win32RemovePrinter]::DeletePrinter($hPrinter)
    [Win32RemovePrinter]::ClosePrinter($hPrinter) | Out-Null
    if ($ok) {
        Log "[OK] Impressora removida com sucesso!"
    Log "     Impressora : $PrinterName"
    } else {
        $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
        Log "[AVISO] DeletePrinter falhou (Win32 erro $err)"
    }
} else {
    Log "[AVISO] Impressora '$PrinterName' nao encontrada no sistema"
}

$LogWriter.Close()
