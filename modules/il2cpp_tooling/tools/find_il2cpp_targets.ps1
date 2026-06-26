param(
    [Parameter(Mandatory = $true)]
    [string]$Offsets,

    [Parameter(Mandatory = $false)]
    [string[]]$Keywords = @("Fishing", "Player", "Inventory", "Item", "Water", "Weather", "UI"),

    [Parameter(Mandatory = $false)]
    [string]$Output = "generated\candidate_targets.txt",

    [Parameter(Mandatory = $false)]
    [int]$MaxClasses = 200,

    [Parameter(Mandatory = $false)]
    [int]$MaxMembersPerClass = 24
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

if (!(Test-Path -LiteralPath $Offsets)) {
    throw "Offsets file was not found: $Offsets"
}

$OutputPath = Resolve-ProjectPath $Output

$Keywords = @($Keywords | ForEach-Object {
    $_ -split ","
} | ForEach-Object {
    $_.Trim()
} | Where-Object {
    ![string]::IsNullOrWhiteSpace($_)
})

$rows = Import-Csv -Delimiter ';' -LiteralPath $Offsets
$matches = foreach ($row in $rows) {
    $haystack = "$($row.type) $($row.alias) $($row.member)"
    foreach ($keyword in $Keywords) {
        if ($haystack -like "*$keyword*") {
            [pscustomobject]@{
                Keyword = $keyword
                Assembly = $row.assembly
                Type = $row.type
                Alias = $row.alias
                Member = $row.member
                Kind = $row.kind
                Offset = $row.offset_hex
            }
            break
        }
    }
}

$classGroups = $matches |
    Group-Object Assembly, Type, Alias |
    Sort-Object Count -Descending |
    Select-Object -First $MaxClasses

$outputDir = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($outputDir) -and !(Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# Candidate IL2CPP targets")
$lines.Add("# Generated from: $Offsets")
$lines.Add("# Keywords: $($Keywords -join ', ')")
$lines.Add("#")
$lines.Add("# Copy interesting non-comment lines into il2cpp_targets.txt next to the game exe.")
$lines.Add("# Format: assembly;type_or_alias;member;kind")
$lines.Add("")

foreach ($group in $classGroups) {
    $items = @($group.Group)
    $first = $items[0]
    $displayType = if (![string]::IsNullOrWhiteSpace($first.Alias)) { $first.Alias } else { $first.Type }
    $lines.Add("# [$($first.Assembly)] $displayType")
    if ($displayType -ne $first.Type) {
        $lines.Add("# obf/type: $($first.Type)")
    }

    $members = $items |
        Sort-Object @{ Expression = { if ($_.Kind -eq "method") { 0 } else { 1 } } }, Member |
        Select-Object -First $MaxMembersPerClass

    foreach ($member in $members) {
        $lines.Add("$($member.Assembly);$displayType;$($member.Member);$($member.Kind)")
    }
    $lines.Add("")
}

[System.IO.File]::WriteAllLines($OutputPath, $lines)

$summary = $classGroups | ForEach-Object {
    $items = @($_.Group)
    $first = $items[0]
    [pscustomobject]@{
        Count = $items.Count
        Assembly = $first.Assembly
        Type = $first.Type
        Alias = $first.Alias
    }
}

$summary | Format-Table -AutoSize
Write-Host "Generated $OutputPath with $(@($classGroups).Count) class groups."
