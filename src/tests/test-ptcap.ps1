Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PTCap3 {
    [DllImport("prntvpt.dll", CharSet=CharSet.Unicode)]
    public static extern int PTOpenProvider(string p, uint v, out IntPtr h);
    [DllImport("prntvpt.dll")]
    public static extern int PTGetPrintCapabilities(IntPtr h, IntPtr pTicket,
        out IntPtr ppCap, [MarshalAs(UnmanagedType.BStr)] out string e);
    [DllImport("prntvpt.dll")]
    public static extern int PTCloseProvider(IntPtr h);
}
"@ -ErrorAction SilentlyContinue

Write-Host "PTOpenProvider..."
$h = [IntPtr]::Zero
$hr1 = [PTCap3]::PTOpenProvider("Meddrive Printer", 1, [ref]$h)
Write-Host "  hr=0x$($hr1.ToString('X8'))  h=$h"

if ($h -ne [IntPtr]::Zero) {
    Write-Host "PTGetPrintCapabilities (Ctrl+C se travar)..."
    $pCap = [IntPtr]::Zero
    $errMsg = $null
    $hr2 = [PTCap3]::PTGetPrintCapabilities($h, [IntPtr]::Zero, [ref]$pCap, [ref]$errMsg)
    Write-Host "  hr=0x$($hr2.ToString('X8'))  cap=$pCap  err=$errMsg"
    [PTCap3]::PTCloseProvider($h) | Out-Null
}
