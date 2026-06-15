#Requires -RunAsAdministrator

$DllPath     = "$env:SystemRoot\System32\pdfmonitor.dll"
$MonitorName = "MedMonitor"
$PortName    = "MedPort"
$PrinterName = "MedPrinter"
$DriverName = "Med PDF Printer"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$PortReg     = "$MonitorReg\Ports\$PortName"

Write-Host "============================================"
Write-Host " DIAGNOSTICO MED-DRIVER"
Write-Host "============================================"

# 1. DLL em System32
Write-Host ""
Write-Host "[1] DLL em System32"
if (Test-Path $DllPath) {
    $dll = Get-Item $DllPath
    $kb  = [math]::Round($dll.Length / 1024)
    Write-Host "  OK - $DllPath ($kb KB)"
} else {
    Write-Host "  FALHOU - DLL nao encontrada"
}

# 2. Arquitetura da DLL
Write-Host ""
Write-Host "[2] Arquitetura da DLL"
try {
    $bytes   = [System.IO.File]::ReadAllBytes($DllPath)
    $offset  = [System.BitConverter]::ToInt32($bytes, 0x3C)
    $machine = [System.BitConverter]::ToInt16($bytes, $offset + 4)
    if ($machine -eq 0x8664)     { Write-Host "  OK - 64-bit (x64)" }
    elseif ($machine -eq 0x014C) { Write-Host "  ATENCAO - 32-bit (x86) incompativel com Spooler 64-bit" }
    else                         { Write-Host "  DESCONHECIDO - machine type: $machine" }
} catch {
    Write-Host "  ERRO ao ler arquitetura: $_"
}

# 3. DLL bloqueada
Write-Host ""
Write-Host "[3] DLL bloqueada pelo Windows"
$zoneFile = $DllPath + ":Zone.Identifier"
if (Test-Path $zoneFile) {
    Write-Host "  ATENCAO - DLL bloqueada. Execute: Unblock-File -Path '$DllPath'"
} else {
    Write-Host "  OK - nao bloqueada"
}

# 4. Permissoes
Write-Host ""
Write-Host "[4] Permissoes da DLL"
try {
    $acl    = Get-Acl $DllPath
    $system = $acl.Access | Where-Object { $_.IdentityReference -like "*SYSTEM*" }
    if ($system) {
        Write-Host "  OK - SYSTEM tem acesso: $($system.FileSystemRights)"
    } else {
        Write-Host "  ATENCAO - SYSTEM nao tem acesso explicito"
    }
} catch {
    Write-Host "  ERRO: $_"
}

# 5. Registry monitor
Write-Host ""
Write-Host "[5] Registry do monitor"
if (Test-Path $MonitorReg) {
    $driver = (Get-ItemProperty $MonitorReg -ErrorAction SilentlyContinue).Driver
    Write-Host "  OK - chave encontrada"
    Write-Host "  Driver = $driver"
} else {
    Write-Host "  FALHOU - chave nao encontrada: $MonitorReg"
}

# 6. Registry porta
Write-Host ""
Write-Host "[6] Registry da porta"
if (Test-Path $PortReg) {
    $props = Get-ItemProperty $PortReg -ErrorAction SilentlyContinue
    Write-Host "  OK - chave encontrada"
    Write-Host "  OutputPath      = $($props.OutputPath)"
    Write-Host "  GhostscriptPath = $($props.GhostscriptPath)"
} else {
    Write-Host "  FALHOU - chave nao encontrada: $PortReg"
}

# 7. Spooler
Write-Host ""
Write-Host "[7] Status do Spooler"
$spooler = Get-Service -Name Spooler
Write-Host "  Status: $($spooler.Status)"

# 7b. Teste AddPrinter — verifica se a impressora pode ser criada com nosso driver e porta
Write-Host ""
Write-Host "[7b] Teste AddPrinter ($DriverName + $PortName)"
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class DbgPrinter {
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
        public uint Attributes;
        public uint Priority;
        public uint DefaultPriority;
        public uint StartTime;
        public uint UntilTime;
        public uint Status;
        public uint cJobs;
        public uint AveragePPM;
    }
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern IntPtr AddPrinter(string pName, uint Level, ref PRINTER_INFO_2 pPrinter);
    [DllImport(""winspool.drv"", SetLastError=true)]
    public static extern bool ClosePrinter(IntPtr hPrinter);
}
" -ErrorAction SilentlyContinue

