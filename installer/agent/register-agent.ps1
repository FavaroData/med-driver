# Compativel com Windows 7 x64 e PowerShell 2.0+
# Registra o MeddrivePrinterAgent no Task Scheduler para iniciar no login do usuario.
# Usa a API COM do Task Scheduler (disponivel desde Vista).

trap {
    Write-Output "EXCEPTION TYPE: $($_.Exception.GetType().FullName)"
    Write-Output "EXCEPTION MSG : $($_.Exception.Message)"
    Write-Output "LINE          : $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
    exit 1
}

$AgentPath = "$env:ProgramData\Meddrive Printer\MeddrivePrinterAgent.exe"

if (-not (Test-Path $AgentPath)) {
    Write-Output "ERRO: MeddrivePrinterAgent.exe nao encontrado em $AgentPath"
    exit 1
}

$svc = New-Object -ComObject "Schedule.Service"
$svc.Connect()
$root = $svc.GetFolder("\")

$def = $svc.NewTask(0)
$def.RegistrationInfo.Description = "MeddrivePrinterAgent  -  agente de impressao PDF Meddrive"
$def.Settings.RestartCount        = 3
$def.Settings.RestartInterval     = "PT1M"
$def.Settings.ExecutionTimeLimit  = "PT0S"
$def.Settings.DisallowStartIfOnBatteries = $false
$def.Settings.StopIfGoingOnBatteries    = $false

$trigger         = $def.Triggers.Create(9)  # TASK_TRIGGER_LOGON  -  qualquer usuario
$trigger.Enabled = $true

$action      = $def.Actions.Create(0)  # TASK_ACTION_EXEC
$action.Path = $AgentPath

# TASK_CREATE_OR_UPDATE=6, sem usuario/senha, TASK_LOGON_INTERACTIVE_TOKEN=3
$root.RegisterTaskDefinition(
    "MeddrivePrinterAgent",
    $def,
    6,
    $null, $null,
    3
) | Out-Null

Write-Output "  OK - tarefa 'MeddrivePrinterAgent' registrada no Task Scheduler"
