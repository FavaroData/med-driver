Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
public class PTCap2 {
    [DllImport("prntvpt.dll", CharSet=CharSet.Unicode)]
    public static extern int PTOpenProvider(string p, uint v, out IntPtr h);
    [DllImport("prntvpt.dll")]
    public static extern int PTGetPrintCapabilities(IntPtr h, IStream t, IStream c,
        [MarshalAs(UnmanagedType.BStr)] out string e);
    [DllImport("ole32.dll")]
    public static extern int CreateStreamOnHGlobal(IntPtr hg, bool del, out IStream s);
    [DllImport("prntvpt.dll")]
    public static extern int PTCloseProvider(IntPtr h);
}
"@ -ErrorAction SilentlyContinue

Write-Host "Abrindo provider..."
$h = [IntPtr]::Zero
$hr1 = [PTCap2]::PTOpenProvider("MedPrinter", 1, [ref]$h)
Write-Host "PTOpenProvider: hr=0x$($hr1.ToString('X8'))  h=$h"

if ($h -ne [IntPtr]::Zero) {
    Write-Host "Chamando PTGetPrintCapabilities (se travar, Ctrl+C)..."
    $cap = $null
    [PTCap2]::CreateStreamOnHGlobal([IntPtr]::Zero, $true, [ref]$cap) | Out-Null
    $errMsg = $null
    $hr2 = [PTCap2]::PTGetPrintCapabilities($h, $null, $cap, [ref]$errMsg)
    Write-Host "PTGetPrintCapabilities: hr=0x$($hr2.ToString('X8'))  err=$errMsg"
    [PTCap2]::PTCloseProvider($h) | Out-Null
}
