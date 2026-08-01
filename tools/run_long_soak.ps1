[CmdletBinding()]
param(
    [string]$ExePath,
    [ValidateRange(1, 1440)]
    [int]$DurationMinutes = 480,
    [ValidateRange(1, 60)]
    [int]$SampleSeconds = 5,
    [ValidateRange(1, 300)]
    [int]$WarmupSeconds = 10,
    [ValidateRange(3, 60)]
    [int]$ShutdownTimeoutSeconds = 15,
    [switch]$StartOverlay,
    [ValidateRange(1, 65535)]
    [int]$OverlayPort = 18765,
    [ValidateRange(1, 60)]
    [int]$OverlayProbeMinutes = 5,
    [ValidateRange(1, 60)]
    [int]$ProgressMinutes = 5,
    [string]$EvidenceRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ($WarmupSeconds -ge ($DurationMinutes * 60)) {
    throw 'WarmupSeconds must be shorter than the requested soak duration.'
}

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $root 'build\output\HallJoy.exe'
}
$ExePath = [IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "Production executable was not found: $ExePath"
}
if ([IO.Path]::GetFileName($ExePath) -ne 'HallJoy.exe') {
    throw "Long soak requires the final executable name HallJoy.exe: $ExePath"
}

$existing = @(Get-Process -Name 'HallJoy' -ErrorAction SilentlyContinue)
if ($existing.Count -ne 0) {
    throw "Refusing to start while HallJoy is already running (PID: $($existing.Id -join ', '))."
}

$output = Split-Path -Parent $ExePath
$trace = Join-Path $output 'HallJoyStabilityTrace.log'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $output "long-soak\$stamp"
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
$checkpointPath = Join-Path $EvidenceRoot 'checkpoint.json'
$samplesPath = Join-Path $EvidenceRoot 'samples.csv'
$probeLogPath = Join-Path $EvidenceRoot 'overlay-probes.log'
$stateBeforePath = Join-Path $EvidenceRoot 'state-before.json'
$stateAfterPath = Join-Path $EvidenceRoot 'state-after.json'

