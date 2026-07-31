[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [ValidateRange(7, 120)]
    [int]$RunSeconds = 8
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

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

$process = Start-Process -FilePath $exe `
    -ArgumentList '--halljoy-simulate-analog=script' `
    -PassThru
try {
    Start-Sleep -Seconds $RunSeconds
    if (-not $process.CloseMainWindow()) {
        throw 'Simulator main window did not accept a graceful close request.'
    }
    if (-not $process.WaitForExit(10000)) {
        throw 'Simulator did not exit within 10 seconds after graceful close.'
    }
}
finally {
    $process.Refresh()
}

if ($process.ExitCode -ne 0) {
    throw "Simulator exited with code $($process.ExitCode)."
}
if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
    throw "Simulator trace was not produced: $trace"
}

$traceText = Get-Content -LiteralPath $trace -Raw -Encoding UTF8
$required = @(
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
)
$missing = @($required | Where-Object { -not $traceText.Contains($_) })
if ($missing.Count -ne 0) {
    throw "Simulator trace is incomplete. Missing: $($missing -join ', ')"
}
if ($traceText -match '\[level=ERROR\]') {
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

Write-Host 'HallJoy analog simulator scenario: PASS' -ForegroundColor Green
Write-Host "Trace: $trace"
Write-Host 'Evidence classification: common pipeline simulation; NOT hardware verification.'
