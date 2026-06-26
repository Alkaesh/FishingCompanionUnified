param(
    [switch]$Build,
    [switch]$UnloadOnly,
    [int]$TargetPid = 0,
    [string]$ProcessName = "rf4_x64",
    [string]$ConfigurePreset = "",
    [string]$BuildPreset = "",
    [string]$BuildDir = "",
    [string]$SourceDll = "",
    [string]$TargetDll = "",
    [int]$UnloadTimeoutSeconds = 0,
    [int]$InjectTimeoutSeconds = 0
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
if ($UnloadOnly) {
    $argsList += "-UnloadOnly"
}
if ($TargetPid -ne 0) {
    $argsList += @("-TargetPid", $TargetPid)
}
if ($ProcessName) {
    $argsList += @("-ProcessName", $ProcessName)
}
if ($ConfigurePreset) {
    $argsList += @("-ConfigurePreset", $ConfigurePreset)
}
if ($BuildPreset) {
    $argsList += @("-BuildPreset", $BuildPreset)
}
if ($BuildDir) {
    $argsList += @("-BuildDir", $BuildDir)
}
if ($SourceDll) {
    $argsList += @("-SourceDll", $SourceDll)
}
if ($TargetDll) {
    $argsList += @("-TargetDll", $TargetDll)
}
if ($UnloadTimeoutSeconds -gt 0) {
    $argsList += @("-UnloadTimeoutSeconds", $UnloadTimeoutSeconds)
}
if ($InjectTimeoutSeconds -gt 0) {
    $argsList += @("-InjectTimeoutSeconds", $InjectTimeoutSeconds)
}

Start-Process -FilePath "powershell.exe" -ArgumentList $argsList -Verb RunAs -Wait
