[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$ForceUserUapRuntime,
    [switch]$StartOverlay,
    [switch]$InjectRealtimeStopTimeout,
    [switch]$InjectDebugLogStopTimeout,
    [switch]$InjectOverlayStopTimeout,
    [switch]$InjectSparkStopTimeout,
    [switch]$InjectSparkShutdownRace,
    [switch]$InjectSayoStopTimeout,
    [switch]$InjectSayoReaderCppFault,
    [switch]$InjectAnalogHostBridgeStopTimeout,
    [switch]$InjectAnalogHostSupervisorStartFailure,
    [switch]$InjectAnalogHostSupervisorCppFault,
    [switch]$InjectAnalogHostChildCppFault,
    [switch]$InjectAnalogHostChildReapTimeout,
    [switch]$InjectRealtimeStartFailure,
    [switch]$InjectNativePhaseStartFailure,
    [ValidateRange(7, 120)]
    [int]$RunSeconds = 8
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$injectionCount = @(
    $InjectRealtimeStopTimeout.IsPresent,
    $InjectDebugLogStopTimeout.IsPresent,
    $InjectOverlayStopTimeout.IsPresent,
    $InjectSparkStopTimeout.IsPresent,
    $InjectSparkShutdownRace.IsPresent,
    $InjectSayoStopTimeout.IsPresent,
    $InjectSayoReaderCppFault.IsPresent,
    $InjectAnalogHostBridgeStopTimeout.IsPresent,
    $InjectAnalogHostSupervisorStartFailure.IsPresent,
    $InjectAnalogHostSupervisorCppFault.IsPresent,
    $InjectAnalogHostChildCppFault.IsPresent,
    $InjectAnalogHostChildReapTimeout.IsPresent,
    $InjectRealtimeStartFailure.IsPresent,
    $InjectNativePhaseStartFailure.IsPresent
).Where({ $_ }).Count
$isFaultInjection = $injectionCount -ne 0
if ($injectionCount -gt 1) {
    throw 'Select only one fault-injection scenario.'
}
if ($StartOverlay -and $injectionCount -ne 0) {
    throw 'StartOverlay cannot be combined with a fault-injection scenario.'
}

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$output = Join-Path $root 'src\HallJoyProject\x64\AnalogSimulator'
$exe = Join-Path $output 'HallJoyV14Simulator.exe'
$trace = Join-Path $output 'HallJoyStabilityTrace.log'

function Find-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
        if ($found) { return $found }
    }

    $fallback = 'C:\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
    if (Test-Path -LiteralPath $fallback) { return $fallback }
    throw 'MSBuild x64 was not found.'
}