if (-not ('HallJoyLongSoakNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class HallJoyLongSoakNative {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern uint GetGuiResources(IntPtr process, uint flags);

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

function Get-HallJoyStateSnapshot {
    param([string]$StateRoot)

    $snapshot = @{}
    if (-not (Test-Path -LiteralPath $StateRoot -PathType Container)) {
        return $snapshot
    }
    foreach ($file in Get-ChildItem -LiteralPath $StateRoot -File -Recurse | Sort-Object FullName) {
        $relative = $file.FullName.Substring($StateRoot.Length).TrimStart('\')
        $snapshot[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
    return $snapshot
}

function Write-StateSnapshot {
    param(
        [hashtable]$Snapshot,
        [string]$Path
    )

    $entries = @($Snapshot.Keys | Sort-Object | ForEach-Object {
        [pscustomobject]@{ file = $_; sha256 = $Snapshot[$_] }
    })
    ConvertTo-Json -InputObject $entries -Depth 3 |
        Set-Content -LiteralPath $Path -Encoding UTF8
}

function Get-ChangedStateFiles {
    param(
        [hashtable]$Before,
        [hashtable]$After
    )

    $keys = @($Before.Keys + $After.Keys | Sort-Object -Unique)
    return @($keys | Where-Object {
        -not $Before.ContainsKey($_) -or -not $After.ContainsKey($_) -or $Before[$_] -ne $After[$_]
    })
}

function Wait-NoHallJoyProcess {
    param([int]$TimeoutSeconds)

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $remaining = @(Get-Process -Name 'HallJoy' -ErrorAction SilentlyContinue)
        if ($remaining.Count -eq 0) {
            return
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "HallJoy process remained after shutdown (PID: $($remaining.Id -join ', '))."
}

function Invoke-OverlayProbe {
    $probeStarted = [DateTime]::UtcNow
    $probeOutput = @(& python (Join-Path $root 'tools\check_overlay_responsiveness.py') `
        --port $OverlayPort --deadline-ms 1000 --connect-deadline-ms 5000 2>&1)
    $probeExitCode = $LASTEXITCODE
    Add-Content -LiteralPath $probeLogPath -Encoding UTF8 -Value @(
        "[$($probeStarted.ToString('o'))] exit_code=$probeExitCode",
        ($probeOutput -join [Environment]::NewLine)
    )
    if ($probeExitCode -ne 0) {
        throw "Overlay responsiveness probe failed with exit code $probeExitCode."
    }
    return [pscustomobject]@{
        utc = $probeStarted.ToString('o')
        exit_code = $probeExitCode
    }
}

$stateRoot = Join-Path $env:LOCALAPPDATA 'HallJoy'
$stateBefore = Get-HallJoyStateSnapshot -StateRoot $stateRoot
Write-StateSnapshot -Snapshot $stateBefore -Path $stateBeforePath
$exeHash = (Get-FileHash -LiteralPath $ExePath -Algorithm SHA256).Hash
$samples = [System.Collections.Generic.List[object]]::new()
$overlayProbes = [System.Collections.Generic.List[object]]::new()
$process = $null
$startedAt = [DateTime]::UtcNow
$failureMessage = ''

function Write-SoakCheckpoint {
    param([string]$Status)

    $lastSample = if ($samples.Count -gt 0) { $samples[$samples.Count - 1] } else { $null }
    $checkpoint = [ordered]@{
        schema = 1
        status = $Status
        updated_utc = [DateTime]::UtcNow.ToString('o')
        executable = $ExePath
        executable_sha256 = $exeHash
        duration_minutes_requested = $DurationMinutes
        sample_seconds = $SampleSeconds
        warmup_seconds = $WarmupSeconds
        progress_minutes = $ProgressMinutes
        samples_completed = $samples.Count
        last_sample = $lastSample
        overlay_enabled = [bool]$StartOverlay
        overlay_probes_completed = $overlayProbes.Count
        fault_injection = $false
        error = $failureMessage
    }
    $checkpoint | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $checkpointPath -Encoding UTF8
}

if (Test-Path -LiteralPath $trace -PathType Leaf) {
    Move-Item -LiteralPath $trace -Destination (Join-Path $EvidenceRoot 'preexisting-trace.log')
}
Write-SoakCheckpoint -Status 'starting'

try {
    $processArguments = @()
    if ($StartOverlay) {
        $processArguments += @('--overlay-server', '--port', [string]$OverlayPort)
    }
    $startParameters = @{
        FilePath = $ExePath
        PassThru = $true
        WindowStyle = 'Hidden'
        WorkingDirectory = $output
    }
    if ($processArguments.Count -ne 0) {
        $startParameters.ArgumentList = $processArguments
    }

    # This is the production path: no simulator and no fault-injection arguments.
    $process = Start-Process @startParameters
    $startedAt = [DateTime]::UtcNow
    $endsAt = $startedAt.AddMinutes($DurationMinutes)
    $nextOverlayProbe = $startedAt
    $nextCheckpoint = $startedAt
    $nextProgress = $startedAt.AddMinutes($ProgressMinutes)
    Write-SoakCheckpoint -Status 'running'

    while ([DateTime]::UtcNow -lt $endsAt) {
        if ($process.WaitForExit(100)) {
            throw "HallJoy exited before the soak duration completed (exit $($process.ExitCode))."
        }
        $process.Refresh()
        $now = [DateTime]::UtcNow
        $sample = [pscustomobject]@{
            utc = $now.ToString('o')
            elapsed_seconds = [Math]::Round(($now - $startedAt).TotalSeconds, 3)
            pid = $process.Id
            handles = $process.HandleCount
            threads = $process.Threads.Count
            gdi_objects = [HallJoyLongSoakNative]::GetGuiResources($process.Handle, 0)
            user_objects = [HallJoyLongSoakNative]::GetGuiResources($process.Handle, 1)
            working_set_bytes = $process.WorkingSet64
            private_bytes = $process.PrivateMemorySize64
            paged_bytes = $process.PagedMemorySize64
            cpu_seconds = [Math]::Round($process.TotalProcessorTime.TotalSeconds, 6)
        }
        $samples.Add($sample)
        $sample | Export-Csv -LiteralPath $samplesPath -Append -NoTypeInformation -Encoding UTF8

        if ($StartOverlay -and $now -ge $nextOverlayProbe) {
            $overlayProbes.Add((Invoke-OverlayProbe))
            $nextOverlayProbe = $now.AddMinutes($OverlayProbeMinutes)
        }
        if ($now -ge $nextCheckpoint) {
            Write-SoakCheckpoint -Status 'running'
            $nextCheckpoint = $now.AddMinutes(1)
        }
        if ($now -ge $nextProgress) {
            Write-Host ("Soak progress: elapsed_min={0:N1}/{1} samples={2} handles={3} private_bytes={4}" -f `
                ($now - $startedAt).TotalMinutes, $DurationMinutes, $samples.Count,
                $sample.handles, $sample.private_bytes)
            $nextProgress = $now.AddMinutes($ProgressMinutes)
        }
        Start-Sleep -Seconds $SampleSeconds
    }

    $shutdownStartedAt = [DateTime]::UtcNow
    $shutdownDeadline = $shutdownStartedAt.AddSeconds($ShutdownTimeoutSeconds)
    $closeAccepted = $false
    do {
        if ([HallJoyLongSoakNative]::PostClose([uint32]$process.Id) -gt 0) {
            $closeAccepted = $true
        }
        if ($process.WaitForExit(100)) { break }
    } while ([DateTime]::UtcNow -lt $shutdownDeadline)
    if (-not $closeAccepted) {
        throw 'HallJoy exposed no window accepting WM_CLOSE after the soak.'
    }
    if (-not $process.HasExited) {
        throw "HallJoy did not exit within $ShutdownTimeoutSeconds seconds after the soak."
    }
    if ($process.ExitCode -ne 0) {
        throw "HallJoy exited with code $($process.ExitCode) after the soak."
    }
    $shutdownMs = [Math]::Round(([DateTime]::UtcNow - $shutdownStartedAt).TotalMilliseconds)
    Wait-NoHallJoyProcess -TimeoutSeconds 3

    if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
        throw 'Production trace was not produced by the soak.'
    }
    $traceText = Get-Content -LiteralPath $trace -Raw -Encoding UTF8
    $requiredTraceTokens = @(
        '[component=app][event=startup.transaction.commit] origin=initial',
        '[component=realtime][event=stop.end]',
        '[component=vigem-output][event=stop.end]',
        '[component=backend][event=shutdown.end]',
        '[component=main][event=session.end] exit_code=0'
    )
    $missing = @($requiredTraceTokens | Where-Object { -not $traceText.Contains($_) })
    if ($missing.Count -ne 0) {
        throw "Production trace is incomplete. Missing: $($missing -join ', ')"
    }
    if ($traceText -match '\[level=ERROR\]') {
        throw 'Production trace contains an ERROR event.'
    }
    if ($traceText -match '\[component=trace\]\[event=capped\]') {
        throw 'Production trace reached its cap.'
    }

    $soakTracePath = Join-Path $EvidenceRoot 'soak-trace.log'
    Move-Item -LiteralPath $trace -Destination $soakTracePath
    $analysisOutput = @(& python (Join-Path $root 'tools\analyze_stability_trace.py') $soakTracePath 2>&1)
    $analysisExitCode = $LASTEXITCODE
    $analysisOutput | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'trace-analysis.txt') -Encoding UTF8
    if ($analysisExitCode -ge 2) {
        throw "Stability trace analyzer returned FAIL (exit $analysisExitCode)."
    }

    if ($samples.Count -eq 0) {
        throw 'No process resource samples were recorded.'
    }
    $startupFirst = $samples[0]
    $baseline = @($samples | Where-Object {
        [double]$_.elapsed_seconds -ge $WarmupSeconds
    } | Select-Object -First 1)
    if ($baseline.Count -ne 1) {
        throw "No resource baseline was recorded after the $WarmupSeconds-second warm-up."
    }
    $first = $baseline[0]
    $last = $samples[$samples.Count - 1]
    $handleGrowth = [long]$last.handles - [long]$first.handles
    $privateGrowth = [long]$last.private_bytes - [long]$first.private_bytes
    $maxHandles = ($samples | Measure-Object -Property handles -Maximum).Maximum
    $maxWorkingSet = ($samples | Measure-Object -Property working_set_bytes -Maximum).Maximum
    $maxPrivate = ($samples | Measure-Object -Property private_bytes -Maximum).Maximum
    $maxGdi = ($samples | Measure-Object -Property gdi_objects -Maximum).Maximum
    $maxUser = ($samples | Measure-Object -Property user_objects -Maximum).Maximum
    if ($handleGrowth -gt 32 -or $maxHandles -gt ([long]$first.handles + 64)) {
        throw "Handle-growth gate failed: first=$($first.handles), last=$($last.handles), max=$maxHandles."
    }
    if ($privateGrowth -gt 268435456) {
        throw "Private-memory growth gate failed: growth_bytes=$privateGrowth."
    }

    $stateAfter = Get-HallJoyStateSnapshot -StateRoot $stateRoot
    Write-StateSnapshot -Snapshot $stateAfter -Path $stateAfterPath
    $changedStateFiles = @(Get-ChangedStateFiles -Before $stateBefore -After $stateAfter)
    if ($changedStateFiles.Count -ne 0) {
        throw "HallJoy user state changed during soak: $($changedStateFiles -join ', ')"
    }

    $actualSeconds = ([DateTime]::UtcNow - $startedAt).TotalSeconds
    $cpuDeltaSeconds = [double]$last.cpu_seconds - [double]$first.cpu_seconds
    $normalizedCpuPercent = if ($actualSeconds -gt 0) {
        [Math]::Round(100.0 * $cpuDeltaSeconds / $actualSeconds / [Environment]::ProcessorCount, 3)
    } else { 0.0 }
    $sparkRouteQueries = 0L
    $sparkRouteOk = 0L
    $sparkRouteFail = 0L
    $sparkStats = [regex]::Matches(
        $traceText,
        '\[component=spark\]\[event=worker\.stats\][^\r\n]*route_queries=(\d+)[^\r\n]*route_ok=(\d+)[^\r\n]*route_fail=(\d+)'
    )
    if ($sparkStats.Count -gt 0) {
        $lastSparkStats = $sparkStats[$sparkStats.Count - 1]
        $sparkRouteQueries = [long]$lastSparkStats.Groups[1].Value
        $sparkRouteOk = [long]$lastSparkStats.Groups[2].Value
        $sparkRouteFail = [long]$lastSparkStats.Groups[3].Value
    }

    $summary = [ordered]@{
        schema = 1
        status = 'passed'
        completed_utc = [DateTime]::UtcNow.ToString('o')
        executable = $ExePath
        executable_sha256 = $exeHash
        duration_minutes_requested = $DurationMinutes
        duration_seconds_actual = [Math]::Round($actualSeconds, 3)
        sample_seconds = $SampleSeconds
        warmup_seconds = $WarmupSeconds
        samples = $samples.Count
        shutdown_ms = $shutdownMs
        fault_injection = $false
        overlay_enabled = [bool]$StartOverlay
        overlay_port = if ($StartOverlay) { $OverlayPort } else { $null }
        overlay_probes = $overlayProbes.Count
        remaining_processes = 0
        user_state_files_before = $stateBefore.Count
        user_state_files_after = $stateAfter.Count
        user_state_changed = 0
        startup_first_handles = $startupFirst.handles
        baseline_elapsed_seconds = $first.elapsed_seconds
        first_handles = $first.handles
        last_handles = $last.handles
        handle_growth = $handleGrowth
        max_handles = $maxHandles
        max_gdi_objects = $maxGdi
        max_user_objects = $maxUser
        startup_first_private_bytes = $startupFirst.private_bytes
        first_private_bytes = $first.private_bytes
        last_private_bytes = $last.private_bytes
        private_growth_bytes = $privateGrowth
        max_private_bytes = $maxPrivate
        max_working_set_bytes = $maxWorkingSet
        cpu_delta_seconds = [Math]::Round($cpuDeltaSeconds, 6)
        normalized_cpu_percent = $normalizedCpuPercent
        spark_route_queries = $sparkRouteQueries
        spark_route_ok = $sparkRouteOk
        spark_route_fail = $sparkRouteFail
        trace_analysis = if ($analysisExitCode -eq 0) { 'PASS' } else { 'WARN' }
        trace_sha256 = (Get-FileHash -LiteralPath $soakTracePath -Algorithm SHA256).Hash
    }
    $summaryPath = Join-Path $EvidenceRoot 'summary.json'
    $summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-SoakCheckpoint -Status 'passed'

    Write-Host "HallJoy long production soak: PASS ($DurationMinutes minute(s), $($samples.Count) samples)" -ForegroundColor Green
    Write-Host "Handles: first=$($first.handles) last=$($last.handles) max=$maxHandles" -ForegroundColor Green
    Write-Host "User state unchanged: $($stateBefore.Count) files" -ForegroundColor Green
    Write-Host "Evidence: $EvidenceRoot"
    Write-Host "Summary: $summaryPath"
}
catch {
    $failureMessage = $_.Exception.Message
    $stateAfterFailure = Get-HallJoyStateSnapshot -StateRoot $stateRoot
    Write-StateSnapshot -Snapshot $stateAfterFailure -Path $stateAfterPath
    Write-SoakCheckpoint -Status 'failed'
    Write-Host "Long soak failed after $($samples.Count) samples." -ForegroundColor Red
    Write-Host "Checkpoint: $checkpointPath" -ForegroundColor Yellow
    throw
}
finally {
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            $process.WaitForExit(5000) | Out-Null
        }
    }
}
