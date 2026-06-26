param(
    [Parameter(Mandatory = $true)]
    [string]$Offsets,

    [Parameter(Mandatory = $false)]
    [string]$CatalogOutput = "generated\interaction_surface.md",

    [Parameter(Mandatory = $false)]
    [string]$TargetsOutput = "generated\interaction_targets.txt",

    [Parameter(Mandatory = $false)]
    [string[]]$IncludeAssembly = @("Assembly-CSharp.dll"),

    [Parameter(Mandatory = $false)]
    [string[]]$IncludeTypePrefix = @("RF4.", "UltimateWater.", "WaveHarmonic.", "Crest."),

    [Parameter(Mandatory = $false)]
    [int]$MaxMembersPerClass = 80
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

function Test-ObfuscatedName {
    param([string]$Name)
    if ([string]::IsNullOrWhiteSpace($Name)) {
        return $true
    }
    $simple = ($Name -split "\.")[-1]
    return $simple -match "^[a-z]{8,18}$"
}

function Get-Category {
    param([string]$Type)

    $rules = [ordered]@{
        "Fishing" = "FishingScene|(^|[._])Fishing($|[._])|(^|[._])Fisher($|[._])|(^|[._])Fish($|[._])|(^|[._])Rod($|[._])|(^|[._])Reel($|[._])|(^|[._])Rig($|[._])|(^|[._])Lure($|[._])|(^|[._])Hook($|[._])|(^|[._])Bait($|[._])|Bobber|Sonar|Tackle|FishingSet"
        "InventoryItems" = "Inventory|(^|[._])Item($|[._])|Items|Asset|Storage|Belongings|Shop|Store|Cafe|Exchange|Repair|Craft"
        "UI" = "\.UI\.|RUI|HUD|Widget|Panel|Table|Cell|Dialog|Tooltip|Button|Toolbar|Explorer"
        "WorldWaterWeather" = "Water|Weather|Atmosphere|Sky|Rain|Cloud|Wind|Wave|Crest"
        "PlayerInput" = "Player|Input|Controller|Camera|FisherFPS|UniversalRigUserInput"
        "Transport" = "Boat|Car|Quad|Paddle"
        "AnimalsWorld" = "Bird|Bear|Duck|Cat|Cow|Seagull|Heron|Eagle|Insect"
        "ProgressStats" = "Skill|Ability|Statistics|History|Rating|Result|Progress|Trophy"
        "Systems" = "Manager|Controller|Loader|Settings|Quality|Orchestrator|Domain"
    }

    foreach ($entry in $rules.GetEnumerator()) {
        if ($Type -match $entry.Value) {
            return $entry.Key
        }
    }

    return "Other"
}

function Get-MemberScore {
    param($Row)

    $member = [string]$Row.member
    $kind = [string]$Row.kind

    if ($kind -eq "field") {
        if ($member -match "^[a-z]{8,18}$") { return 20 }
        if ($member -match "^<.*>k__BackingField$") { return 35 }
        return 70
    }

    if ($member -match "^(Awake|Start|Update|FixedUpdate|LateUpdate|OnEnable|OnDisable|OnDestroy)\(0\)$") { return 100 }
    if ($member -match "^(get_|set_)") { return 90 }
    if ($member -match "^(Initialize|Init|Open|Close|Show|Hide|Enable|Disable|Refresh|Reset|Create|Destroy|Apply|Set|Get)") { return 80 }
    if ($member -match "^[a-z]{8,18}\([0-4]\)$") { return 30 }
    return 55
}

function Convert-ToTargetLine {
    param($Row)
    $typeOrAlias = if (![string]::IsNullOrWhiteSpace($Row.alias)) { $Row.alias } else { $Row.type }
    return "$($Row.assembly);$typeOrAlias;$($Row.member);$($Row.kind)"
}

if (!(Test-Path -LiteralPath $Offsets)) {
    throw "Offsets file was not found: $Offsets"
}

$CatalogOutputPath = Resolve-ProjectPath $CatalogOutput
$TargetsOutputPath = Resolve-ProjectPath $TargetsOutput
$rows = Import-Csv -Delimiter ';' -LiteralPath $Offsets

$filtered = foreach ($row in $rows) {
    if ($IncludeAssembly.Count -gt 0 -and $IncludeAssembly -notcontains $row.assembly) {
        continue
    }

    $typeOrAlias = if (![string]::IsNullOrWhiteSpace($row.alias)) { $row.alias } else { $row.type }
    $prefixOk = $false
    foreach ($prefix in $IncludeTypePrefix) {
        if ($typeOrAlias.StartsWith($prefix) -or $row.type.StartsWith($prefix)) {
            $prefixOk = $true
            break
        }
    }

    if (!$prefixOk -and (Test-ObfuscatedName $typeOrAlias)) {
        continue
    }

    $score = Get-MemberScore $row
    [pscustomobject]@{
        Assembly = $row.assembly
        Type = $row.type
        Alias = $row.alias
        DisplayType = $typeOrAlias
        Member = $row.member
        Kind = $row.kind
        Offset = $row.offset_hex
        Category = Get-Category $typeOrAlias
        Score = $score
        TargetLine = Convert-ToTargetLine $row
    }
}

$groups = $filtered |
    Group-Object Assembly, Type, Alias |
    Sort-Object @{ Expression = { ($_.Group | Measure-Object Score -Maximum).Maximum }; Descending = $true },
                @{ Expression = { $_.Count }; Descending = $true }

$catalogDir = Split-Path -Parent $CatalogOutputPath
if (![string]::IsNullOrWhiteSpace($catalogDir) -and !(Test-Path -LiteralPath $catalogDir)) {
    New-Item -ItemType Directory -Path $catalogDir | Out-Null
}

$targetsDir = Split-Path -Parent $TargetsOutputPath
if (![string]::IsNullOrWhiteSpace($targetsDir) -and !(Test-Path -LiteralPath $targetsDir)) {
    New-Item -ItemType Directory -Path $targetsDir | Out-Null
}

$catalog = New-Object System.Collections.Generic.List[string]
$targets = New-Object System.Collections.Generic.List[string]
$seenTargets = New-Object "System.Collections.Generic.HashSet[string]"

$catalog.Add("# IL2CPP Interaction Surface")
$catalog.Add("")
$catalog.Add("Generated from: $Offsets")
$catalog.Add("")
$catalog.Add('This is a broad catalog of classes and members that are useful for authorized runtime inspection. Copy target lines into il2cpp_targets.txt, inject the dumper, then regenerate the C++ offsets header.')
$catalog.Add("")

$categoryStats = $filtered | Group-Object Category | Sort-Object Count -Descending
$catalog.Add("## Category Counts")
$catalog.Add("")
foreach ($stat in $categoryStats) {
    $catalog.Add("- $($stat.Name): $($stat.Count)")
}
$catalog.Add("")

$targets.Add("# Interaction targets generated from $Offsets")
$targets.Add("# Format: assembly;type_or_alias;member;kind")
$targets.Add("# Start smaller while testing: copy a section into the game folder as il2cpp_targets.txt.")
$targets.Add("")

foreach ($group in $groups) {
    $items = @($group.Group)
    if ($items.Count -eq 0) {
        continue
    }

    $first = $items[0]
    $displayType = $first.DisplayType
    $category = $first.Category
    $members = $items |
        Sort-Object @{ Expression = "Score"; Descending = $true }, Kind, Member |
        Select-Object -First $MaxMembersPerClass

    $catalog.Add("## $category / $displayType")
    if ($displayType -ne $first.Type) {
        $catalog.Add("")
        $catalog.Add("Runtime type: $($first.Type)")
    }
    $catalog.Add("")
    $catalog.Add('```text')

    $targets.Add("# $category / $displayType")
    foreach ($member in $members) {
        $line = $member.TargetLine
        $catalog.Add($line)
        [void]$seenTargets.Add($line)
        $targets.Add($line)
    }
    $catalog.Add('```')
    $catalog.Add("")
    $targets.Add("")
}

[System.IO.File]::WriteAllLines($CatalogOutputPath, $catalog)
[System.IO.File]::WriteAllLines($TargetsOutputPath, $targets)

$presetDir = Join-Path (Split-Path -Parent $TargetsOutputPath) "interaction_presets"
if (!(Test-Path -LiteralPath $presetDir)) {
    New-Item -ItemType Directory -Path $presetDir | Out-Null
}

foreach ($category in ($filtered | Select-Object -ExpandProperty Category -Unique | Sort-Object)) {
    $presetLines = New-Object System.Collections.Generic.List[string]
    $presetLines.Add("# $category targets")
    $presetLines.Add("# Copy this file next to the game exe as il2cpp_targets.txt")
    $presetLines.Add("")

    $categoryRows = $filtered |
        Where-Object { $_.Category -eq $category } |
        Sort-Object DisplayType, @{ Expression = "Score"; Descending = $true }, Kind, Member

    foreach ($row in $categoryRows) {
        $presetLines.Add($row.TargetLine)
    }

    $presetPath = Join-Path $presetDir "$category.txt"
    [System.IO.File]::WriteAllLines($presetPath, $presetLines)
}

Write-Host "Catalog: $CatalogOutputPath"
Write-Host "Targets: $TargetsOutputPath"
Write-Host "Presets: $presetDir"
Write-Host "Classes: $(@($groups).Count)"
Write-Host "Targets: $($seenTargets.Count)"
