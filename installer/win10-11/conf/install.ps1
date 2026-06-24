#Requires -RunAsAdministrator

# Instala as dependencias do sistema: DLL, monitor, driver PSCRIPT5, PPD.
# Distribui tambem o aplicativo de gerenciamento para ProgramData.
# NAO cria impressoras  -  isso e responsabilidade de add-printer.ps1.

trap {
    $msg = "[TRAP] $($_.Exception.Message)  - linha $($_.InvocationInfo.ScriptLineNumber)"
    try { $msg | Out-File "C:\Windows\Temp\meddrive_preinit.log" -Append -Encoding UTF8 } catch {}
    Write-Output "EXCEPTION TYPE: $($_.Exception.GetType().FullName)"
    Write-Output "EXCEPTION MSG : $($_.Exception.Message)"
    Write-Output "LINE          : $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
    Write-Output "STACK         : $($_.ScriptStackTrace)"
    exit 1
}
function Trace-Step($msg) { Write-Output "CHECKPOINT: $msg" }

# Log pre-transcript  -  captura falhas antes do Start-Transcript
try { "[$(Get-Date -Format 'HH:mm:ss')] install.ps1 iniciando" | Out-File "C:\Windows\Temp\meddrive_preinit.log" -Append -Encoding UTF8 } catch {}

Trace-Step "inicio do script"
Start-Transcript -Path "C:\Windows\Temp\meddrive_ps_install.log" -Force
Trace-Step "Start-Transcript OK"

$ErrorActionPreference = "Stop"

# $PSScriptRoot e mais confiavel que $MyInvocation.MyCommand.Path quando chamado via nsExec
if ($PSScriptRoot -and (Test-Path $PSScriptRoot)) {
    $ScriptDir = $PSScriptRoot
} else {
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}
Trace-Step "ScriptDir resolvido: $ScriptDir"

$DllSource   = Join-Path $ScriptDir "..\meddrivemon.dll"
$DllDest     = "$env:SystemRoot\System32\meddrivemon.dll"
$MonitorName = "Meddrive Printer MONITOR"
$DriverName  = "Meddrive Printer DRIVER"
$MonitorReg  = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\$MonitorName"
$driverKey   = "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Environments\Windows x64\Drivers\Version-3\$DriverName"
$PpdSource   = Join-Path $ScriptDir "MEDDRIVE.PPD"
$PpdDest     = "$env:SystemRoot\System32\spool\drivers\x64\3\MEDDRIVE.PPD"
$AppDir      = "$env:ProgramData\Meddrive Printer"

# -- 1. Para o Spooler e copia a DLL --------------------------------------
Write-Host "Parando o Spooler..."
Stop-Service -Name Spooler -Force
$p = Get-Process -Name spoolsv -ErrorAction SilentlyContinue
if ($p) { $p.WaitForExit() }
Trace-Step "Spooler parado"

Write-Host "Copiando DLL para System32..."
if (-not (Test-Path $DllSource)) {
    Write-Host "ERRO: DLL nao encontrada em $DllSource"
    exit 1
}
Copy-Item $DllSource $DllDest -Force
Trace-Step "DLL copiada"

# -- 2. Registra o monitor no registry ------------------------------------
Write-Host "Registrando monitor no registry..."
if (-not (Test-Path $MonitorReg)) {
    New-Item -Path $MonitorReg | Out-Null
}
Set-ItemProperty -Path $MonitorReg -Name "Driver" -Value "meddrivemon.dll" -Type String
Trace-Step "monitor registrado"

# -- 3. Driver Generic / Text Only (base exigida pelo Windows) ------------
Write-Host "Verificando driver 'Generic / Text Only'..."
Start-Service -Name Spooler
Start-Sleep -Seconds 3
if (-not (Get-PrinterDriver -Name "Generic / Text Only" -ErrorAction SilentlyContinue)) {
    Write-Host "  Instalando 'Generic / Text Only'..."
    Add-PrinterDriver -Name "Generic / Text Only" -ErrorAction Stop
    Write-Host "  OK - instalado"
} else {
    Write-Host "  OK - ja presente"
}

