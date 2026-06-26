param(
    [Parameter(Mandatory = $true)]
    [string]$ResolvedTargets,

    [Parameter(Mandatory = $false)]
    [string]$Output = "generated\game_offsets.hpp",

    [Parameter(Mandatory = $false)]
    [string]$Namespace = "game_offsets"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

function Resolve-ProjectPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $ProjectRoot $Path
}

function Test-HexLiteral {
    param([string]$Value)
    if ($Value -notmatch "^0x[0-9A-Fa-f]+$") {
        return $false
    }
    $parsed = [UInt64]0
    return [UInt64]::TryParse(
        $Value.Substring(2),
        [System.Globalization.NumberStyles]::HexNumber,
        [System.Globalization.CultureInfo]::InvariantCulture,
        [ref]$parsed)
}

function Test-RvaLiteral {
    param([string]$Value)
    if (!(Test-HexLiteral $Value)) {
        return $false
    }
    $parsed = [Convert]::ToUInt64($Value.Substring(2), 16)
    return $parsed -gt 0 -and $parsed -le ([Convert]::ToUInt64("FFFFFFFF", 16))
}

function Convert-ToIdentifier {
    param([string]$Value)

    $clean = [regex]::Replace($Value, "\([0-9]+\)", "")
    $clean = [regex]::Replace($clean, "[^A-Za-z0-9_]+", "_")
    $clean = $clean.Trim("_")
    if ([string]::IsNullOrWhiteSpace($clean)) {
        return "unnamed"
    }
    if ($clean[0] -match "[0-9]") {
        $clean = "_" + $clean
    }
    return $clean
}

$ResolvedTargetsPath = Resolve-ProjectPath $ResolvedTargets
$OutputPath = Resolve-ProjectPath $Output

if (!(Test-Path -LiteralPath $ResolvedTargetsPath)) {
    throw "Resolved targets file was not found: $ResolvedTargetsPath"
}

$rows = Import-Csv -Delimiter ';' -LiteralPath $ResolvedTargetsPath
$usable = @()
$rejected = @()
foreach ($row in $rows) {
    if ($row.offset_hex -eq "NOT_FOUND" -or [string]::IsNullOrWhiteSpace($row.offset_hex)) {
        continue
    }
    if ($row.assembly -eq "INFO" -or $row.assembly -eq "ERROR") {
        continue
    }
    if ($row.kind -eq "method" -and !(Test-RvaLiteral $row.offset_hex)) {
        $rejected += $row
        continue
    }
    if ($row.kind -eq "field" -and !(Test-HexLiteral $row.offset_hex)) {
        $rejected += $row
        continue
    }
    $usable += $row
}

$outputDir = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($outputDir) -and !(Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("#pragma once")
$lines.Add("")
$lines.Add("#include <cstdint>")
$lines.Add("#include `"../include/il2cpp_runtime_sdk.hpp`"")
$lines.Add("")
$lines.Add("namespace $Namespace {")
$lines.Add("")
$lines.Add("struct TargetInfo {")
$lines.Add("    const char* assembly;")
$lines.Add("    const char* type;")
$lines.Add("    const char* alias;")
$lines.Add("    const char* member;")
$lines.Add("    const char* kind;")
$lines.Add("    uintptr_t offset;")
$lines.Add("};")
$lines.Add("")

foreach ($row in $usable) {
    $typeName = if (![string]::IsNullOrWhiteSpace($row.alias)) { $row.alias } else { $row.resolved_type }
    $typeId = Convert-ToIdentifier $typeName
    $memberId = Convert-ToIdentifier $row.member
    $kindId = Convert-ToIdentifier $row.kind
    $constName = "${typeId}_${memberId}_${kindId}"

    $lines.Add("inline constexpr uintptr_t $constName = $($row.offset_hex);")
}

$lines.Add("")
$lines.Add("inline constexpr TargetInfo targets[] = {")
foreach ($row in $usable) {
    $assembly = $row.assembly.Replace("\", "\\").Replace('"', '\"')
    $type = $row.resolved_type.Replace("\", "\\").Replace('"', '\"')
    $alias = $row.alias.Replace("\", "\\").Replace('"', '\"')
    $member = $row.member.Replace("\", "\\").Replace('"', '\"')
    $kind = $row.kind.Replace("\", "\\").Replace('"', '\"')
    $lines.Add("    {`"$assembly`", `"$type`", `"$alias`", `"$member`", `"$kind`", $($row.offset_hex)},")
}
$lines.Add("};")
$lines.Add("")
$lines.Add("} // namespace $Namespace")

[System.IO.File]::WriteAllLines($OutputPath, $lines)

Write-Host "Generated $Output with $($usable.Count) targets."
if ($rejected.Count -gt 0) {
    Write-Warning "Rejected $($rejected.Count) invalid target offsets."
}