if (-not $SkipBuild) {
    $msbuild = Find-MSBuild
    & $msbuild $project `
        '/t:Rebuild' `
        '/p:Configuration=Release' `
        '/p:Platform=x64' `
        '/p:HallJoyAnalogSimulator=true' `
        '/m'
    if ($LASTEXITCODE -ne 0) {
        throw "Simulator build failed: $LASTEXITCODE"
    }
}

if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Simulator executable was not produced: $exe"
}
if (Test-Path -LiteralPath $trace) {
    Remove-Item -LiteralPath $trace -Force
}

$arguments = @('--halljoy-simulate-analog=script')
if ($ForceUserUapRuntime) {
    $arguments += '--halljoy-test-uap-exe-write-denied'
}
if ($InjectRealtimeStopTimeout) {
    $arguments += '--halljoy-test-realtime-stop-timeout'
}
if ($InjectDebugLogStopTimeout) {
    $arguments += '--halljoy-test-debug-log-stop-timeout'
}
if ($InjectOverlayStopTimeout) {
    $arguments += @('--halljoy-test-overlay-stop-timeout', '--overlay-server', '--port', '18765')
}
if ($InjectSparkStopTimeout) {
    $arguments += '--halljoy-test-spark-stop-timeout'
}
if ($InjectSparkShutdownRace) {
    $arguments += '--halljoy-test-spark-service-shutdown'
}
if ($InjectSayoStopTimeout) {
    $arguments += '--halljoy-test-sayo-stop-timeout'
}
if ($InjectSayoReaderCppFault) {
    $arguments += '--halljoy-test-sayo-reader-cpp-fault'
}
if ($InjectAnalogHostBridgeStopTimeout) {
    $arguments += '--halljoy-test-analog-host-bridge-stop-timeout'
}
if ($InjectAnalogHostSupervisorStartFailure) {
    $arguments += '--halljoy-test-analog-host-supervisor-start-failure'
}
if ($InjectAnalogHostSupervisorCppFault) {
    $arguments += '--halljoy-test-analog-host-supervisor-cpp-fault'
}
if ($InjectAnalogHostChildCppFault) {
    $arguments += '--halljoy-test-analog-host-child-cpp-fault'
}
if ($InjectAnalogHostChildReapTimeout) {
    $arguments += '--halljoy-test-analog-host-child-reap-timeout'
}
if ($InjectRealtimeStartFailure) {
    $arguments += '--halljoy-test-realtime-start-failure'
}
if ($InjectNativePhaseStartFailure) {
    $arguments += '--halljoy-test-native-phase-start-failure'
}
if ($StartOverlay) {
    $arguments += @('--overlay-server', '--port', '18765')
}
$process = Start-Process -FilePath $exe `
    -ArgumentList $arguments `
    -PassThru
try {
    if ($StartOverlay) {
        & python (Join-Path $root 'tools\check_overlay_responsiveness.py') `
            --port 18765 --deadline-ms 1000 --connect-deadline-ms 5000
        if ($LASTEXITCODE -ne 0) {
            throw "Overlay responsiveness regression gate failed with exit code $LASTEXITCODE."
        }
    }
    Start-Sleep -Seconds $RunSeconds
    $processExitTimeoutMs = if ($InjectAnalogHostBridgeStopTimeout) { 15000 } else { 10000 }
    $closeDeadline = [DateTime]::UtcNow.AddMilliseconds($processExitTimeoutMs)
    $closeAccepted = $false
    do {
        $process.Refresh()
        if ($process.HasExited) { break }
        if ($process.CloseMainWindow()) { $closeAccepted = $true }
        if ($process.WaitForExit(250)) { break }
    } while ([DateTime]::UtcNow -lt $closeDeadline)
    if (-not $closeAccepted) {
        throw 'Simulator did not expose a window that accepted graceful close.'
    }
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "Simulator did not exit within $processExitTimeoutMs ms after graceful close."
    }
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
    $process.Refresh()
}

