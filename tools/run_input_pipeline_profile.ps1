[CmdletBinding()]
param(
    [string]$ExePath,
    [ValidateRange(10, 180)]
    [int]$PhaseSeconds = 30,
    [ValidateRange(3, 30)]
    [int]$BrowserWarmupSeconds = 6,
    [ValidateRange(1, 65535)]
    [int]$OverlayPort = 0,
    [string]$BrowserPath,
    [string]$EvidenceRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $root 'build\output\HallJoy.exe'
}
$ExePath = [IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "HallJoy executable was not found: $ExePath"
}
if ([IO.Path]::GetFileName($ExePath) -ne 'HallJoy.exe') {
    throw "Input pipeline profiling requires HallJoy.exe: $ExePath"
}
if (@(Get-Process -Name HallJoy -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'Refusing to profile while HallJoy is already running.'
}

$output = Split-Path -Parent $ExePath
$tracePath = Join-Path $output 'HallJoyStabilityTrace.log'
$overlayPerfPath = Join-Path $output 'overlay_perf.log'
$settingsPath = Join-Path $env:LOCALAPPDATA 'HallJoy\settings.ini'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $root "build\evidence\input-pipeline-profile\$stamp"
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null

function Get-StateManifest {
    $stateRoot = Join-Path $env:LOCALAPPDATA 'HallJoy'
    $entries = @()
    if (Test-Path -LiteralPath $stateRoot -PathType Container) {
        $entries = @(Get-ChildItem -LiteralPath $stateRoot -File -Recurse |
            Sort-Object FullName | ForEach-Object {
                [pscustomobject]@{
                    file = $_.FullName.Substring($stateRoot.Length).TrimStart('\')
                    sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
                }
            })
    }
    return $entries
}

function Get-ConfiguredOverlayPort {
    if ($OverlayPort -ne 0) {
        return $OverlayPort
    }
    if (-not (Test-Path -LiteralPath $settingsPath -PathType Leaf)) {
        return 8765
    }
    $inOverlay = $false
    foreach ($line in Get-Content -LiteralPath $settingsPath) {
        if ($line -match '^\[(.+)\]$') {
            $inOverlay = $Matches[1] -eq 'InputOverlay'
            continue
        }
        if ($inOverlay -and $line -match '^Port=(\d+)$') {
            $value = [int]$Matches[1]
            if ($value -ge 1 -and $value -le 65535) {
                return $value
            }
        }
    }
    return 8765
}

function Find-Browser {
    if (-not [string]::IsNullOrWhiteSpace($BrowserPath)) {
        $candidate = [IO.Path]::GetFullPath($BrowserPath)
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            throw "Requested browser was not found: $candidate"
        }
        return $candidate
    }
    $candidates = @(
        'C:\Program Files\Google\Chrome\Application\chrome.exe',
        'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
        'C:\Program Files\Microsoft\Edge\Application\msedge.exe'
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    throw 'Chrome or Edge was not found for the real overlay render phases.'
}

if (-not ('HallJoyPipelineProfileNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class HallJoyPipelineProfileNative {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)]
    public struct FILETIME { public uint Low; public uint High; }
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("kernel32.dll")]
    public static extern bool GetSystemTimes(out FILETIME idle, out FILETIME kernel, out FILETIME user);
    public static ulong Value(FILETIME value) { return ((ulong)value.High << 32) | value.Low; }
    public static int PostClose(uint targetProcessId) {
        int sent = 0;
        EnumWindows((window, unused) => {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId == targetProcessId && PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero))
                ++sent;
            return true;
        }, IntPtr.Zero);
        return sent;
    }
}
'@
}

function Get-SystemTimesSnapshot {
    $idle = New-Object HallJoyPipelineProfileNative+FILETIME
    $kernel = New-Object HallJoyPipelineProfileNative+FILETIME
    $user = New-Object HallJoyPipelineProfileNative+FILETIME
    if (-not [HallJoyPipelineProfileNative]::GetSystemTimes([ref]$idle, [ref]$kernel, [ref]$user)) {
        throw 'GetSystemTimes failed.'
    }
    return [pscustomobject]@{
        idle = [HallJoyPipelineProfileNative]::Value($idle)
        kernel = [HallJoyPipelineProfileNative]::Value($kernel)
        user = [HallJoyPipelineProfileNative]::Value($user)
    }
}

