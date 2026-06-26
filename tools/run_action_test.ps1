param(
    [string]$Command = "",
    [int]$WaitSeconds = 8,
    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\RussianFishing4",
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$TestLog = "",
    [int]$TargetPid = 0,
    [switch]$ScreenshotOnReview,
    [switch]$Summary
)

$ErrorActionPreference = "Stop"
$culture = [Globalization.CultureInfo]::InvariantCulture

if (-not $TestLog) {
    $TestLog = Join-Path $ProjectRoot "test_logs\action_tests.jsonl"
}

$testDir = Split-Path -Parent $TestLog
if (-not (Test-Path -LiteralPath $testDir)) {
    New-Item -ItemType Directory -Path $testDir | Out-Null
}

$latestText = Join-Path $testDir "action_tests_latest.txt"
$commandPath = Join-Path $GameDir "FishingCompanion_command.txt"
$actionLogPath = Join-Path $GameDir "FishingCompanion_actions.log"
$coordPath = Join-Path $GameDir "FishingCompanion_fishing_coords_v4.csv"
$coordinateTailColumns = @(
    "coordinate_quality",
    "lure_position_source",
    "fish_position_source",
    "has_fishing_set",
    "has_fisher",
    "has_rod",
    "has_reel",
    "has_lure",
    "has_lure_simple",
    "has_best_lure_pos",
    "has_closest_fish"
)

function Convert-JsonLine($object) {
    return ($object | ConvertTo-Json -Depth 8 -Compress)
}

function Get-CanonicalCommand([string]$value) {
    $text = ($value.Trim().ToLowerInvariant() -replace "[\s\.-]+", "_")
    switch ($text) {
        { $_ -in @("manualrollboost", "roll_boost") } { return "manual_roll_boost" }
        { $_ -in @("manualroll") } { return "manual_roll" }
        { $_ -in @("autocast") } { return "auto_cast" }
        { $_ -in @("autocatch") } { return "auto_catch" }
        { $_ -in @("autoscout", "scout_cast") } { return "auto_scout" }
        { $_ -in @("returntoidle") } { return "return_idle" }
        { $_ -in @("snapshot", "snapshot_diagnostics") } { return "snapshot_diagnostics" }
        default { return $text }
    }
}

function Get-EffectiveWaitSeconds([string]$canonical, [int]$requested) {
    $minimum = switch ($canonical) {
        "manual_roll_boost" { 10; break }
        "auto_cast" { 14; break }
        "auto_catch" { 16; break }
        "auto_scout" { 16; break }
        default { 0 }
    }

    return [Math]::Max($requested, $minimum)
}

function Read-LastCsvRow {
    if (-not (Test-Path -LiteralPath $coordPath)) {
        return $null
    }

    $header = Get-Content -LiteralPath $coordPath -TotalCount 1
    $last = Get-Content -LiteralPath $coordPath -Tail 1
    if (-not $header -or -not $last) {
        return $null
    }

    $headerColumns = @($header -split ",")
    $lastColumns = @($last -split ",")
    if ($lastColumns.Count -gt $headerColumns.Count) {
        $baseHeaderCount = $headerColumns.Count
        for ($i = $headerColumns.Count; $i -lt $lastColumns.Count; $i++) {
            $tailIndex = $i - $baseHeaderCount
            if ($tailIndex -lt $coordinateTailColumns.Count) {
                $columnName = $coordinateTailColumns[$tailIndex]
            }
            else {
                $columnName = "extra_$i"
            }
            if ($headerColumns -contains $columnName) {
                $columnName = "extra_$i"
            }
            $headerColumns += $columnName
        }
        $header = $headerColumns -join ","
    }

    return @($header, $last) | ConvertFrom-Csv
}

function Get-Number($row, [string]$name) {
    if (-not $row) {
        return 0.0
    }
    $raw = [string]$row.$name
    if ([string]::IsNullOrWhiteSpace($raw)) {
        return 0.0
    }
    return [double]::Parse($raw, $culture)
}