$testName = "DiagPrinter_Temp"
$pi = New-Object DbgPrinter+PRINTER_INFO_2
$pi.pPrinterName    = $testName
$pi.pPortName       = $PortName
$pi.pDriverName     = $DriverName
$pi.pPrintProcessor = "winprint"
$pi.pDatatype       = "RAW"
$pi.Attributes      = 0x40
Remove-Printer -Name $testName -ErrorAction SilentlyContinue
$h = [DbgPrinter]::AddPrinter($null, 2, [ref]$pi)
$e = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
if ($h -ne [IntPtr]::Zero) {
    Write-Host "  OK - AddPrinter retornou handle=$h"
    [DbgPrinter]::ClosePrinter($h) | Out-Null
    Remove-Printer -Name $testName -ErrorAction SilentlyContinue
} else {
    Write-Host "  FAIL - AddPrinter falhou, Win32 erro=$e"
}

# 7c. Driver Med PDF Printer no registry
Write-Host ""
Write-Host "[7c] Driver '$DriverName' no registry"
$driverKey = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Environments\Windows x64\Drivers\Version-3\$DriverName"
if (Test-Path $driverKey) {
    $dp = Get-ItemProperty $driverKey -ErrorAction SilentlyContinue
    Write-Host "  OK - chave encontrada"
    Write-Host "  Driver                  = $($dp.Driver)"
    Write-Host "  Configuration File      = $($dp.'Configuration File')"
    Write-Host "  Data File               = $($dp.'Data File')"
    $attrs = $dp.PrinterDriverAttributes
    $attrsLabel = if ($attrs -eq 2) { "PRINTER_DRIVER_XPS — correto" } else { "ATENCAO: esperado 2" }
    Write-Host "  PrinterDriverAttributes = $attrs ($attrsLabel)"
    Write-Host "  Dependent Files         = $($dp.'Dependent Files' -join ', ')"
    $ppdPath = "C:\Windows\System32\spool\drivers\x64\3\MEDPDF.PPD"
    if (Test-Path $ppdPath) {
        $ppd = Get-Item $ppdPath
        Write-Host "  PPD: OK - $ppdPath ($([math]::Round($ppd.Length / 1024)) KB)"
    } else {
        Write-Host "  PPD: FALHOU - nao encontrado em $ppdPath"
    }
} else {
    Write-Host "  FALHOU - chave nao encontrada: $driverKey"
}

# 7d. Impressora MedPrinter
Write-Host ""
Write-Host "[7d] Impressora '$PrinterName'"
$printer = Get-Printer -Name $PrinterName -ErrorAction SilentlyContinue
if ($printer) {
    Write-Host "  OK - impressora encontrada"
    Write-Host "  DriverName    = $($printer.DriverName)"
    Write-Host "  PortName      = $($printer.PortName)"
    Write-Host "  PrinterStatus = $($printer.PrinterStatus)"
    if ($printer.DriverName -ne $DriverName) {
        Write-Host "  ATENCAO - driver incorreto (esperado: $DriverName)"
    }
    if ($printer.PortName -ne $PortName) {
        Write-Host "  ATENCAO - porta incorreta (esperado: $PortName)"
    }
} else {
    Write-Host "  FALHOU - impressora nao encontrada"
}