function Get-ProcessRows {
    param([int[]]$Ids)
    $rows = @()
    foreach ($id in @($Ids | Sort-Object -Unique)) {
        $process = Get-Process -Id $id -ErrorAction SilentlyContinue
        if (-not $process) { continue }
        try {
            $process.Refresh()
            $rows += [pscustomobject]@{
                id = $id
                cpu_seconds = $process.TotalProcessorTime.TotalSeconds
                working_set_bytes = [long]$process.WorkingSet64
                private_bytes = [long]$process.PrivateMemorySize64
                handles = [int]$process.HandleCount
                threads = [int]$process.Threads.Count
            }
        }
        catch {
            # A child may exit between enumeration and sampling.
        }
    }
    return $rows
}

function Get-HallJoyProcessInfo {
    param([int]$MainId)
    $rows = @()
    foreach ($item in @(Get-CimInstance Win32_Process -Filter "Name='HallJoy.exe'")) {
        $role = if ([int]$item.ProcessId -eq $MainId) {
            'main'
        } elseif ($item.CommandLine -match '--halljoy-analog-host') {
            'uap-host'
        } elseif ($item.CommandLine -match '--diagnostic-watch') {
            'diagnostic-watch'
        } else {
            'other-halljoy'
        }
        $rows += [pscustomobject]@{ id = [int]$item.ProcessId; role = $role }
    }
    return $rows
}

function Get-ThreadLabels {
    param([int]$MainId)
    $labels = @{}
    if (-not (Test-Path -LiteralPath $tracePath -PathType Leaf)) {
        return $labels
    }
    foreach ($line in Get-Content -LiteralPath $tracePath) {
        if ($line -notmatch '\[pid=(\d+)\]\[tid=(\d+)\].*\[component=([^\]]+)\]\[event=([^\]]+)\](.*)$') {
            continue
        }
        if ([int]$Matches[1] -ne $MainId) { continue }
        $tid = [int]$Matches[2]
        $component = $Matches[3]
        $event = $Matches[4]
        $details = $Matches[5]
        if ($event -eq 'session.start') {
            $labels[$tid] = 'ui-main'
        } elseif ($event -eq 'worker.start') {
            $label = $component
            if ($component -eq 'analog-host' -and $details -match 'worker=([^\s]+)') {
                $label = "analog-host-$($Matches[1])"
            }
            $labels[$tid] = $label
        }
    }
    return $labels
}

function Get-MainThreadRows {
    param([int]$MainId, [hashtable]$Labels)
    $rows = @()
    $process = Get-Process -Id $MainId -ErrorAction SilentlyContinue
    if (-not $process) { return $rows }
    $process.Refresh()
    foreach ($thread in $process.Threads) {
        try {
            $label = if ($Labels.ContainsKey([int]$thread.Id)) { $Labels[[int]$thread.Id] } else { 'auxiliary' }
            $rows += [pscustomobject]@{
                id = [int]$thread.Id
                label = $label
                cpu_seconds = $thread.TotalProcessorTime.TotalSeconds
            }
        }
        catch {
            # A short-lived client worker may disappear during the snapshot.
        }
    }
    return $rows
}

function Get-DescendantIds {
    param([int]$RootId, [string]$ProfileDirectory)
    $all = @(Get-CimInstance Win32_Process)
    $known = @{$RootId = $true}
    $changed = $true
    while ($changed) {
        $changed = $false
        foreach ($item in $all) {
            if ($known.ContainsKey([int]$item.ParentProcessId) -and -not $known.ContainsKey([int]$item.ProcessId)) {
                $known[[int]$item.ProcessId] = $true
                $changed = $true
            }
        }
    }
    foreach ($item in $all) {
        if ($item.Name -match '^(chrome|msedge)\.exe$' -and
            $item.CommandLine -and $item.CommandLine.Contains($ProfileDirectory)) {
            $known[[int]$item.ProcessId] = $true
        }
    }
    return @($known.Keys | ForEach-Object { [int]$_ })
}