if ($InjectRealtimeStopTimeout -and $process.ExitCode -ne 2) {
    throw "Timeout-injected simulator exited with code $($process.ExitCode), expected 2."
}
if ($InjectDebugLogStopTimeout -and $process.ExitCode -ne 3) {
    throw "Debug-log-timeout simulator exited with code $($process.ExitCode), expected 3."
}
if ($InjectOverlayStopTimeout -and $process.ExitCode -ne 2) {
    throw "Overlay-timeout simulator exited with code $($process.ExitCode), expected 2."
}
if ($InjectSparkStopTimeout -and $process.ExitCode -ne 2) {
    throw "Spark-timeout simulator exited with code $($process.ExitCode), expected 2."
}
if ($InjectSparkShutdownRace -and $process.ExitCode -ne 0) {
    throw "Spark service-shutdown simulator exited with code $($process.ExitCode), expected 0."
}
if ($InjectSayoStopTimeout -and $process.ExitCode -ne 2) {
    throw "Sayo-timeout simulator exited with code $($process.ExitCode), expected 2."
}
if ($InjectSayoReaderCppFault -and $process.ExitCode -ne 0) {
    throw "Sayo reader C++ fault simulator exited with code $($process.ExitCode), expected 0."
}
if ($InjectAnalogHostBridgeStopTimeout -and $process.ExitCode -ne 2) {
    throw "Analog-host bridge-timeout simulator exited with code $($process.ExitCode), expected 2."
}
if ($InjectAnalogHostSupervisorStartFailure -and $process.ExitCode -ne 0) {
    throw "Analog-host partial-start simulator exited with code $($process.ExitCode), expected 0."
}
if ($InjectAnalogHostSupervisorCppFault -and $process.ExitCode -ne 0) {
    throw "Analog-host supervisor C++ fault simulator exited with code $($process.ExitCode), expected 0."
}
if ($InjectAnalogHostChildCppFault -and $process.ExitCode -ne 0) {
    throw "Analog-host child C++ fault simulator exited with code $($process.ExitCode), expected 0."
}
if ($InjectAnalogHostChildReapTimeout -and $process.ExitCode -ne 0) {
    throw "Analog-host child reap-timeout simulator exited with code $($process.ExitCode), expected 0."
}
if ($InjectRealtimeStartFailure -and $process.ExitCode -ne 0) {
    throw "Realtime-start-failure simulator exited with code $($process.ExitCode), expected 0."
}
if ($InjectNativePhaseStartFailure -and $process.ExitCode -ne 0) {
    throw "Native-phase-start-failure simulator exited with code $($process.ExitCode), expected 0."
}
if (-not $isFaultInjection -and $process.ExitCode -ne 0) {
    throw "Simulator exited with code $($process.ExitCode)."
}
if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
    throw "Simulator trace was not produced: $trace"
}

