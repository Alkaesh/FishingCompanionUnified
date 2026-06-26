param(
    [Parameter(Mandatory = $true)]
    [string]$EnrichedCsv,

    [Parameter(Mandatory = $false)]
    [string[]]$TypeFilter = @(),

    [Parameter(Mandatory = $false)]
    [string]$Output = "generated\il2cpp_wrappers.hpp",

    [Parameter(Mandatory = $false)]
    [string]$Namespace = "game_wrappers",

    [Parameter(Mandatory = $false)]
    [int]$MaxMethodsPerType = 80,

    [Parameter(Mandatory = $false)]
    [int]$MaxFieldsPerType = 80
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
    if ([string]::IsNullOrWhiteSpace($clean)) { return "unnamed" }
    if ($clean[0] -match "[0-9]") { $clean = "_" + $clean }
    return $clean
}

function Convert-Type {
    param([string]$Type)

    $t = $Type.Trim()
    $t = $t -replace "\b(public|private|protected|internal|static|virtual|override|abstract|sealed|readonly|const|extern|unsafe|new)\b", ""
    $t = $t.Trim()

    switch -Regex ($t) {
        "^void$" { return "void" }
        "^bool$|^System.Boolean$" { return "bool" }
        "^byte$|^System.Byte$" { return "uint8_t" }
        "^sbyte$|^System.SByte$" { return "int8_t" }
        "^short$|^System.Int16$" { return "int16_t" }
        "^ushort$|^System.UInt16$" { return "uint16_t" }
        "^int$|^System.Int32$" { return "int32_t" }
        "^uint$|^System.UInt32$" { return "uint32_t" }
        "^long$|^System.Int64$" { return "int64_t" }
        "^ulong$|^System.UInt64$" { return "uint64_t" }
        "^float$|^System.Single$" { return "float" }
        "^double$|^System.Double$" { return "double" }
        "^string$|^System.String$" { return "void*" }
        default { return "void*" }
    }
}

function Convert-ParamTypes {
    param([string]$Params)

    if ([string]::IsNullOrWhiteSpace($Params)) {
        return ""
    }

    $types = New-Object System.Collections.Generic.List[string]
    foreach ($param in ($Params -split ",")) {
        $p = $param.Trim()
        if ([string]::IsNullOrWhiteSpace($p)) { continue }
        $pieces = $p -split "\s+"
        if ($pieces.Count -le 1) {
            $types.Add((Convert-Type $p))
        } else {
            $typePart = ($pieces[0..($pieces.Count - 2)] -join " ")
            $types.Add((Convert-Type $typePart))
        }
    }
    return ($types -join ", ")
}

$EnrichedCsvPath = Resolve-ProjectPath $EnrichedCsv
$OutputPath = Resolve-ProjectPath $Output

if (!(Test-Path -LiteralPath $EnrichedCsvPath)) {
    throw "CSV was not found: $EnrichedCsvPath"
}

$rows = Import-Csv -LiteralPath $EnrichedCsvPath
if ($TypeFilter.Count -gt 0) {
    $rows = $rows | Where-Object {
        $type = $_.Type
        $alias = $_.Alias
        foreach ($filter in $TypeFilter) {
            if ($type -like $filter -or $alias -like $filter) { return $true }
        }
        return $false
    }
}

$groups = $rows |
    Where-Object { $_.Kind -eq "field" -or $_.Kind -eq "method" } |
    Group-Object Type |
    Sort-Object Name

$outputDir = Split-Path -Parent $OutputPath
if (![string]::IsNullOrWhiteSpace($outputDir) -and !(Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("#pragma once")
$lines.Add("")
$lines.Add("#include <cstdint>")
$lines.Add('#include "../include/il2cpp_interaction_sdk.hpp"')
$lines.Add("")
$lines.Add("namespace $Namespace {")
$lines.Add("")
$lines.Add("using il2cpp_runtime::Field;")
$lines.Add("using il2cpp_runtime::InstanceMethod;")
$lines.Add("using il2cpp_runtime::StaticMethod;")
$lines.Add("")

foreach ($group in $groups) {
    $items = @($group.Group)
    if ($items.Count -eq 0) { continue }

    $first = $items[0]
    $typeName = if (![string]::IsNullOrWhiteSpace($first.Alias)) { $first.Alias } else { $first.Type }
    $structName = Convert-ToIdentifier $typeName

    $lines.Add("struct $structName {")
    $lines.Add("    static constexpr const char* type_name = `"$($first.Type)`";")
    $lines.Add("    static constexpr const char* alias = `"$($first.Alias)`";")
    $lines.Add("")

    $fields = $items |
        Where-Object { $_.Kind -eq "field" -and ![string]::IsNullOrWhiteSpace($_.OffsetHex) -and (Test-HexLiteral $_.OffsetHex) } |
        Select-Object -First $MaxFieldsPerType

    foreach ($field in $fields) {
        $name = Convert-ToIdentifier $field.Member
        $cppType = Convert-Type $field.FieldType
        $lines.Add("    static constexpr uintptr_t ${name}_offset = $($field.OffsetHex);")
        $lines.Add("    static Field<$cppType> $name() { return Field<$cppType>(${name}_offset); }")
    }

    if (@($fields).Count -gt 0) {
        $lines.Add("")
    }

    $methods = $items |
        Where-Object { $_.Kind -eq "method" -and ![string]::IsNullOrWhiteSpace($_.RvaHex) -and (Test-RvaLiteral $_.RvaHex) -and $_.Member -notlike ".ctor*" -and $_.Member -notlike ".cctor*" } |
        Select-Object -First $MaxMethodsPerType

    foreach ($method in $methods) {
        $name = Convert-ToIdentifier $method.RuntimeMember
        $returnType = Convert-Type $method.ReturnType
        $paramTypes = Convert-ParamTypes $method.Params
        $methodTemplate = if ($method.IsStatic -eq "True") { "StaticMethod" } else { "InstanceMethod" }
        $templateArgs = if ([string]::IsNullOrWhiteSpace($paramTypes)) { $returnType } else { "$returnType, $paramTypes" }

        $lines.Add("    static constexpr uintptr_t ${name}_rva = $($method.RvaHex);")
        $lines.Add("    static $methodTemplate<$templateArgs> $name() { return $methodTemplate<$templateArgs>(${name}_rva); }")
    }

    $lines.Add("};")
    $lines.Add("")
}

$lines.Add("} // namespace $Namespace")

[System.IO.File]::WriteAllLines($OutputPath, $lines)

Write-Host "Generated $Output with $(@($groups).Count) wrapper structs."
