[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$ForceUserUapRuntime,
    [switch]$StartOverlay,
    [switch]$InjectRealtimeStopTimeout,
    [switch]$InjectDebugLogStopTimeout,
    [switch]$InjectOverlayStopTimeout,
    [ValidateRange(7, 120)]
    [int]$RunSeconds = 8
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$injectionCount = @(
    $InjectRealtimeStopTimeout.IsPresent,
    $InjectDebugLogStopTimeout.IsPresent,
    $InjectOverlayStopTimeout.IsPresent
).Where({ $_ }).Count
if ($injectionCount -gt 1) {
    throw 'Select only one timeout injection scenario.'
}
if ($StartOverlay -and $injectionCount -ne 0) {
    throw 'StartOverlay cannot be combined with a timeout injection scenario.'
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
if ($StartOverlay) {
    $arguments += @('--overlay-server', '--port', '18765')
}
$process = Start-Process -FilePath $exe `
    -ArgumentList $arguments `
    -PassThru
try {
    if ($StartOverlay) {
        $overlayDeadline = [DateTime]::UtcNow.AddSeconds(5)
        $overlayResponse = $null
        do {
            try {
                $overlayResponse = Invoke-WebRequest -UseBasicParsing -Uri 'http://127.0.0.1:18765/state' -TimeoutSec 1
            }
            catch {
                Start-Sleep -Milliseconds 100
            }
        } while ($null -eq $overlayResponse -and [DateTime]::UtcNow -lt $overlayDeadline)
        if ($null -eq $overlayResponse -or $overlayResponse.StatusCode -ne 200) {
            throw 'Overlay /state endpoint did not return HTTP 200.'
        }
    }
    Start-Sleep -Seconds $RunSeconds
    if (-not $process.CloseMainWindow()) {
        throw 'Simulator main window did not accept a graceful close request.'
    }
    if (-not $process.WaitForExit(10000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw 'Simulator did not exit within 10 seconds after graceful close.'
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
if (-not $InjectRealtimeStopTimeout -and -not $InjectDebugLogStopTimeout -and -not $InjectOverlayStopTimeout -and $process.ExitCode -ne 0) {
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
    '[event=pipeline-report.observed] phase=opposing-ws lx=0 ly=0',
    '[event=pipeline-report.observed] phase=opposing-ad lx=0 ly=0',
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
if (-not $InjectRealtimeStopTimeout -and -not $InjectDebugLogStopTimeout -and -not $InjectOverlayStopTimeout -and $traceText -match '\[level=ERROR\]') {
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
} elseif ($StartOverlay) {
    Write-Host 'HallJoy overlay HTTP and cooperative shutdown scenario: PASS' -ForegroundColor Green
} else {
    Write-Host 'HallJoy analog simulator scenario: PASS' -ForegroundColor Green
}
Write-Host "Trace: $trace"
if ($InjectRealtimeStopTimeout -or $InjectDebugLogStopTimeout -or $InjectOverlayStopTimeout) {
    Write-Host 'Evidence classification: simulator lifecycle fault injection; NOT hardware verification.'
} else {
    Write-Host 'Evidence classification: common pipeline simulation; NOT hardware verification.'
}