function Start-ProfileBrowser {
    param([string]$Url, [string]$Name)
    $profileDirectory = Join-Path $EvidenceRoot "browser-$Name"
    New-Item -ItemType Directory -Path $profileDirectory -Force | Out-Null
    $arguments = @(
        '--headless=new',
        "--user-data-dir=$profileDirectory",
        '--no-first-run',
        '--disable-default-apps',
        '--disable-extensions',
        '--disable-background-timer-throttling',
        '--disable-renderer-backgrounding',
        '--disable-backgrounding-occluded-windows',
        '--window-size=1280,720',
        $Url
    )
    $process = Start-Process -FilePath $script:browserExecutable -ArgumentList $arguments `
        -WindowStyle Hidden -PassThru
    return [pscustomobject]@{ process = $process; profile = $profileDirectory; name = $Name }
}

function Stop-ProfileBrowser {
    param($Browser)
    $ids = @(Get-DescendantIds -RootId $Browser.process.Id -ProfileDirectory $Browser.profile)
    foreach ($id in @($ids | Sort-Object -Descending)) {
        $process = Get-Process -Id $id -ErrorAction SilentlyContinue
        if ($process -and $process.ProcessName -match '^(chrome|msedge)$') {
            Stop-Process -Id $id -Force -ErrorAction SilentlyContinue
        }
    }
    Start-Sleep -Milliseconds 500
    $remaining = @(Get-DescendantIds -RootId $Browser.process.Id -ProfileDirectory $Browser.profile |
        Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue })
    if ($remaining.Count -ne 0) {
        throw "Profile browser '$($Browser.name)' left process IDs: $($remaining -join ',')"
    }
    $resolvedEvidence = [IO.Path]::GetFullPath($EvidenceRoot).TrimEnd('\') + '\'
    $resolvedProfile = [IO.Path]::GetFullPath($Browser.profile)
    if (-not $resolvedProfile.StartsWith($resolvedEvidence, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolvedProfile) -notlike 'browser-*') {
        throw "Refusing to remove unexpected browser profile path: $resolvedProfile"
    }
    if (Test-Path -LiteralPath $resolvedProfile -PathType Container) {
        Remove-Item -LiteralPath $resolvedProfile -Recurse -Force
    }
}

function Get-CpuDeltaByRole {
    param($StartRows, $EndRows, $ProcessInfo, [double]$WallSeconds, [int]$LogicalProcessors)
    $startById = @{}
    foreach ($row in $StartRows) { $startById[[int]$row.id] = [double]$row.cpu_seconds }
    $roleById = @{}
    foreach ($row in $ProcessInfo) { $roleById[[int]$row.id] = $row.role }
    $deltas = @{}
    foreach ($row in $EndRows) {
        $id = [int]$row.id
        $start = if ($startById.ContainsKey($id)) { $startById[$id] } else { 0.0 }
        $delta = [Math]::Max(0.0, [double]$row.cpu_seconds - $start)
        $role = if ($roleById.ContainsKey($id)) { $roleById[$id] } else { 'other' }
        if (-not $deltas.ContainsKey($role)) { $deltas[$role] = 0.0 }
        $deltas[$role] += $delta
    }
    return @($deltas.Keys | Sort-Object | ForEach-Object {
        [pscustomobject]@{
            role = $_
            cpu_seconds = [Math]::Round($deltas[$_], 6)
            one_core_percent = [Math]::Round(100.0 * $deltas[$_] / $WallSeconds, 3)
            machine_percent = [Math]::Round(100.0 * $deltas[$_] / $WallSeconds / $LogicalProcessors, 3)
        }
    })
}

function Get-ThreadCpuDelta {
    param($StartRows, $EndRows, [double]$WallSeconds, [int]$LogicalProcessors)
    $startById = @{}
    foreach ($row in $StartRows) { $startById[[int]$row.id] = [double]$row.cpu_seconds }
    $totals = @{}
    foreach ($row in $EndRows) {
        $id = [int]$row.id
        $start = if ($startById.ContainsKey($id)) { $startById[$id] } else { 0.0 }
        $delta = [Math]::Max(0.0, [double]$row.cpu_seconds - $start)
        if (-not $totals.ContainsKey($row.label)) { $totals[$row.label] = 0.0 }
        $totals[$row.label] += $delta
    }
    return @($totals.Keys | Sort-Object | ForEach-Object {
        [pscustomobject]@{
            stage = $_
            cpu_seconds = [Math]::Round($totals[$_], 6)
            one_core_percent = [Math]::Round(100.0 * $totals[$_] / $WallSeconds, 3)
            machine_percent = [Math]::Round(100.0 * $totals[$_] / $WallSeconds / $LogicalProcessors, 3)
        }
    })
}

function Get-NumericPropertySum {
    param($Rows, [string]$Property)
    [double]$total = 0.0
    foreach ($row in @($Rows)) {
        if ($null -ne $row -and $null -ne $row.PSObject.Properties[$Property]) {
            $total += [double]$row.$Property
        }
    }
    return $total
}

function Measure-ProfilePhase {
    param([string]$Name, $Browser)
    $labels = Get-ThreadLabels -MainId $script:hallJoy.Id
    $hallInfo = @(Get-HallJoyProcessInfo -MainId $script:hallJoy.Id)
    $hallIds = @($hallInfo | ForEach-Object { [int]$_.id })
    $browserIds = if ($Browser) {
        @(Get-DescendantIds -RootId $Browser.process.Id -ProfileDirectory $Browser.profile)
    } else { @() }

    $started = Get-Date
    $systemStart = Get-SystemTimesSnapshot
    $hallStart = @(Get-ProcessRows -Ids $hallIds)
    $threadStart = @(Get-MainThreadRows -MainId $script:hallJoy.Id -Labels $labels)
    $browserStart = @(Get-ProcessRows -Ids $browserIds)
    $maxHallWorking = 0L
    $maxHallPrivate = 0L
    $maxHallHandles = 0
    $maxHallThreads = 0
    $maxBrowserWorking = 0L
    $maxBrowserPrivate = 0L

    $phaseDeadline = $started.AddSeconds($PhaseSeconds)
    while ((Get-Date) -lt $phaseDeadline) {
        $sleepMilliseconds = [int][Math]::Min(1000.0,
            [Math]::Max(1.0, ($phaseDeadline - (Get-Date)).TotalMilliseconds))
        Start-Sleep -Milliseconds $sleepMilliseconds
        $currentHallInfo = @(Get-HallJoyProcessInfo -MainId $script:hallJoy.Id)
        $currentHall = @(Get-ProcessRows -Ids @($currentHallInfo | ForEach-Object { [int]$_.id }))
        $maxHallWorking = [Math]::Max($maxHallWorking, [long](Get-NumericPropertySum $currentHall 'working_set_bytes'))
        $maxHallPrivate = [Math]::Max($maxHallPrivate, [long](Get-NumericPropertySum $currentHall 'private_bytes'))
        $maxHallHandles = [Math]::Max($maxHallHandles, [int](Get-NumericPropertySum $currentHall 'handles'))
        $maxHallThreads = [Math]::Max($maxHallThreads, [int](Get-NumericPropertySum $currentHall 'threads'))
        if ($Browser) {
            $browserIds = @(Get-DescendantIds -RootId $Browser.process.Id -ProfileDirectory $Browser.profile)
            $currentBrowser = @(Get-ProcessRows -Ids $browserIds)
            $maxBrowserWorking = [Math]::Max($maxBrowserWorking, [long](Get-NumericPropertySum $currentBrowser 'working_set_bytes'))
            $maxBrowserPrivate = [Math]::Max($maxBrowserPrivate, [long](Get-NumericPropertySum $currentBrowser 'private_bytes'))
        }
    }

    $ended = Get-Date
    $wallSeconds = [Math]::Max(0.001, ($ended - $started).TotalSeconds)
    $systemEnd = Get-SystemTimesSnapshot
    $hallEndInfo = @(Get-HallJoyProcessInfo -MainId $script:hallJoy.Id)
    $hallEnd = @(Get-ProcessRows -Ids @($hallEndInfo | ForEach-Object { [int]$_.id }))
    $threadEnd = @(Get-MainThreadRows -MainId $script:hallJoy.Id -Labels $labels)
    $browserEnd = if ($Browser) {
        @(Get-ProcessRows -Ids @(Get-DescendantIds -RootId $Browser.process.Id -ProfileDirectory $Browser.profile))
    } else { @() }

    $idleDelta = [double]($systemEnd.idle - $systemStart.idle)
    $kernelDelta = [double]($systemEnd.kernel - $systemStart.kernel)
    $userDelta = [double]($systemEnd.user - $systemStart.user)
    $totalDelta = $kernelDelta + $userDelta
    $systemBusy = if ($totalDelta -gt 0) {
        [Math]::Round(100.0 * ($totalDelta - $idleDelta) / $totalDelta, 3)
    } else { 0.0 }

    $browserStartCpu = Get-NumericPropertySum $browserStart 'cpu_seconds'
    $browserEndCpu = Get-NumericPropertySum $browserEnd 'cpu_seconds'
    $browserCpuDelta = [Math]::Max(0.0, $browserEndCpu - $browserStartCpu)
    $roles = @(Get-CpuDeltaByRole -StartRows $hallStart -EndRows $hallEnd `
        -ProcessInfo $hallEndInfo -WallSeconds $wallSeconds -LogicalProcessors $script:logicalProcessors)
    $stages = @(Get-ThreadCpuDelta -StartRows $threadStart -EndRows $threadEnd `
        -WallSeconds $wallSeconds -LogicalProcessors $script:logicalProcessors)

    $hallCpuDelta = Get-NumericPropertySum $roles 'cpu_seconds'
    $mainCpuDelta = Get-NumericPropertySum @($roles | Where-Object role -eq 'main') 'cpu_seconds'
    $persistentThreadCpuDelta = Get-NumericPropertySum $stages 'cpu_seconds'
    $shortLivedCpuDelta = [Math]::Max(0.0, $mainCpuDelta - $persistentThreadCpuDelta)
    if ($shortLivedCpuDelta -gt 0.0) {
        $stages += [pscustomobject]@{
            stage = 'ui-and-short-lived-workers'
            cpu_seconds = [Math]::Round($shortLivedCpuDelta, 6)
            one_core_percent = [Math]::Round(100.0 * $shortLivedCpuDelta / $wallSeconds, 3)
            machine_percent = [Math]::Round(100.0 * $shortLivedCpuDelta / $wallSeconds / $script:logicalProcessors, 3)
        }
    }
    $result = [pscustomobject]@{
        name = $Name
        started_local = $started.ToString('o')
        ended_local = $ended.ToString('o')
        duration_seconds = [Math]::Round($wallSeconds, 3)
        system_busy_percent = $systemBusy
        halljoy_cpu_seconds = [Math]::Round($hallCpuDelta, 6)
        halljoy_one_core_percent = [Math]::Round(100.0 * $hallCpuDelta / $wallSeconds, 3)
        halljoy_machine_percent = [Math]::Round(100.0 * $hallCpuDelta / $wallSeconds / $script:logicalProcessors, 3)
        browser_cpu_seconds = [Math]::Round($browserCpuDelta, 6)
        browser_one_core_percent = [Math]::Round(100.0 * $browserCpuDelta / $wallSeconds, 3)
        browser_machine_percent = [Math]::Round(100.0 * $browserCpuDelta / $wallSeconds / $script:logicalProcessors, 3)
        halljoy_max_working_set_bytes = $maxHallWorking
        halljoy_max_private_bytes = $maxHallPrivate
        halljoy_max_handles = $maxHallHandles
        halljoy_max_threads = $maxHallThreads
        browser_max_working_set_bytes = $maxBrowserWorking
        browser_max_private_bytes = $maxBrowserPrivate
        process_roles = $roles
        main_thread_stages = $stages
    }
    Write-Host ("Pipeline phase {0}: HallJoy={1:N3}% machine, browser={2:N3}% machine, system={3:N1}%" -f `
        $Name, $result.halljoy_machine_percent, $result.browser_machine_percent, $result.system_busy_percent)
    return $result
}

