[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$output = Join-Path $root 'src\HallJoyProject\x64\AnalogSimulator'
$exe = Join-Path $output 'HallJoyV14Simulator.exe'
$evidenceParent = Join-Path $root 'build\evidence\factory-reset'
$runRoot = Join-Path $evidenceParent (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
$dataRoot = Join-Path $runRoot 'data'
$legacyRoot = Join-Path $runRoot 'legacy-empty'
$summaryPath = Join-Path $runRoot 'summary.json'

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

function Invoke-Simulator([string[]]$ExtraArguments, [int]$ExpectedExitCode) {
    $arguments = @(
        '--halljoy-test-data-root', $dataRoot,
        '--halljoy-test-legacy-root', $legacyRoot
    ) + $ExtraArguments
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -Wait -PassThru
    $actual = $process.ExitCode
    if ($actual -ne $ExpectedExitCode) {
        throw "Simulator exit mismatch: expected=$ExpectedExitCode actual=$actual args=$($ExtraArguments -join ' ')"
    }
}

function Get-StateHashes([hashtable]$Fixtures) {
    $hashes = [ordered]@{}
    foreach ($relative in $Fixtures.Keys) {
        $path = Join-Path $dataRoot $relative
        $hashes[$relative] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
    return $hashes
}

function Assert-StateHashes([hashtable]$Expected) {
    foreach ($relative in $Expected.Keys) {
        $path = Join-Path $dataRoot $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "State file was not restored: $relative"
        }
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($actual -ne $Expected[$relative]) {
            throw "State hash changed during rollback: $relative"
        }
    }
}

if (-not $SkipBuild) {
    $msbuild = Find-MSBuild
    & $msbuild $project '/t:Rebuild' '/p:Configuration=Release' '/p:Platform=x64' `
        '/p:HallJoyAnalogSimulator=true' '/m'
    if ($LASTEXITCODE -ne 0) { throw "Simulator build failed: $LASTEXITCODE" }
}
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Simulator executable was not found: $exe"
}

New-Item -ItemType Directory -Path $dataRoot,$legacyRoot -Force | Out-Null
$fixtures = [ordered]@{
    'settings.ini' = "[Main]`r`nFactoryResetSentinel=SETTINGS`r`n"
    'bindings.ini' = "FACTORY_RESET_BINDINGS_SENTINEL`r`n"
    'GlobalProfiles\Factory Reset.settings.ini' = "[Profile]`r`nSentinel=GLOBAL_PROFILE`r`n"
    'Layouts\Factory Reset Layout.ini' = "[LayoutPreset]`r`nSentinel=LAYOUT`r`n"
    'CurvePresets\Factory Reset Curve.ini' = "[Curve]`r`nSentinel=CURVE`r`n"
}
foreach ($relative in $fixtures.Keys) {
    $path = Join-Path $dataRoot $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    [IO.File]::WriteAllText($path, $fixtures[$relative], [Text.UTF8Encoding]::new($false))
}
$unrelatedPath = Join-Path $dataRoot 'factory-reset-must-not-touch.txt'
$migrationMarker = Join-Path $dataRoot '.migration-from-exe-test.ini'
[IO.File]::WriteAllText($unrelatedPath, "UNRELATED_SENTINEL`r`n", [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($migrationMarker, "[Migration]`r`nComplete=1`r`n", [Text.UTF8Encoding]::new($false))
$unrelatedHash = (Get-FileHash -LiteralPath $unrelatedPath -Algorithm SHA256).Hash
$migrationHash = (Get-FileHash -LiteralPath $migrationMarker -Algorithm SHA256).Hash
# The first process writes and validates the atomic request. Normal HallJoy
# shutdown may canonicalize a sparse test INI, so the transaction boundary is
# captured after that normal save and immediately before reset application.
Invoke-Simulator @('--halljoy-test-factory-reset-request') 0
$requestPath = Join-Path $dataRoot '.factory-reset-request.ini'
if (-not (Test-Path -LiteralPath $requestPath -PathType Leaf)) {
    throw 'Factory reset request marker was not committed.'
}
$preApplyHashes = Get-StateHashes $fixtures

# Force a failure after three real moves and prove reverse rollback is byte-exact.
Invoke-Simulator @(
    '--halljoy-test-factory-reset-apply',
    '--halljoy-test-factory-reset-fail-after-three-moves'
) 1
if (-not (Test-Path -LiteralPath $requestPath -PathType Leaf)) {
    throw 'Failed reset consumed its retry marker.'
}
Assert-StateHashes $preApplyHashes
$failedBackupEntries = @(Get-ChildItem -LiteralPath (Join-Path $dataRoot 'FactoryResetBackups') -Force)
if ($failedBackupEntries.Count -ne 0) {
    throw "Rollback left an incomplete backup entry: $($failedBackupEntries.FullName -join ', ')"
}

# Retry without injection. This must commit a complete recoverable backup.
Invoke-Simulator @('--halljoy-test-factory-reset-apply') 0
if (Test-Path -LiteralPath $requestPath) {
    throw 'Successful reset left its request marker behind.'
}
foreach ($leaf in @('GlobalProfiles', 'Layouts', 'CurvePresets')) {
    $directory = Join-Path $dataRoot $leaf
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        throw "Successful reset did not create a fresh directory: $leaf"
    }
}
$activeState = @(
    Get-ChildItem -LiteralPath $dataRoot -File -Filter '*.ini'
    Get-ChildItem -LiteralPath (Join-Path $dataRoot 'GlobalProfiles') -Recurse -File
    Get-ChildItem -LiteralPath (Join-Path $dataRoot 'Layouts') -Recurse -File
    Get-ChildItem -LiteralPath (Join-Path $dataRoot 'CurvePresets') -Recurse -File
)
foreach ($file in $activeState) {
    $content = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    if ($content.Contains('FACTORY_RESET_') -or $content.Contains('FactoryResetSentinel')) {
        throw "Successful reset retained a test setting in active state: $($file.FullName)"
    }
}

$backupRoots = @(Get-ChildItem -LiteralPath (Join-Path $dataRoot 'FactoryResetBackups') -Directory -Filter 'reset-*')
if ($backupRoots.Count -ne 1) {
    throw "Expected one recoverable reset backup, found $($backupRoots.Count)."
}
foreach ($relative in $fixtures.Keys) {
    $backupPath = Join-Path $backupRoots[0].FullName $relative
    if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
        throw "Reset backup is missing: $relative"
    }
    if ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -ne $preApplyHashes[$relative]) {
        throw "Reset backup hash mismatch: $relative"
    }
}
if ((Get-FileHash -LiteralPath $unrelatedPath -Algorithm SHA256).Hash -ne $unrelatedHash -or
    (Get-FileHash -LiteralPath $migrationMarker -Algorithm SHA256).Hash -ne $migrationHash) {
    throw 'Factory reset changed an unrelated or migration-policy file.'
}

Start-Sleep -Milliseconds 250
$survivors = @(Get-Process -Name 'HallJoyV14Simulator' -ErrorAction SilentlyContinue)
if ($survivors.Count -ne 0) {
    throw "Simulator survivors after reset gate: $($survivors.Id -join ', ')"
}

$summary = [ordered]@{
    schema = 1
    status = 'PASS'
    request_atomic = $true
    partial_move_rollback = 'byte_exact'
    reset_targets = @('settings.ini', 'bindings.ini', 'GlobalProfiles', 'Layouts', 'CurvePresets')
    backup = $backupRoots[0].FullName
    unrelated_state_preserved = $true
    migration_marker_preserved = $true
    survivor_count = 0
}
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host "HallJoy recoverable factory reset request, rollback and commit: PASS" -ForegroundColor Green
Write-Host "Evidence: $summaryPath"