function Get-IntValue($row, [string]$name) {
    if (-not $row) {
        return 0
    }
    $raw = [string]$row.$name
    if ([string]::IsNullOrWhiteSpace($raw)) {
        return 0
    }
    return [int][double]::Parse($raw, $culture)
}

function Get-TextValue($row, [string]$name) {
    if (-not $row) {
        return ""
    }
    return [string]$row.$name
}

function Get-Point($row, [string]$prefix) {
    return @{
        x = Get-Number $row "${prefix}_x"
        y = Get-Number $row "${prefix}_y"
        z = Get-Number $row "${prefix}_z"
    }
}

function Get-Distance($a, $b) {
    $dx = [double]$a.x - [double]$b.x
    $dy = [double]$a.y - [double]$b.y
    $dz = [double]$a.z - [double]$b.z
    return [Math]::Sqrt(($dx * $dx) + ($dy * $dy) + ($dz * $dz))
}

function Send-FcCommand([string]$value, [int]$extraWaitMs) {
    Set-Content -LiteralPath $commandPath -Value $value -Encoding ASCII

    $deadline = (Get-Date).AddSeconds(5)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 100
    }

    if ($extraWaitMs -gt 0) {
        Start-Sleep -Milliseconds $extraWaitMs
    }
}

function Get-LogTail {
    if (-not (Test-Path -LiteralPath $actionLogPath)) {
        return @()
    }
    return @(Get-Content -LiteralPath $actionLogPath -Tail 160)
}

function Get-LogLength {
    if (-not (Test-Path -LiteralPath $actionLogPath)) {
        return 0
    }
    return (Get-Item -LiteralPath $actionLogPath).Length
}

