# Compativel com Windows 7 x64 e PowerShell 2.0+
# Nao usa modulo PrintManagement (ausente no Windows 7 sem RSAT)

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
    Log "STACK         : $($_.ScriptStackTrace)"
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
    [DllImport("winspool.drv", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool OpenPrinter(string pPrinterName, out IntPtr phPrinter, IntPtr pDefault);
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
$hPrinter = [IntPtr]::Zero
if ([Win32RemovePrinter]::OpenPrinter($PrinterName, [ref]$hPrinter, [IntPtr]::Zero)) {
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
