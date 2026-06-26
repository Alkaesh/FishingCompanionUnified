param(
    [Parameter(Mandatory = $true)]
    [string]$DumpCs,

    [Parameter(Mandatory = $true)]
    [string]$Offsets,

    [Parameter(Mandatory = $false)]
    [string]$OutputCsv = "generated\il2cpp_enriched_targets.csv",

    [Parameter(Mandatory = $false)]
    [string]$OutputTargets = "generated\il2cpp_enriched_targets.txt"
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

function Get-ParamCount {
    param([string]$Params)
    $text = $Params.Trim()
    if ([string]::IsNullOrWhiteSpace($text)) {
        return 0
    }
    return ($text -split ",").Count
}

function Get-LastToken {
    param([string]$Text)
    $parts = $Text.Trim() -split "\s+"
    return $parts[$parts.Count - 1]
}

function Normalize-MethodName {
    param([string]$Name, [string]$Params)
    return "$Name($(Get-ParamCount $Params))"
}

function Convert-ToCsvSafeRows {
    param($Rows)
    $Rows | Select-Object `
        Assembly,
        Type,
        Alias,
        Parent,
        InstanceSize,
        Kind,
        Member,
        RuntimeMember,
        Access,
        IsStatic,
        IsVirtual,
        ReturnType,
        FieldType,
        Params,
        OffsetHex,
        RvaHex,
        Signature
}

$DumpCsPath = Resolve-ProjectPath $DumpCs
$OffsetsPath = Resolve-ProjectPath $Offsets
$OutputCsvPath = Resolve-ProjectPath $OutputCsv
$OutputTargetsPath = Resolve-ProjectPath $OutputTargets

if (!(Test-Path -LiteralPath $DumpCsPath)) {
    throw "Dump file was not found: $DumpCsPath"
}
if (!(Test-Path -LiteralPath $OffsetsPath)) {
    throw "Offsets file was not found: $OffsetsPath"
}

$outputDir = Split-Path -Parent $OutputCsvPath
if (![string]::IsNullOrWhiteSpace($outputDir) -and !(Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$targetOutputDir = Split-Path -Parent $OutputTargetsPath
if (![string]::IsNullOrWhiteSpace($targetOutputDir) -and !(Test-Path -LiteralPath $targetOutputDir)) {
    New-Item -ItemType Directory -Path $targetOutputDir | Out-Null
}

$offsetRows = Import-Csv -Delimiter ';' -LiteralPath $OffsetsPath
$aliasByType = @{}
$runtimeByKey = @{}
foreach ($row in $offsetRows) {
    $aliasByType["$($row.assembly);$($row.type)"] = $row.alias
    $runtimeByKey["$($row.assembly);$($row.type);$($row.member);$($row.kind)"] = $row.offset_hex
}

$rows = New-Object System.Collections.Generic.List[object]
$assembly = ""
$namespaceStack = New-Object System.Collections.Generic.Stack[string]
$currentType = ""
$currentAlias = ""
$currentParent = ""
$currentInstanceSize = ""
$braceDepth = 0
$classDepth = -1

foreach ($line in [System.IO.File]::ReadLines($DumpCsPath)) {
    if ($line -match "^// Image:\s*(.+?)\s*\(") {
        $assembly = $Matches[1].Trim()
        continue
    }

    if ($line -match "^\s*namespace\s+([A-Za-z0-9_.]+)\s*$") {
        $namespaceStack.Push($Matches[1])
        continue
    }

    if ($line -match "^\s*// TypeDefIndex:.*Instance Size:\s*(0x[0-9A-Fa-f]+)") {
        $currentInstanceSize = $Matches[1]
        continue
    }

    if ($line -match "^\s*// Parent:\s*(.+)$") {
        $currentParent = $Matches[1].Trim()
        continue
    }

    if ($line -match "^\s*(?:sealed\s+|abstract\s+|static\s+)?(?:class|enum|struct|interface)\s+([^\s:]+).*?(?:\/\/\s*(.+))?$") {
        $rawName = $Matches[1].Trim()
        $commentName = ""
        if ($Matches.Count -gt 2) {
            $commentName = $Matches[2].Trim()
        }

        if (![string]::IsNullOrWhiteSpace($commentName) -and $commentName -notmatch "^\s*$") {
            $currentType = $commentName
        } elseif ($namespaceStack.Count -gt 0) {
            $currentType = "$($namespaceStack.Peek()).$rawName"
        } else {
            $currentType = $rawName
        }

        $aliasKey = "$assembly;$currentType"
        $currentAlias = if ($aliasByType.ContainsKey($aliasKey)) { $aliasByType[$aliasKey] } else { $currentType }
        $classDepth = $braceDepth
        continue
    }

    if (![string]::IsNullOrWhiteSpace($currentType)) {
        if ($line -match "^\s*((?:public|private|protected|internal)\s+.*?);\s*//\s*RVA:\s*(0x[0-9A-Fa-f]+)") {
            $signature = $Matches[1].Trim()
            $rva = $Matches[2]

            if ($signature -match "^(?<before>.+?)\s+(?<name>[^\s\(]+)\((?<params>.*)\)$") {
                $tokens = $signature -split "\s+"
                $access = $tokens[0]
                $before = $Matches["before"].Trim()
                $name = $Matches["name"].Trim()
                $params = $Matches["params"].Trim()
                $runtimeMember = Normalize-MethodName $name $params
                if ([string]::IsNullOrWhiteSpace($access)) { $access = "unknown" }

                $key = "$assembly;$currentType;$runtimeMember;method"
                $runtimeOffset = if ($runtimeByKey.ContainsKey($key)) { $runtimeByKey[$key] } else { $rva }

                $rows.Add([pscustomobject]@{
                    Assembly = $assembly
                    Type = $currentType
                    Alias = $currentAlias
                    Parent = $currentParent
                    InstanceSize = $currentInstanceSize
                    Kind = "method"
                    Member = $name
                    RuntimeMember = $runtimeMember
                    Access = $access
                    IsStatic = $signature -match "\bstatic\b"
                    IsVirtual = $signature -match "\bvirtual\b|\boverride\b"
                    ReturnType = $before
                    FieldType = ""
                    Params = $params
                    OffsetHex = ""
                    RvaHex = $runtimeOffset
                    Signature = $signature
                })
            }
            continue
        }

        if ($line -match "^\s*((?:public|private|protected|internal|static|readonly|const)\s+.*?);\s*//\s*(0x[0-9A-Fa-f]+)") {
            $signature = $Matches[1].Trim()
            $offset = $Matches[2]
            if ($signature -notmatch "\(") {
                $member = Get-LastToken $signature
                $fieldType = $signature.Substring(0, $signature.Length - $member.Length).Trim()
                $access = ($signature -split "\s+")[0]
                $runtimeMember = $member
                $key = "$assembly;$currentType;$runtimeMember;field"
                $runtimeOffset = if ($runtimeByKey.ContainsKey($key)) { $runtimeByKey[$key] } else { $offset }

                $rows.Add([pscustomobject]@{
                    Assembly = $assembly
                    Type = $currentType
                    Alias = $currentAlias
                    Parent = $currentParent
                    InstanceSize = $currentInstanceSize
                    Kind = "field"
                    Member = $member
                    RuntimeMember = $runtimeMember
                    Access = $access
                    IsStatic = $signature -match "\bstatic\b"
                    IsVirtual = $false
                    ReturnType = ""
                    FieldType = $fieldType
                    Params = ""
                    OffsetHex = $runtimeOffset
                    RvaHex = ""
                    Signature = $signature
                })
            }
            continue
        }
    }

    $openCount = ([regex]::Matches($line, "\{")).Count
    $closeCount = ([regex]::Matches($line, "\}")).Count
    $braceDepth += $openCount - $closeCount

    if ($classDepth -ge 0 -and $braceDepth -le $classDepth) {
        $currentType = ""
        $currentAlias = ""
        $currentParent = ""
        $currentInstanceSize = ""
        $classDepth = -1
    }

    if ($closeCount -gt 0 -and $namespaceStack.Count -gt 0 -and $braceDepth -lt $namespaceStack.Count) {
        [void]$namespaceStack.Pop()
    }
}

Convert-ToCsvSafeRows $rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $OutputCsvPath

$targetLines = New-Object System.Collections.Generic.List[string]
$targetLines.Add("# Enriched IL2CPP targets")
$targetLines.Add("# Format: assembly;type_or_alias;member;kind")
$targetLines.Add("# Generated from dump.cs with signatures in CSV.")
$targetLines.Add("")

foreach ($row in $rows) {
    $typeOrAlias = if (![string]::IsNullOrWhiteSpace($row.Alias)) { $row.Alias } else { $row.Type }
    $targetLines.Add("$($row.Assembly);$typeOrAlias;$($row.RuntimeMember);$($row.Kind)")
}

[System.IO.File]::WriteAllLines($OutputTargetsPath, $targetLines)

Write-Host "Parsed rows: $($rows.Count)"
Write-Host "CSV: $OutputCsv"
Write-Host "Targets: $OutputTargets"