# -- 4. Driver PSCRIPT5 customizado ---------------------------------------
Write-Host "Instalando driver PSCRIPT5 customizado..."
if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Stop-Service -Name Spooler -Force
    $p = Get-Process -Name spoolsv -ErrorAction SilentlyContinue
    if ($p) { $p.WaitForExit() }

    New-Item -Path $driverKey -Force | Out-Null
    Set-ItemProperty $driverKey -Name "Configuration File"      -Value "PS5UI.DLL"
    Set-ItemProperty $driverKey -Name "Data File"               -Value "PSCRIPT.NTF"
    Set-ItemProperty $driverKey -Name "Driver"                  -Value "PSCRIPT5.DLL"
    Set-ItemProperty $driverKey -Name "Help File"               -Value "PSCRIPT.HLP"
    Set-ItemProperty $driverKey -Name "Driver Version"          -Value 3 -Type DWord
    Set-ItemProperty $driverKey -Name "Version"                 -Value 3 -Type DWord
    # PRINTER_DRIVER_XPS (0x2)  -  habilita Print Ticket Provider do PSCRIPT5.
    Set-ItemProperty $driverKey -Name "PrinterDriverAttributes" -Value 2 -Type DWord
    Write-Host "  OK - driver '$DriverName' registrado via registry"
} else {
    Write-Host "  OK - driver '$DriverName' ja instalado"
}

# -- 5. Copia o PPD e registra em Dependent Files -------------------------
Write-Host "Instalando PPD do driver..."
if (-not (Test-Path $PpdSource)) {
    Write-Host "ERRO: MEDDRIVE.PPD nao encontrado em $PpdSource"
    exit 1
}
Copy-Item $PpdSource $PpdDest -Force
Set-ItemProperty $driverKey -Name "Dependent Files" -Value @("MEDDRIVE.PPD", "") -Type MultiString
Write-Host "  OK - PPD copiado e registrado em Dependent Files"

# -- 6. Reinicia o Spooler para enumerar o driver -------------------------
Write-Host "Reiniciando o Spooler para enumerar o driver..."
Restart-Service -Name Spooler -Force
Start-Sleep -Seconds 3
if (-not (Get-PrinterDriver -Name $DriverName -ErrorAction SilentlyContinue)) {
    Write-Host "ERRO: driver '$DriverName' nao reconhecido pelo spooler apos registro"
    exit 1
}
Write-Host "  OK - driver '$DriverName' reconhecido pelo spooler"

# -- 7. Distribui arquivos do aplicativo para ProgramData -----------------
Write-Host "Copiando aplicativo para $AppDir..."
New-Item -ItemType Directory -Path $AppDir -Force | Out-Null
$ConfDir = Join-Path $AppDir "conf"
New-Item -ItemType Directory -Path $ConfDir -Force | Out-Null
Copy-Item (Join-Path $ScriptDir "conf\add-printer.ps1")    "$ConfDir\add-printer.ps1"    -Force
Copy-Item (Join-Path $ScriptDir "conf\create-profile.ps1") "$ConfDir\create-profile.ps1" -Force
Copy-Item (Join-Path $ScriptDir "conf\edit-profile.ps1")   "$ConfDir\edit-profile.ps1"   -Force
Copy-Item (Join-Path $ScriptDir "conf\edit-printer.ps1")   "$ConfDir\edit-printer.ps1"   -Force
Copy-Item (Join-Path $ScriptDir "conf\remove-printer.ps1") "$ConfDir\remove-printer.ps1" -Force
Copy-Item (Join-Path $ScriptDir "conf\remove-profile.ps1") "$ConfDir\remove-profile.ps1" -Force
Copy-Item (Join-Path $ScriptDir "MEDDRIVE.PPD")            "$AppDir\MEDDRIVE.PPD"        -Force
Copy-Item (Join-Path $ScriptDir "MedDriveManager.exe")         "$AppDir\MedDriveManager.exe"         -Force
Copy-Item (Join-Path $ScriptDir "MeddrivePrinterAgent.exe")    "$AppDir\MeddrivePrinterAgent.exe"    -Force
Trace-Step "arquivos do aplicativo copiados"

Write-Host ""
Write-Host "Instalacao concluida!"
Write-Host "  Monitor    : $MonitorName"
Write-Host "  Driver     : $DriverName"
Write-Host "  Aplicativo : $AppDir\MedDriveManager.exe"
Write-Host "  Ghostscript: $env:ProgramData\Meddrive Printer\Ghostscript\bin\gswin64c.exe"
