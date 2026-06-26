param(
    [Parameter(Mandatory = $true)]
    [string]$EnrichedCsv,

    [Parameter(Mandatory = $true)]
    [string[]]$Types,

    [Parameter(Mandatory = $false)]
    [string]$Output = "generated\action_offsets.hpp",

    [Parameter(Mandatory = $false)]
    [string]$Namespace = "action_offsets"
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
    $clean = [regex]::Replace($Value, "\(([0-9]+)\)", '_$1')
    $clean = [regex]::Replace($clean, "[^A-Za-z0-9_]+", "_")
    $clean = $clean.Trim("_")
    if ([string]::IsNullOrWhiteSpace($clean)) { return "unnamed" }
    if ($clean[0] -match "[0-9]") { $clean = "_" + $clean }
    return $clean
}

$EnrichedCsvPath = Resolve-ProjectPath $EnrichedCsv
$OutputPath = Resolve-ProjectPath $Output

if (!(Test-Path -LiteralPath $EnrichedCsvPath)) {
    throw "CSV was not found: $EnrichedCsvPath"
}

$Types = @($Types | ForEach-Object {
    $_ -split ","
} | ForEach-Object {
    $_.Trim()
} | Where-Object {
    ![string]::IsNullOrWhiteSpace($_)
})

$rows = Import-Csv -LiteralPath $EnrichedCsvPath | Where-Object {
    $Types -contains $_.Type
}

$outputDir = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($outputDir) -and !(Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("#pragma once")
$lines.Add("")
$lines.Add("#include <cstdint>")
$lines.Add("")
$lines.Add("namespace $Namespace {")
$lines.Add("")

$seenNames = @{}
$rejected = 0
foreach ($row in $rows) {
    $typeId = Convert-ToIdentifier $row.Type
    $memberId = Convert-ToIdentifier $row.RuntimeMember
    if ($row.Kind -eq "method" -and ![string]::IsNullOrWhiteSpace($row.RvaHex)) {
        if (!(Test-RvaLiteral $row.RvaHex)) {
            $rejected += 1
            continue
        }
        $name = "${typeId}_${memberId}_method"
        if ($seenNames.ContainsKey($name)) {
            $seenNames[$name] += 1
            $name = "${name}_$($seenNames[$name])"
        } else {
            $seenNames[$name] = 1
        }
        $lines.Add("inline constexpr uintptr_t $name = $($row.RvaHex);")
    } elseif ($row.Kind -eq "field" -and ![string]::IsNullOrWhiteSpace($row.OffsetHex)) {
        if (!(Test-HexLiteral $row.OffsetHex)) {
            $rejected += 1
            continue
        }
        $name = "${typeId}_${memberId}_field"
        if ($seenNames.ContainsKey($name)) {
            $seenNames[$name] += 1
            $name = "${name}_$($seenNames[$name])"
        } else {
            $seenNames[$name] = 1
        }
        $lines.Add("inline constexpr uintptr_t $name = $($row.OffsetHex);")
    }
}

$lines.Add("")
$lines.Add("} // namespace $Namespace")

[System.IO.File]::WriteAllLines($OutputPath, $lines)
Write-Host "Generated $Output with $(@($rows).Count) rows."
if ($rejected -gt 0) {
    Write-Warning "Rejected $rejected invalid offsets."
}