# 8. Carregamento manual da DLL
Write-Host ""
Write-Host "[8] DLL carrega manualmente (LoadLibrary)"
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class DiagDll {
    [DllImport(""kernel32.dll"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);
    [DllImport(""kernel32.dll"", SetLastError=true)]
    public static extern bool FreeLibrary(IntPtr hModule);
    [DllImport(""kernel32.dll"", SetLastError=true, CharSet=CharSet.Ansi)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);
}
" -ErrorAction SilentlyContinue

$handle = [DiagDll]::LoadLibraryEx($DllPath, [IntPtr]::Zero, 0)
if ($handle -eq [IntPtr]::Zero) {
    Write-Host "  FALHOU - erro: $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error())"
} else {
    Write-Host "  OK - DLL carregada"
    $proc = [DiagDll]::GetProcAddress($handle, "InitializePrintMonitor2")
    if ($proc -eq [IntPtr]::Zero) {
        Write-Host "  FALHOU - InitializePrintMonitor2 nao encontrado"
    } else {
        Write-Host "  OK - InitializePrintMonitor2 encontrado"
    }
    [DiagDll]::FreeLibrary($handle) | Out-Null
}

# 9. Monitores no Spooler
Write-Host ""
Write-Host "[9] Monitores carregados no Spooler"
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class DiagMonitors {
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool EnumMonitors(string pName, uint Level, IntPtr pMonitor, uint cbBuf, ref uint pcbNeeded, ref uint pcReturned);
}
" -ErrorAction SilentlyContinue

$needed = [uint32]0
$returned = [uint32]0
[DiagMonitors]::EnumMonitors($null, 1, [IntPtr]::Zero, 0, [ref]$needed, [ref]$returned) | Out-Null
$buf = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([int]$needed)
[DiagMonitors]::EnumMonitors($null, 1, $buf, $needed, [ref]$needed, [ref]$returned) | Out-Null
Write-Host "  Total: $returned"
$found = $false
for ($i = 0; $i -lt [int]$returned; $i++) {
    $ptr  = [System.Runtime.InteropServices.Marshal]::ReadIntPtr([IntPtr]::Add($buf, $i * [IntPtr]::Size))
    $name = [System.Runtime.InteropServices.Marshal]::PtrToStringUni($ptr)
    if ($name -eq $MonitorName) {
        Write-Host "  $name  <-- NOSSO MONITOR"
        $found = $true
    } else {
        Write-Host "  $name"
    }
}
[System.Runtime.InteropServices.Marshal]::FreeHGlobal($buf)
if (-not $found) { Write-Host "  ATENCAO - '$MonitorName' NAO esta na lista do Spooler" }

# 10. Log Print Service
Write-Host ""
Write-Host "[10] Ultimos eventos do Print Service"
try {
    $events = Get-WinEvent -LogName "Microsoft-Windows-PrintService/Admin" -MaxEvents 10 -ErrorAction Stop
    foreach ($e in $events) {
        Write-Host "  [$($e.TimeCreated)] ID=$($e.Id) $($e.Message.Split([char]10)[0])"
    }
} catch {
    Write-Host "  Log nao disponivel ou vazio"
}

# 11. AddMonitor
Write-Host ""
Write-Host "[11] Tentando AddMonitor via API"
Add-Type -TypeDefinition "
using System;
using System.Runtime.InteropServices;
public class DiagAddMon {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct MONITOR_INFO_2 {
        public string pName;
        public string pEnvironment;
        public string pDLLName;
    }
    [DllImport(""winspool.drv"", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool AddMonitor(string pName, uint Level, ref MONITOR_INFO_2 pMonitors);
}
" -ErrorAction SilentlyContinue

$mon              = New-Object DiagAddMon+MONITOR_INFO_2
$mon.pName        = $MonitorName
$mon.pEnvironment = "Windows x64"
$mon.pDLLName     = "pdfmonitor.dll"

$result = [DiagAddMon]::AddMonitor($null, 2, [ref]$mon)
$err    = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
Write-Host "  AddMonitor resultado: $result"
Write-Host "  Erro Win32: $err"
switch ($err) {
    0   { Write-Host "  Codigo 0 = ERROR_SUCCESS" }
    5   { Write-Host "  Codigo 5 = ERROR_ACCESS_DENIED" }
    87  { Write-Host "  Codigo 87 = ERROR_INVALID_PARAMETER" }
    183 { Write-Host "  Codigo 183 = ERROR_ALREADY_EXISTS" }
    default { Write-Host "  Codigo $err - verifique System Error Codes da Microsoft" }
}

Write-Host ""
Write-Host "============================================"
Write-Host " FIM DO DIAGNOSTICO"
Write-Host "============================================"