function Read-NewOverlayPerf {
    param([long]$Offset)
    if (-not (Test-Path -LiteralPath $overlayPerfPath -PathType Leaf)) { return '' }
    $stream = [IO.File]::Open($overlayPerfPath, [IO.FileMode]::Open, [IO.FileAccess]::Read,
        [IO.FileShare]::ReadWrite)
    try {
        if ($Offset -gt $stream.Length) { $Offset = 0 }
        [void]$stream.Seek($Offset, [IO.SeekOrigin]::Begin)
        $reader = New-Object IO.StreamReader($stream, [Text.UTF8Encoding]::new($false), $true, 4096, $true)
        try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
    }
    finally { $stream.Dispose() }
}

function Add-OverlayMetrics {
    param($Phases, [string]$Text)
    foreach ($phase in $Phases) {
        $start = [DateTimeOffset]::Parse($phase.started_local)
        $end = [DateTimeOffset]::Parse($phase.ended_local)
        $windows = @()
        foreach ($line in @($Text -split "`r?`n")) {
            if ($line -notmatch '^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}) \[overlay\.perf\] (.*)$') { continue }
            $when = [DateTimeOffset]::new([DateTime]::ParseExact(
                $Matches[1], 'yyyy-MM-dd HH:mm:ss.fff', [Globalization.CultureInfo]::InvariantCulture),
                [TimeZoneInfo]::Local.GetUtcOffset([DateTime]::Now))
            if ($when -lt $start -or $when -gt $end) { continue }
            $fields = @{}
            foreach ($token in $Matches[2] -split ' ') {
                if ($token -match '^([^=]+)=([0-9.]+)$') { $fields[$Matches[1]] = [double]$Matches[2] }
            }
            $windows += $fields
        }
        [double]$state = 0.0
        [double]$sendSamples = 0.0
        [double]$frames = 0.0
        [double]$fetches = 0.0
        [double]$weightedBuild = 0.0
        [double]$weightedSend = 0.0
        [double]$weightedFetch = 0.0
        [double]$weightedRender = 0.0
        [double]$buildMax = 0.0
        [double]$sendMax = 0.0
        foreach ($window in $windows) {
            $state += [double]$window['state']
            $sendSamples += [double]$window['send_samples']
            $frames += [double]$window['client_frames']
            $fetches += [double]$window['client_fetches']
            $weightedBuild += [double]$window['build_us_avg'] * [double]$window['state']
            $weightedSend += [double]$window['send_us_avg'] * [double]$window['send_samples']
            $weightedFetch += [double]$window['client_fetch_us_avg'] * [double]$window['client_fetches']
            $weightedRender += [double]$window['client_render_us_avg'] * [double]$window['client_frames']
            $buildMax = [Math]::Max($buildMax, [double]$window['build_us_max'])
            $sendMax = [Math]::Max($sendMax, [double]$window['send_us_max'])
        }
        Add-Member -InputObject $phase -NotePropertyName overlay_metrics -NotePropertyValue ([pscustomobject]@{
            windows = $windows.Count
            state_requests = [long]$state
            state_requests_per_second = [Math]::Round($state / [double]$phase.duration_seconds, 3)
            build_us_avg = if ($state) { [Math]::Round($weightedBuild / $state, 3) } else { 0.0 }
            build_us_max = [long]$buildMax
            send_responses = [long]$sendSamples
            send_us_avg = if ($sendSamples) { [Math]::Round($weightedSend / $sendSamples, 3) } else { 0.0 }
            send_us_max = [long]$sendMax
            client_frames = [long]$frames
            client_frames_per_second = [Math]::Round($frames / [double]$phase.duration_seconds, 3)
            client_fetch_us_avg = if ($fetches) { [Math]::Round($weightedFetch / $fetches, 3) } else { 0.0 }
            client_render_us_avg = if ($frames) { [Math]::Round($weightedRender / $frames, 3) } else { 0.0 }
        })
    }
}

