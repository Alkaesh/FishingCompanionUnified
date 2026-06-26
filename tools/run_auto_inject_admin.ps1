param(
    [switch]$Build,
    [int]$TargetPid = 0,
    [string]$ProcessName = "rf4_x64"
)

$ErrorActionPreference = "Stop"

$script = Join-Path $PSScriptRoot "auto_inject.ps1"
if (-not (Test-Path -LiteralPath $script)) {
    throw "auto_inject.ps1 was not found: $script"
}

$argsList = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $script
)

if ($Build) {
    $argsList += "-Build"
}
if ($TargetPid -ne 0) {
    $argsList += @("-TargetPid", $TargetPid)
}
if ($ProcessName) {
    $argsList += @("-ProcessName", $ProcessName)
}

Start-Process -FilePath "powershell.exe" -ArgumentList $argsList -Verb RunAs -Wait

