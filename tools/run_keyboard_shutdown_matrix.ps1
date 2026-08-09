[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [ValidateRange(7, 120)]
    [int]$RunSeconds = 7,
    [string]$EvidenceRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$scenarioRunner = Join-Path $PSScriptRoot 'run_analog_simulator.ps1'
$simulatorOutput = Join-Path $root 'src\HallJoyProject\x64\AnalogSimulator'
$tracePath = Join-Path $simulatorOutput 'HallJoyStabilityTrace.log'
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
    $EvidenceRoot = Join-Path $root "build\evidence\keyboard-shutdown-matrix\$stamp"
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null

# Every production keyboard route appears exactly once. UAP/Soup is one shared
# isolated-host boundary for all plugin-backed keyboards, including MAD68 HE.
$scenarios = @(
    [pscustomobject]@{ Name = 'normal';             Switch = $null;                         Route = 'common pipeline';             ExpectedAppExit = 0 },
    [pscustomobject]@{ Name = 'sparklink';          Switch = '-InjectSparkStopTimeout';     Route = 'native SparkLink/Irok';       ExpectedAppExit = 2 },
    [pscustomobject]@{ Name = 'sayo';               Switch = '-InjectSayoStopTimeout';      Route = 'native Sayo';                 ExpectedAppExit = 2 },
    [pscustomobject]@{ Name = 'addressed';          Switch = '-InjectAddressedStopTimeout'; Route = 'native Addressed';            ExpectedAppExit = 2 },
    [pscustomobject]@{ Name = 'hex80';              Switch = '-InjectHex80StopTimeout';     Route = 'native Hex80';                ExpectedAppExit = 2 },
    [pscustomobject]@{ Name = 'mad68-pro-r';        Switch = '-InjectMad68StopTimeout';     Route = 'native MAD68 Pro R A0';       ExpectedAppExit = 2 },
    [pscustomobject]@{ Name = 'aula-win60he';       Switch = '-InjectAulaStopTimeout';      Route = 'native Aula WIN60HE';         ExpectedAppExit = 2 },
    [pscustomobject]@{ Name = 'uap-soup';           Switch = '-InjectAnalogHostChildStopHang'; Route = 'all UAP/Soup keyboards';  ExpectedAppExit = 0 },
    [pscustomobject]@{ Name = 'process-watchdog';   Switch = '-InjectMad68OwnerStopHang';   Route = 'all routes/final teardown';   ExpectedAppExit = 4 }
)

$results = @()
$buildRequired = -not $SkipBuild
foreach ($scenario in $scenarios) {
    $arguments = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $scenarioRunner,
        '-RunSeconds', [string]$RunSeconds
    )
    if (-not $buildRequired) {
        $arguments += '-SkipBuild'
    }
    if ($scenario.Switch) {
        $arguments += $scenario.Switch
    }

    $started = [DateTime]::UtcNow
    & powershell.exe @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Keyboard shutdown scenario '$($scenario.Name)' failed with runner exit $LASTEXITCODE."
    }
    $buildRequired = $false

    if (-not (Test-Path -LiteralPath $tracePath -PathType Leaf)) {
        throw "Keyboard shutdown scenario '$($scenario.Name)' produced no stability trace."
    }
    $destination = Join-Path $EvidenceRoot "$($scenario.Name).log"
    Copy-Item -LiteralPath $tracePath -Destination $destination -Force

    $survivors = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -eq 'HallJoy' -or $_.ProcessName -eq 'HallJoyV14Simulator'
    })
    if ($survivors.Count -ne 0) {
        $ids = ($survivors | ForEach-Object { $_.Id }) -join ','
        throw "Keyboard shutdown scenario '$($scenario.Name)' left process IDs: $ids"
    }

    $results += [pscustomobject]@{
        name = $scenario.Name
        route = $scenario.Route
        injection = if ($scenario.Switch) { $scenario.Switch.TrimStart('-') } else { 'none' }
        expected_app_exit = $scenario.ExpectedAppExit
        elapsed_ms = [int]([DateTime]::UtcNow - $started).TotalMilliseconds
        trace = [IO.Path]::GetFileName($destination)
        trace_sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        survivors = 0
        classification = if ($scenario.Name -eq 'normal') {
            'common pipeline simulation; not hardware verification'
        } else {
            'lifecycle fault injection; not hardware verification'
        }
    }
    Write-Host "Keyboard shutdown matrix $($scenario.Name): PASS" -ForegroundColor Green
}

$summary = [ordered]@{
    schema = 1
    status = 'passed'
    completed_utc = [DateTime]::UtcNow.ToString('o')
    scenarios_requested = $scenarios.Count
    scenarios_passed = $results.Count
    simulator_only = $true
    hardware_verified = $false
    evidence_root = $EvidenceRoot
    results = $results
}
$summaryPath = Join-Path $EvidenceRoot 'summary.json'
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "HallJoy all-keyboard shutdown matrix: PASS ($($results.Count)/$($scenarios.Count))" -ForegroundColor Green
Write-Host 'Evidence classification: simulator lifecycle coverage; NOT hardware verification.'
Write-Host "Evidence: $EvidenceRoot"
Write-Host "Summary: $summaryPath"