$script:logicalProcessors = [Environment]::ProcessorCount
$script:browserExecutable = Find-Browser
$configuredPort = Get-ConfiguredOverlayPort
$stateBefore = @(Get-StateManifest)
$stateBefore | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'state-before.json') -Encoding UTF8
$exeHash = (Get-FileHash -LiteralPath $ExePath -Algorithm SHA256).Hash
$overlayPerfOffset = if (Test-Path -LiteralPath $overlayPerfPath -PathType Leaf) {
    (Get-Item -LiteralPath $overlayPerfPath).Length
} else { 0L }
$phases = @()
$script:hallJoy = $null
$activeBrowser = $null

try {
    $script:hallJoy = Start-Process -FilePath $ExePath -ArgumentList '--overlay-server' `
        -WorkingDirectory $output -WindowStyle Hidden -PassThru
    $ready = $false
    for ($attempt = 0; $attempt -lt 20; ++$attempt) {
        Start-Sleep -Milliseconds 500
        if ($script:hallJoy.HasExited) { throw "HallJoy exited during startup with $($script:hallJoy.ExitCode)." }
        $probeOutput = @(& python (Join-Path $root 'tools\check_overlay_responsiveness.py') `
            --port $configuredPort --deadline-ms 1000 --connect-deadline-ms 500 2>&1)
        if ($LASTEXITCODE -eq 0) { $ready = $true; break }
    }
    if (-not $ready) { throw "Overlay did not become ready on port $configuredPort." }
    Start-Sleep -Seconds 5

    $phases += Measure-ProfilePhase -Name 'server-idle' -Browser $null

    $activeBrowser = Start-ProfileBrowser -Url "http://127.0.0.1:$configuredPort/" -Name 'real'
    Start-Sleep -Seconds $BrowserWarmupSeconds
    $phases += Measure-ProfilePhase -Name 'real-overlay' -Browser $activeBrowser
    Stop-ProfileBrowser -Browser $activeBrowser
    $activeBrowser = $null
    Start-Sleep -Seconds 6

    $activeBrowser = Start-ProfileBrowser `
        -Url "http://127.0.0.1:$configuredPort/?synthetic=1&active=32&hz=2.2" -Name 'synthetic'
    Start-Sleep -Seconds $BrowserWarmupSeconds
    $phases += Measure-ProfilePhase -Name 'synthetic-32-key-overlay' -Browser $activeBrowser
    Stop-ProfileBrowser -Browser $activeBrowser
    $activeBrowser = $null

    $closeCount = [HallJoyPipelineProfileNative]::PostClose([uint32]$script:hallJoy.Id)
    if ($closeCount -lt 1) { throw 'No HallJoy window accepted WM_CLOSE.' }
    if (-not $script:hallJoy.WaitForExit(15000)) { throw 'HallJoy did not exit within 15 seconds.' }
    if ($script:hallJoy.ExitCode -ne 0) { throw "HallJoy exited with code $($script:hallJoy.ExitCode)." }
    Start-Sleep -Milliseconds 500
    if (@(Get-Process -Name HallJoy -ErrorAction SilentlyContinue).Count -ne 0) {
        throw 'HallJoy left a process after profiling shutdown.'
    }

    $stateAfter = @(Get-StateManifest)
    $stateAfter | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'state-after.json') -Encoding UTF8
    $beforeJson = ConvertTo-Json -InputObject $stateBefore -Depth 4 -Compress
    $afterJson = ConvertTo-Json -InputObject $stateAfter -Depth 4 -Compress
    if ($beforeJson -ne $afterJson) { throw 'User state changed during input pipeline profiling.' }

    if (-not (Test-Path -LiteralPath $tracePath -PathType Leaf)) { throw 'No stability trace was produced.' }
    $traceDestination = Join-Path $EvidenceRoot 'HallJoyStabilityTrace.log'
    Copy-Item -LiteralPath $tracePath -Destination $traceDestination -Force
    $traceText = Get-Content -LiteralPath $traceDestination -Raw
    if ($traceText -match '\[level=ERROR\]') { throw 'Stability trace contains ERROR.' }
    if ($traceText -notmatch '\[component=spark\]\[event=worker\.stats\].*route_queries=(\d+).*route_ok=(\d+).*route_fail=(\d+).*avg_route_tx_us=(\d+).*max_route_tx_us=(\d+)') {
        throw 'SparkLink worker statistics were not found.'
    }
    $sparkQueries = [long]$Matches[1]
    $sparkOk = [long]$Matches[2]
    $sparkFail = [long]$Matches[3]
    $sparkAverageTxUs = [long]$Matches[4]
    $sparkMaxTxUs = [long]$Matches[5]
    if ($sparkQueries -eq 0 -or $sparkOk -eq 0) { throw 'Physical SparkLink polling was not proven.' }

    $overlayPerfText = Read-NewOverlayPerf -Offset $overlayPerfOffset
    $overlayPerfEvidence = Join-Path $EvidenceRoot 'overlay_perf.log'
    Set-Content -LiteralPath $overlayPerfEvidence -Value $overlayPerfText -Encoding UTF8
    Add-OverlayMetrics -Phases $phases -Text $overlayPerfText
    $realPhase = @($phases | Where-Object name -eq 'real-overlay')[0]
    $syntheticPhase = @($phases | Where-Object name -eq 'synthetic-32-key-overlay')[0]
    if ($realPhase.overlay_metrics.state_requests -eq 0 -or $realPhase.overlay_metrics.client_frames -eq 0) {
        throw 'The real browser phase produced no state or render telemetry.'
    }
    if ($syntheticPhase.overlay_metrics.client_frames -eq 0) {
        throw 'The synthetic browser phase produced no render telemetry.'
    }

    $summary = [ordered]@{
        schema = 1
        status = 'passed'
        completed_local = (Get-Date).ToString('o')
        executable = $ExePath
        executable_sha256 = $exeHash
        browser = $script:browserExecutable
        browser_version = (Get-Item -LiteralPath $script:browserExecutable).VersionInfo.FileVersion
        logical_processors = $script:logicalProcessors
        overlay_port = $configuredPort
        phase_seconds = $PhaseSeconds
        browser_warmup_seconds = $BrowserWarmupSeconds
        physical_keyboard = 'Irok MG75 Max / native SparkLink'
        spark_route_queries = $sparkQueries
        spark_route_ok = $sparkOk
        spark_route_fail = $sparkFail
        spark_avg_route_tx_us = $sparkAverageTxUs
        spark_max_route_tx_us = $sparkMaxTxUs
        user_state_unchanged = $true
        remaining_halljoy_processes = 0
        phases = $phases
        trace_sha256 = (Get-FileHash -LiteralPath $traceDestination -Algorithm SHA256).Hash
        overlay_perf_sha256 = (Get-FileHash -LiteralPath $overlayPerfEvidence -Algorithm SHA256).Hash
    }
    $summaryPath = Join-Path $EvidenceRoot 'summary.json'
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-Host 'HallJoy input-to-overlay production profile: PASS' -ForegroundColor Green
    Write-Host "Evidence: $EvidenceRoot"
    Write-Host "Summary: $summaryPath"
}
finally {
    if ($activeBrowser) {
        try { Stop-ProfileBrowser -Browser $activeBrowser } catch { Write-Warning $_.Exception.Message }
    }
    if ($script:hallJoy -and -not $script:hallJoy.HasExited) {
        [void][HallJoyPipelineProfileNative]::PostClose([uint32]$script:hallJoy.Id)
        if (-not $script:hallJoy.WaitForExit(15000)) {
            Write-Warning "HallJoy PID $($script:hallJoy.Id) did not exit during profiler cleanup."
        }
    }
}