$traceText = Get-Content -LiteralPath $trace -Raw -Encoding UTF8
$required = if ($InjectRealtimeStopTimeout) { @(
    '[component=realtime][event=test.stop_timeout.injected] simulator_only=1',
    '[component=realtime][event=stop.timeout]',
    'handle_retained=1 restart_blocked=1',
    '[component=app][event=shutdown.poisoned]',
    'backend_cleanup_skipped=1',
    '[component=main][event=process_exit.poisoned] exit_code=2 crt_cleanup_skipped=1'
) } elseif ($InjectDebugLogStopTimeout) { @(
    '[component=debug-log][event=test.stop_timeout.injected] simulator_only=1',
    '[component=debug-log][event=stop.timeout]',
    'handles_retained=1 restart_blocked=1',
    '[component=main][event=process_exit.log_poisoned]',
    'exit_code=3 crt_cleanup_skipped=1'
) } elseif ($InjectOverlayStopTimeout) { @(
    '[component=overlay][event=start.ok] port=18765',
    '[component=overlay][event=test.stop_timeout.injected] simulator_only=1',
    '[component=overlay][event=stop.timeout]',
    'thread_handle_retained=1 wsa_retained=1',
    'restart_blocked=1',
    '[component=app][event=shutdown.poisoned] component=overlay',
    'dependent_cleanup_skipped=1',
    '[component=main][event=process_exit.poisoned] exit_code=2 crt_cleanup_skipped=1'
) } elseif ($InjectSparkStopTimeout) { @(
    '[component=spark][event=test.start] simulator_only=1',
    '[component=spark][event=worker.start]',
    '[component=spark][event=test.stop_timeout.injected] simulator_only=1',
    '[component=spark][event=stop.timeout]',
    'thread_handle_retained=1 hid_handle_retained=0 stop_event_retained=1 restart_blocked=1',
    '[component=native-registry][event=stop.incomplete]',
    '[component=app][event=shutdown.poisoned] component=native-analog dependent_cleanup_skipped=1',
    '[component=main][event=process_exit.poisoned] exit_code=2 crt_cleanup_skipped=1'
) } elseif ($InjectSparkShutdownRace) { @(
    '[component=spark][event=service.start]',
    '[component=spark][event=test.service_worker] simulator_only=1 cooperative_wait=1',
    '[component=spark][event=service.stop.begin]',
    'reconnect_blocked=1',
    '[component=spark][event=worker.exit]',
    '[component=spark][event=stop.end]',
    '[component=spark][event=test.service_stop_probe] simulator_only=1 reconnect_blocked=1',
    '[component=spark][event=service.stop.end] joined=1 reconnect_blocked=1',
    '[component=backend][event=shutdown.end]',
    '[component=main][event=session.end] exit_code=0'
) } elseif ($InjectSayoStopTimeout) { @(
    '[component=sayo][event=test.start] simulator_only=1',
    '[component=sayo][event=test.stop_timeout.injected] simulator_only=1',
    '[component=sayo][event=stop.timeout]',
    'readers=1',
    'reader_group_retained=1 hid_handles_retained=0 stop_event_retained=1 restart_blocked=1',
    '[component=native-registry][event=stop.incomplete]',
    '[component=app][event=shutdown.poisoned] component=native-analog dependent_cleanup_skipped=1',
    '[component=main][event=process_exit.poisoned] exit_code=2 crt_cleanup_skipped=1'
) } elseif ($InjectSayoReaderCppFault) { @(
    '[component=sayo][event=worker.start] reader=0',
    '[component=sayo][event=worker.fault] reader=0 kind=1 neutralized=1',
    'group_stop_signaled=1',
    '[component=sayo][event=worker.exit] reader=0 fault_kind=1 live_readers=0',
    '[component=sayo][event=start.failed] stage=reader_early_exit',
    '[component=backend][event=shutdown.end]'
) } elseif ($InjectAnalogHostBridgeStopTimeout) { @(
    '[component=analog-host][event=test.bridge_stop_timeout.injected] simulator_only=1',
    '[component=analog-host][event=stop.timeout]',
    'bridge_handle_retained=1 supervisor_handle_retained=1 ipc_retained=1',
    'restart_blocked=1',
    '[component=backend][event=shutdown.end] native_joined=1 analog_host_joined=0',
    '[component=app][event=shutdown.poisoned] component=backend',
    '[component=main][event=process_exit.poisoned] exit_code=2 crt_cleanup_skipped=1'
) } elseif ($InjectAnalogHostSupervisorStartFailure) { @(
    '[component=analog-host][event=partial_start.rollback] stage=supervisor_create joined=1 resources_released=1 injected=1',
    '[component=backend][event=shutdown.end] native_joined=1 analog_host_joined=1',
    '[component=main][event=session.end] exit_code=0'
) } elseif ($InjectAnalogHostSupervisorCppFault) { @(
    '[component=analog-host][event=parent_worker.fault] worker=supervisor',
    'neutralized=1 restart_blocked=1',
    '[component=analog-host][event=worker.exit] worker=supervisor fault_kind=1',
    '[component=analog-host][event=worker.exit] worker=snapshot_bridge fault_kind=0',
    '[component=backend][event=shutdown.end] native_joined=1 analog_host_joined=1',
    '[component=main][event=session.end] exit_code=0'
) } elseif ($InjectAnalogHostChildCppFault) { @(
    '[component=analog-host][event=child.start]',
    'restart=0',
    '[component=analog-host][event=child.exit]',
    'code=0xE0484303',
    'restart=1',
    '[component=backend][event=shutdown.end] native_joined=1 analog_host_joined=1',
    '[component=main][event=session.end] exit_code=0'
) } elseif ($InjectAnalogHostChildReapTimeout) { @(
    '[component=analog-host][event=test.child_reap_timeout.injected] simulator_only=1',
    '[component=analog-host][event=child.reap_timeout]',
    'process_handle_retained=1 restart_blocked=1',
    '[component=analog-host][event=child.reaped_after_timeout]',
    '[component=backend][event=shutdown.end] native_joined=1 analog_host_joined=1',
    '[component=main][event=session.end] exit_code=0'
) } elseif ($InjectRealtimeStartFailure) { @(
    '[component=realtime][event=test.start_failure.injected] simulator_only=1',
    '[component=app][event=startup.rollback.begin] failed_stage=realtime',
    '[component=app][event=startup.rollback.step] component=realtime joined=1',
    '[component=app][event=startup.rollback.step] component=backend joined=1',
    '[component=app][event=startup.rollback.end] failed_stage=realtime restart_safe=1',
    '[component=main][event=session.end] exit_code=0'
) } elseif ($InjectNativePhaseStartFailure) { @(
    '[component=app][event=test.native_phase_start_failure.injected] phase=after_realtime simulator_only=1',
    '[component=app][event=startup.rollback.begin] failed_stage=native-after-realtime',
    '[component=app][event=startup.rollback.step] component=native-after-realtime joined=1',
    '[component=app][event=startup.rollback.step] component=realtime joined=1',
    '[component=app][event=startup.rollback.step] component=backend joined=1',
    '[component=app][event=startup.rollback.end] failed_stage=native-after-realtime restart_safe=1',
    '[component=main][event=session.end] exit_code=0'
) } else { @(
    '[component=analog-simulator][event=start]',
    'name=w-ramp',
    'name=opposing-ws',
    'name=opposing-ad',
    'name=diagonal',
    'name=disconnected',
    'name=reconnected',
    'name=post-reconnect-input',
    'name=source-fault',
    'name=recovered',
    '[event=pipeline-report.observed] phase=w-ramp',
    '[event=pipeline-report.observed] phase=opposing-ws',
    '[event=pipeline-report.observed] phase=opposing-ad',
    '[event=pipeline-report.observed] phase=diagonal',
    '[event=pipeline-report.observed] phase=disconnected lx=0 ly=0',
    '[event=pipeline-report.observed] phase=post-reconnect-input',
    '[event=pipeline-report.observed] phase=source-fault lx=0 ly=0',
    '[event=vigem-report.accepted] state=non-neutral',
    '[event=vigem-report.accepted] state=neutral-after-input',
    '[component=analog-simulator][event=stop]',
    '[component=vigem][event=init.ok]',
    '[component=backend][event=shutdown.end]'
) }
$missing = @($required | Where-Object { -not $traceText.Contains($_) })
if ($StartOverlay) {
    $overlayRequired = @(
        '[component=overlay][event=start.ok] port=18765',
        '[component=overlay][event=worker.start] port=18765',
        '[component=overlay][event=worker.exit] fault_kind=0',
        '[component=overlay][event=stop.end]'
    )
    $missing += @($overlayRequired | Where-Object { -not $traceText.Contains($_) })
}
if ($ForceUserUapRuntime -and -not $traceText.Contains('[component=embedded-uap][event=prepare.ok] location=user exact_resource_match=1 system_sdk_required=0')) {
    $missing += 'verified per-user private UAP fallback'
}
if ($missing.Count -ne 0) {
    throw "Simulator trace is incomplete. Missing: $($missing -join ', ')"
}
if ($InjectSparkShutdownRace) {
    $serviceStopIndex = $traceText.IndexOf('[component=spark][event=service.stop.begin]')
    $reconnectAfterStop = if ($serviceStopIndex -ge 0) {
        $traceText.IndexOf('[component=spark][event=hotplug.reconnect]', $serviceStopIndex)
    } else {
        -1
    }
    if ($serviceStopIndex -lt 0 -or $reconnectAfterStop -ge 0) {
        throw 'SparkLink reconnect after service stop was observed.'
    }
}
if ($InjectNativePhaseStartFailure) {
    $afterRealtimeStop = $traceText.IndexOf('[component=app][event=startup.rollback.step] component=native-after-realtime joined=1')
    $realtimeStop = $traceText.IndexOf('[component=app][event=startup.rollback.step] component=realtime joined=1')
    $backendStop = $traceText.IndexOf('[component=app][event=startup.rollback.step] component=backend joined=1')
    if ($afterRealtimeStop -lt 0 -or $realtimeStop -le $afterRealtimeStop -or $backendStop -le $realtimeStop) {
        throw 'Startup dependencies were not rolled back in reverse acquisition order.'
    }
}
if (-not $isFaultInjection) {
    # The simulator shares the aggregation pipeline with real devices. Assert the
    # SOCD-neutralized axis, while allowing the other axis to carry live input.
    if ($traceText -notmatch '\[event=pipeline-report\.observed\] phase=opposing-ws lx=-?\d+ ly=0 ') {
        throw 'Opposing W/S did not neutralize the vertical axis.'
    }
    if ($traceText -notmatch '\[event=pipeline-report\.observed\] phase=opposing-ad lx=0 ly=-?\d+ ') {
        throw 'Opposing A/D did not neutralize the horizontal axis.'
    }
}
if (-not $isFaultInjection -and $traceText -match '\[level=ERROR\]') {
    throw 'Simulator trace contains an ERROR event.'
}