function Get-NewLogText([long]$offset) {
    if (-not (Test-Path -LiteralPath $actionLogPath)) {
        return ""
    }

    $file = [System.IO.File]::Open($actionLogPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        if ($offset -gt $file.Length) {
            $offset = 0
        }
        $file.Seek($offset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $reader = New-Object System.IO.StreamReader($file)
        return $reader.ReadToEnd()
    }
    finally {
        $file.Dispose()
    }
}

function Get-FileSnapshot([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) {
        return [ordered]@{
            path = $path
            exists = $false
            length = 0
            last_write_time = ""
        }
    }

    $item = Get-Item -LiteralPath $path
    return [ordered]@{
        path = $path
        exists = $true
        length = $item.Length
        last_write_time = $item.LastWriteTime.ToString("o")
    }
}

function Save-Screenshot([string]$runId) {
    $process = $null
    if ($TargetPid -ne 0) {
        $process = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
    }
    if (-not $process) {
        $process = Get-Process -Name rf4_x64 -ErrorAction SilentlyContinue | Select-Object -First 1
    }
    if (-not $process -or $process.MainWindowHandle -eq [IntPtr]::Zero) {
        return $null
    }

    Add-Type -AssemblyName System.Drawing
    if (-not ("FcTestShot.Native" -as [type])) {
        Add-Type -Namespace FcTestShot -Name Native -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool SetForegroundWindow(System.IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool GetWindowRect(System.IntPtr hWnd, out RECT rect);
public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
"@
    }

    [FcTestShot.Native]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 300
    $rect = New-Object FcTestShot.Native+RECT
    [FcTestShot.Native]::GetWindowRect($process.MainWindowHandle, [ref]$rect) | Out-Null
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        return $null
    }

    $shotDir = Join-Path $testDir "screens"
    if (-not (Test-Path -LiteralPath $shotDir)) {
        New-Item -ItemType Directory -Path $shotDir | Out-Null
    }

    $path = Join-Path $shotDir "$runId.png"
    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, [System.Drawing.Size]::new($width, $height))
        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        return $path
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-Summary {
    if (-not (Test-Path -LiteralPath $TestLog)) {
        "No test log yet: $TestLog"
        return
    }

    $items = Get-Content -LiteralPath $TestLog | Where-Object { $_.Trim() } | ForEach-Object { $_ | ConvertFrom-Json }
    $count = @($items).Count
    if ($count -eq 0) {
        "No test entries yet: $TestLog"
        return
    }

    $groups = $items | Group-Object verdict | Sort-Object Name
    "Test log: $TestLog"
    "Total: $count"
    foreach ($group in $groups) {
        "{0}: {1}" -f $group.Name, $group.Count
    }
    "Last:"
    $items | Select-Object -Last 8 | ForEach-Object {
        "{0} | {1} | {2} | distDelta={3} lureDelta={4} reelDelta={5} fish {6}->{7}" -f `
            $_.time, $_.command, $_.verdict, $_.metrics.distance_delta, $_.metrics.lure_delta, $_.metrics.reel_delta, $_.before.fish_count, $_.after.fish_count
    }
}

if ($Summary) {
    Get-Summary
    return
}

if ([string]::IsNullOrWhiteSpace($Command)) {
    throw "Pass -Command or use -Summary."
}

if (-not (Test-Path -LiteralPath $GameDir)) {
    throw "GameDir was not found: $GameDir"
}

$canonical = Get-CanonicalCommand $Command
$effectiveWaitSeconds = Get-EffectiveWaitSeconds $canonical $WaitSeconds
$runId = "{0}_{1}" -f (Get-Date -Format "yyyyMMdd_HHmmss_fff"), $canonical

Send-FcCommand "snapshot" 700
$before = Read-LastCsvRow
$logOffset = Get-LogLength

Send-FcCommand $Command 0
Start-Sleep -Seconds $effectiveWaitSeconds
Send-FcCommand "snapshot" 900

$after = Read-LastCsvRow
$newLog = Get-NewLogText $logOffset
$logLines = @($newLog -split "`r?`n" | Where-Object { $_.Trim() })
$coordSnapshot = Get-FileSnapshot $coordPath
$actionLogSnapshot = Get-FileSnapshot $actionLogPath

$beforeLure = Get-Point $before "best_lure"
$afterLure = Get-Point $after "best_lure"
$lureDelta = Get-Distance $beforeLure $afterLure
$distanceDelta = (Get-Number $before "fisher_to_lure") - (Get-Number $after "fisher_to_lure")
$reelDelta = (Get-Number $before "reel_value") - (Get-Number $after "reel_value")
$fishBefore = Get-IntValue $before "fish_count"
$fishAfter = Get-IntValue $after "fish_count"
$flagsBefore = Get-TextValue $before "fishing_set_0x160"
$flagsAfter = Get-TextValue $after "fishing_set_0x160"
$logicalBefore = Get-TextValue $before "logical_fish_lure"
$logicalAfter = Get-TextValue $after "logical_fish_lure"
$closestBefore = Get-TextValue $before "closest_fish"
$closestAfter = Get-TextValue $after "closest_fish"
$qualityBefore = Get-TextValue $before "coordinate_quality"
$qualityAfter = Get-TextValue $after "coordinate_quality"
$lureSourceBefore = Get-TextValue $before "lure_position_source"
$lureSourceAfter = Get-TextValue $after "lure_position_source"
$fishSourceBefore = Get-TextValue $before "fish_position_source"
$fishSourceAfter = Get-TextValue $after "fish_position_source"
$beforeTick = Get-TextValue $before "tick_ms"
$afterTick = Get-TextValue $after "tick_ms"
$tickChanged = $beforeTick -ne $afterTick

$confirmedByLog = $logLines | Where-Object { $_ -match "^$([regex]::Escape($canonical)):\s+confirmed" } | Select-Object -Last 1
$failedByLog = $logLines | Where-Object { $_ -match "^$([regex]::Escape($canonical)):\s+failed" } | Select-Object -Last 1
$calledByLog = $logLines | Where-Object { $_ -match "^$([regex]::Escape($canonical)):\s+called" } | Select-Object -Last 1
$observedByLog = $logLines | Where-Object { $_ -match "^$([regex]::Escape($canonical)) observed:" } | Select-Object -Last 1
$snapshotQualityByLog = $logLines | Where-Object { $_ -match "^snapshot_quality\[" } | Select-Object -Last 1
$lureCandidatesByLog = $logLines | Where-Object { $_ -match "^lure_vector_candidates\[" } | Select-Object -Last 1
$fishCandidatesByLog = $logLines | Where-Object { $_ -match "^fish_vector_candidates\[" } | Select-Object -Last 1
$fishScanByLog = $logLines | Where-Object { $_ -match "^fish_scan:" } | Select-Object -Last 1

$stateChanged =
    ([Math]::Abs($distanceDelta) -ge 0.05) -or
    ([Math]::Abs($lureDelta) -ge 0.05) -or
    ([Math]::Abs($reelDelta) -ge 0.05) -or
    ($flagsBefore -ne $flagsAfter) -or
    ($logicalBefore -ne $logicalAfter) -or
    ($closestBefore -ne $closestAfter) -or
    ($fishBefore -ne $fishAfter)

$fishResolved =
    ($fishBefore -gt 0 -and $fishAfter -lt $fishBefore) -or
    ($logicalBefore -ne "0x0000000000000000" -and $logicalAfter -eq "0x0000000000000000") -or
    ($flagsBefore -ne $flagsAfter -and $flagsAfter -match "0800$")

$verdict = "review"
$reason = "no decisive signal"

if ($failedByLog) {
    $verdict = "fail"
    $reason = "runtime reported failure"
}
elseif ($confirmedByLog) {
    $verdict = "pass"
    $reason = "runtime reported confirmed"
}
elseif ($canonical -eq "auto_cast" -and $lureDelta -ge 1.0) {
    $verdict = "pass"
    $reason = "lure moved after cast"
}
elseif ($canonical -eq "snapshot_diagnostics" -and $tickChanged) {
    $verdict = "pass"
    $reason = "snapshot updated coordinate CSV"
}
elseif ($canonical -eq "manual_roll_boost" -and $fishResolved) {
    $verdict = "pass"
    $reason = "fish-fight state resolved or reset"
}
elseif ($stateChanged) {
    $verdict = "review"
    $reason = "state changed but expected effect is ambiguous"
}
elseif ($calledByLog) {
    $verdict = "review"
    $reason = "command was called but sensors barely changed"
}
else {
    $verdict = "fail"
    $reason = "no runtime response for command"
}

$screenshotPath = $null
if ($ScreenshotOnReview -and $verdict -ne "pass") {
    $screenshotPath = Save-Screenshot $runId
}

$resultLine = $confirmedByLog
if (-not $resultLine) {
    $resultLine = $failedByLog
}
if (-not $resultLine) {
    $resultLine = $calledByLog
}

$record = [ordered]@{
    run_id = $runId
    time = (Get-Date).ToString("o")
    command = $canonical
    original_command = $Command
    wait_seconds = $effectiveWaitSeconds
    requested_wait_seconds = $WaitSeconds
    verdict = $verdict
    reason = $reason
    screenshot = $screenshotPath
    diagnostics = [ordered]@{
        command_path = $commandPath
        coordinate_csv = $coordSnapshot
        action_log = $actionLogSnapshot
        has_before_row = $null -ne $before
        has_after_row = $null -ne $after
        tick_changed = $tickChanged
        new_log_line_count = @($logLines).Count
    }
    before = [ordered]@{
        tick_ms = $beforeTick
        reason = Get-TextValue $before "reason"
        fish_count = $fishBefore
        fisher_to_lure = Get-Number $before "fisher_to_lure"
        reel_value = Get-Number $before "reel_value"
        flags_0x160 = $flagsBefore
        logical_fish_lure = $logicalBefore
        closest_fish = $closestBefore
        coordinate_quality = $qualityBefore
        lure_position_source = $lureSourceBefore
        fish_position_source = $fishSourceBefore
        has_best_lure_pos = Get-IntValue $before "has_best_lure_pos"
        has_closest_fish = Get-IntValue $before "has_closest_fish"
        best_lure = $beforeLure
    }
    after = [ordered]@{
        tick_ms = $afterTick
        reason = Get-TextValue $after "reason"
        fish_count = $fishAfter
        fisher_to_lure = Get-Number $after "fisher_to_lure"
        reel_value = Get-Number $after "reel_value"
        flags_0x160 = $flagsAfter
        logical_fish_lure = $logicalAfter
        closest_fish = $closestAfter
        coordinate_quality = $qualityAfter
        lure_position_source = $lureSourceAfter
        fish_position_source = $fishSourceAfter
        has_best_lure_pos = Get-IntValue $after "has_best_lure_pos"
        has_closest_fish = Get-IntValue $after "has_closest_fish"
        best_lure = $afterLure
    }
    metrics = [ordered]@{
        distance_delta = [Math]::Round($distanceDelta, 4)
        lure_delta = [Math]::Round($lureDelta, 4)
        reel_delta = [Math]::Round($reelDelta, 4)
        state_changed = $stateChanged
        fish_resolved = $fishResolved
    }
    runtime = [ordered]@{
        result_line = [string]$resultLine
        observed_line = [string]$observedByLog
        snapshot_quality_line = [string]$snapshotQualityByLog
        lure_candidates_line = [string]$lureCandidatesByLog
        fish_candidates_line = [string]$fishCandidatesByLog
        fish_scan_line = [string]$fishScanByLog
        new_log_tail = @($logLines | Select-Object -Last 20)
    }
}

Convert-JsonLine $record | Add-Content -LiteralPath $TestLog -Encoding UTF8

$latest = @(
    "run_id: $runId"
    "time: $($record.time)"
    "command: $canonical"
    "verdict: $verdict"
    "reason: $reason"
    "wait_seconds: $effectiveWaitSeconds requested=$WaitSeconds"
    "tick_ms: $beforeTick -> $afterTick"
    "distance_delta: $($record.metrics.distance_delta)"
    "lure_delta: $($record.metrics.lure_delta)"
    "reel_delta: $($record.metrics.reel_delta)"
    "fish: $fishBefore -> $fishAfter"
    "flags_0x160: $flagsBefore -> $flagsAfter"
    "quality_csv: $qualityBefore/$lureSourceBefore/$fishSourceBefore -> $qualityAfter/$lureSourceAfter/$fishSourceAfter"
    "coord_csv: exists=$($record.diagnostics.coordinate_csv.exists) bytes=$($record.diagnostics.coordinate_csv.length) updated=$($record.diagnostics.coordinate_csv.last_write_time)"
    "action_log: exists=$($record.diagnostics.action_log.exists) bytes=$($record.diagnostics.action_log.length) new_lines=$($record.diagnostics.new_log_line_count)"
    "quality: $($record.runtime.snapshot_quality_line)"
    "lure_candidates: $($record.runtime.lure_candidates_line)"
    "fish_candidates: $($record.runtime.fish_candidates_line)"
    "fish_scan: $($record.runtime.fish_scan_line)"
    "runtime: $($record.runtime.result_line)"
    "observed: $($record.runtime.observed_line)"
    "jsonl: $TestLog"
)
$lastLogLine = $logLines | Select-Object -Last 1
if ($lastLogLine) {
    $latest += "log_tail_last: $lastLogLine"
}
if ($screenshotPath) {
    $latest += "screenshot: $screenshotPath"
}
$latest | Set-Content -LiteralPath $latestText -Encoding UTF8

$latest
