[CmdletBinding()]
param(
    [string]$ExePath,
    [ValidateRange(1, 1000)]
    [int]$Cycles = 25,
    [ValidateRange(1, 600)]
    [int]$RunSeconds = 2,
    [ValidateRange(3, 60)]
    [int]$ShutdownTimeoutSeconds = 15,
    [ValidateRange(1, 1000)]
    [int]$ProgressEvery = 25,
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
    throw "Production executable was not found: $ExePath"
}
if ([IO.Path]::GetFileName($ExePath) -ne 'HallJoy.exe') {
    throw "Release qualification requires the final executable name HallJoy.exe: $ExePath"
}

$existing = @(Get-Process -Name 'HallJoy' -ErrorAction SilentlyContinue)
if ($existing.Count -ne 0) {
    throw "Refusing to start while HallJoy is already running (PID: $($existing.Id -join ', '))."
}

$output = Split-Path -Parent $ExePath
$trace = Join-Path $output 'HallJoyStabilityTrace.log'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $output "release-qualification\$stamp"
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
$checkpointPath = Join-Path $EvidenceRoot 'checkpoint.json'
$stateBeforePath = Join-Path $EvidenceRoot 'state-before.json'
$stateAfterPath = Join-Path $EvidenceRoot 'state-after.json'

if (-not ('HallJoyReleaseQualificationWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class HallJoyReleaseQualificationWindow {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);

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

function Convert-StateSnapshotToEntries {
    param([hashtable]$Snapshot)

    return @($Snapshot.Keys | Sort-Object | ForEach-Object {
        [pscustomobject]@{
            file = $_
            sha256 = $Snapshot[$_]
        }
    })
}

function Write-StateSnapshot {
    param(
        [hashtable]$Snapshot,
        [string]$Path
    )

    $entries = Convert-StateSnapshotToEntries -Snapshot $Snapshot
    ConvertTo-Json -InputObject @($entries) -Depth 3 |
        Set-Content -LiteralPath $Path -Encoding UTF8
}

function Assert-SameStateSnapshot {
    param(
        [hashtable]$Before,
        [hashtable]$After
    )

    $beforeKeys = @($Before.Keys | Sort-Object)
    $afterKeys = @($After.Keys | Sort-Object)
    if (($beforeKeys -join "`n") -ne ($afterKeys -join "`n")) {
        throw 'HallJoy user-state file set changed during release qualification.'
    }
    foreach ($key in $beforeKeys) {
        if ($Before[$key] -ne $After[$key]) {
            throw "HallJoy user-state file changed during release qualification: $key"
        }
    }
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

function Write-QualificationCheckpoint {
    param(
        [string]$Status,
        [System.Collections.Generic.List[object]]$CycleResults,
        [string]$ErrorMessage = ''
    )

    $checkpoint = [ordered]@{
        schema = 1
        status = $Status
        updated_utc = [DateTime]::UtcNow.ToString('o')
        executable = $ExePath
        executable_sha256 = $exeHash
        cycles_requested = $Cycles
        cycles_completed = $CycleResults.Count
        run_seconds = $RunSeconds
        shutdown_timeout_seconds = $ShutdownTimeoutSeconds
        fault_injection = $false
        error = $ErrorMessage
        cycles = $CycleResults
    }
    $checkpoint | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $checkpointPath -Encoding UTF8
}

$stateRoot = Join-Path $env:LOCALAPPDATA 'HallJoy'
$stateBefore = Get-HallJoyStateSnapshot -StateRoot $stateRoot
Write-StateSnapshot -Snapshot $stateBefore -Path $stateBeforePath
$exeHash = (Get-FileHash -LiteralPath $ExePath -Algorithm SHA256).Hash
$requiredTraceTokens = @(
    '[component=app][event=startup.transaction.commit] origin=initial',
    '[component=realtime][event=stop.end]',
    '[component=vigem-output][event=stop.end]',
    '[component=backend][event=shutdown.end]',
    '[component=main][event=session.end] exit_code=0'
)
$results = [System.Collections.Generic.List[object]]::new()

if (Test-Path -LiteralPath $trace -PathType Leaf) {
    Move-Item -LiteralPath $trace -Destination (Join-Path $EvidenceRoot 'preexisting-trace.log')
}
Write-QualificationCheckpoint -Status 'running' -CycleResults $results

try {
    for ($cycle = 1; $cycle -le $Cycles; ++$cycle) {
        $process = $null
        $startedAt = [DateTime]::UtcNow
        $maxHandles = 0
        $maxWorkingSet = 0L
        try {
            # Production qualification intentionally passes no fault-injection or
            # simulator arguments. Every cycle is the same path a user launches.
            $process = Start-Process -FilePath $ExePath -PassThru -WindowStyle Hidden -WorkingDirectory $output
            $runDeadline = [DateTime]::UtcNow.AddSeconds($RunSeconds)
            do {
                if ($process.WaitForExit(100)) { break }
                $process.Refresh()
                $maxHandles = [Math]::Max($maxHandles, $process.HandleCount)
                $maxWorkingSet = [Math]::Max($maxWorkingSet, $process.WorkingSet64)
            } while ([DateTime]::UtcNow -lt $runDeadline)

            if ($process.HasExited) {
                throw "HallJoy exited before graceful close in cycle $cycle (exit $($process.ExitCode))."
            }

            $closeAccepted = $false
            $shutdownStartedAt = [DateTime]::UtcNow
            $shutdownDeadline = $shutdownStartedAt.AddSeconds($ShutdownTimeoutSeconds)
            do {
                if ([HallJoyReleaseQualificationWindow]::PostClose([uint32]$process.Id) -gt 0) {
                    $closeAccepted = $true
                }
                if ($process.WaitForExit(100)) { break }
            } while ([DateTime]::UtcNow -lt $shutdownDeadline)

            if (-not $closeAccepted) {
                throw "HallJoy exposed no window accepting WM_CLOSE in cycle $cycle."
            }
            if (-not $process.HasExited) {
                throw "HallJoy did not exit within $ShutdownTimeoutSeconds seconds in cycle $cycle."
            }
            if ($process.ExitCode -ne 0) {
                throw "HallJoy exited with code $($process.ExitCode) in cycle $cycle."
            }

            Wait-NoHallJoyProcess -TimeoutSeconds 3
            if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
                throw "Production trace was not produced in cycle $cycle."
            }
            $traceText = Get-Content -LiteralPath $trace -Raw -Encoding UTF8
            $missing = @($requiredTraceTokens | Where-Object { -not $traceText.Contains($_) })
            if ($missing.Count -ne 0) {
                throw "Production trace is incomplete in cycle $cycle. Missing: $($missing -join ', ')"
            }
            if ($traceText -match '\[level=ERROR\]') {
                throw "Production trace contains an ERROR event in cycle $cycle."
            }
            if ($traceText -match '\[component=trace\]\[event=capped\]') {
                throw "Production trace was capped in cycle $cycle."
            }

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

            $cycleTrace = Join-Path $EvidenceRoot ('cycle-{0:D4}.log' -f $cycle)
            Move-Item -LiteralPath $trace -Destination $cycleTrace
            $results.Add([pscustomobject]@{
                cycle = $cycle
                pid = $process.Id
                runtime_ms = [Math]::Round(([DateTime]::UtcNow - $startedAt).TotalMilliseconds)
                shutdown_ms = [Math]::Round(([DateTime]::UtcNow - $shutdownStartedAt).TotalMilliseconds)
                max_handles = $maxHandles
                max_working_set_bytes = $maxWorkingSet
                spark_route_queries = $sparkRouteQueries
                spark_route_ok = $sparkRouteOk
                spark_route_fail = $sparkRouteFail
                trace_sha256 = (Get-FileHash -LiteralPath $cycleTrace -Algorithm SHA256).Hash
            })
            Write-QualificationCheckpoint -Status 'running' -CycleResults $results
            if ($cycle -eq 1 -or $cycle -eq $Cycles -or ($cycle % $ProgressEvery) -eq 0) {
                Write-Host ("Cycle {0}/{1}: PASS exit=0 shutdown_ms={2} max_handles={3} spark_ok={4} spark_fail={5}" -f `
                    $cycle, $Cycles, $results[$results.Count - 1].shutdown_ms, $maxHandles,
                    $sparkRouteOk, $sparkRouteFail)
            }
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
    }

    $stateAfter = Get-HallJoyStateSnapshot -StateRoot $stateRoot
    Write-StateSnapshot -Snapshot $stateAfter -Path $stateAfterPath
    Assert-SameStateSnapshot -Before $stateBefore -After $stateAfter
    Wait-NoHallJoyProcess -TimeoutSeconds 3

    $summary = [ordered]@{
        schema = 1
        status = 'passed'
        completed_utc = [DateTime]::UtcNow.ToString('o')
        executable = $ExePath
        executable_sha256 = $exeHash
        cycles_requested = $Cycles
        cycles_passed = $results.Count
        run_seconds = $RunSeconds
        shutdown_timeout_seconds = $ShutdownTimeoutSeconds
        fault_injection = $false
        remaining_processes = 0
        user_state_files_before = $stateBefore.Count
        user_state_files_after = $stateAfter.Count
        user_state_changed = 0
        min_shutdown_ms = ($results | Measure-Object -Property shutdown_ms -Minimum).Minimum
        max_shutdown_ms = ($results | Measure-Object -Property shutdown_ms -Maximum).Maximum
        max_handles_observed = ($results | Measure-Object -Property max_handles -Maximum).Maximum
        max_working_set_bytes = ($results | Measure-Object -Property max_working_set_bytes -Maximum).Maximum
        spark_route_queries = ($results | Measure-Object -Property spark_route_queries -Sum).Sum
        spark_route_ok = ($results | Measure-Object -Property spark_route_ok -Sum).Sum
        spark_route_fail = ($results | Measure-Object -Property spark_route_fail -Sum).Sum
        cycles = $results
    }
    $summaryPath = Join-Path $EvidenceRoot 'summary.json'
    $summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-QualificationCheckpoint -Status 'passed' -CycleResults $results

    Write-Host "HallJoy normal start/stop release qualification: PASS ($Cycles/$Cycles cycles)" -ForegroundColor Green
    Write-Host "User state unchanged: $($stateBefore.Count) files" -ForegroundColor Green
    Write-Host "Evidence: $EvidenceRoot"
    Write-Host "Summary: $summaryPath"
}
catch {
    $failureMessage = $_.Exception.Message
    $stateAfterFailure = Get-HallJoyStateSnapshot -StateRoot $stateRoot
    Write-StateSnapshot -Snapshot $stateAfterFailure -Path $stateAfterPath
    Write-QualificationCheckpoint -Status 'failed' -CycleResults $results -ErrorMessage $failureMessage
    Write-Host "Release qualification failed after $($results.Count)/$Cycles completed cycles." -ForegroundColor Red
    Write-Host "Checkpoint: $checkpointPath" -ForegroundColor Yellow
    throw
}