$childDeadline = [DateTime]::UtcNow.AddSeconds(10)
do {
    $remaining = @(Get-Process -Name 'HallJoyV14Simulator' -ErrorAction SilentlyContinue)
    if ($remaining.Count -eq 0) { break }
    Start-Sleep -Milliseconds 100
} while ([DateTime]::UtcNow -lt $childDeadline)
if ($remaining.Count -ne 0) {
    throw 'A HallJoyV14Simulator process remained after shutdown.'
}

if ($InjectRealtimeStopTimeout) {
    Write-Host 'HallJoy realtime timeout containment scenario: PASS' -ForegroundColor Green
} elseif ($InjectDebugLogStopTimeout) {
    Write-Host 'HallJoy debug-log timeout containment scenario: PASS' -ForegroundColor Green
} elseif ($InjectOverlayStopTimeout) {
    Write-Host 'HallJoy overlay timeout containment scenario: PASS' -ForegroundColor Green
} elseif ($InjectSparkStopTimeout) {
    Write-Host 'HallJoy SparkLink timeout containment scenario: PASS' -ForegroundColor Green
} elseif ($InjectSparkShutdownRace) {
    Write-Host 'HallJoy SparkLink service-stop reconnect suppression scenario: PASS' -ForegroundColor Green
} elseif ($InjectSayoStopTimeout) {
    Write-Host 'HallJoy Sayo timeout containment scenario: PASS' -ForegroundColor Green
} elseif ($InjectSayoReaderCppFault) {
    Write-Host 'HallJoy Sayo reader C++ fault containment scenario: PASS' -ForegroundColor Green
} elseif ($InjectAnalogHostBridgeStopTimeout) {
    Write-Host 'HallJoy analog-host bridge timeout containment scenario: PASS' -ForegroundColor Green
} elseif ($InjectAnalogHostSupervisorStartFailure) {
    Write-Host 'HallJoy analog-host partial-start rollback scenario: PASS' -ForegroundColor Green
} elseif ($InjectAnalogHostSupervisorCppFault) {
    Write-Host 'HallJoy analog-host supervisor C++ fault containment scenario: PASS' -ForegroundColor Green
} elseif ($InjectAnalogHostChildCppFault) {
    Write-Host 'HallJoy analog-host child C++ fault restart scenario: PASS' -ForegroundColor Green
} elseif ($InjectAnalogHostChildReapTimeout) {
    Write-Host 'HallJoy analog-host child reap-timeout ownership scenario: PASS' -ForegroundColor Green
} elseif ($InjectRealtimeStartFailure) {
    Write-Host 'HallJoy realtime-start rollback scenario: PASS' -ForegroundColor Green
} elseif ($InjectNativePhaseStartFailure) {
    Write-Host 'HallJoy native-phase reverse rollback scenario: PASS' -ForegroundColor Green
} elseif ($StartOverlay) {
    Write-Host 'HallJoy overlay HTTP and cooperative shutdown scenario: PASS' -ForegroundColor Green
} else {
    Write-Host 'HallJoy analog simulator scenario: PASS' -ForegroundColor Green
}
Write-Host "Trace: $trace"
if ($isFaultInjection) {
    Write-Host 'Evidence classification: simulator lifecycle fault injection; NOT hardware verification.'
} else {
    Write-Host 'Evidence classification: common pipeline simulation; NOT hardware verification.'
}
