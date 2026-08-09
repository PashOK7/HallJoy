[CmdletBinding()]
param(
    [ValidateRange(7, 30)]
    [int]$RunSeconds = 7
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'src\HallJoyProject\x64\AnalogSimulator'
$runner = Join-Path $root 'tools\run_analog_simulator.ps1'
$trace = Join-Path $output 'HallJoyStabilityTrace.log'
$runsRoot = Join-Path $output 'StorageMigrationRuns'
$runRoot = Join-Path $runsRoot ([Guid]::NewGuid().ToString('N'))
$legacy = Join-Path $runRoot 'legacy'
$target = Join-Path $runRoot 'local'

New-Item -ItemType Directory -Path $legacy,$target -Force | Out-Null
$decomposedCafe = "Cafe$([char]0x0301)"
$fixtures = [ordered]@{
    'settings.ini' = "[Main]`r`nActiveGlobalProfile=Default`r`n"
    'bindings.ini' = "HALLJOY_MIGRATION_BINDINGS`r`n"
    'GlobalProfiles\Café.settings.ini' = "[Profile]`r`nName=Café`r`n"
    'GlobalProfiles\Café.bindings.ini' = "HALLJOY_MIGRATION_PROFILE_BINDINGS`r`n"
    'Layouts\Legacy Collision.ini' = "LEGACY_LAYOUT_SOURCE`r`n"
    'Layouts\Migration Layout.ini' = "[LayoutPreset]`r`nCount=0`r`n"
    ("CurvePresets\$decomposedCafe.ini") = "[Curve]`r`nLow=17`r`n"
    'CurvePresets\_preset_state.ini' = "[UI]`r`nActiveName=`r`n"
}

foreach ($relative in $fixtures.Keys) {
    $path = Join-Path $legacy $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    [IO.File]::WriteAllText($path, $fixtures[$relative], [Text.UTF8Encoding]::new($false))
}

$collisionRelative = 'Layouts\Legacy Collision.ini'
$collisionPath = Join-Path $target $collisionRelative
New-Item -ItemType Directory -Path (Split-Path -Parent $collisionPath) -Force | Out-Null
[IO.File]::WriteAllText($collisionPath, "TARGET_WINS`r`n", [Text.UTF8Encoding]::new($false))
$collisionHash = (Get-FileHash -LiteralPath $collisionPath -Algorithm SHA256).Hash

$sourceHashes = @{}
foreach ($relative in $fixtures.Keys) {
    $sourceHashes[$relative] = (Get-FileHash -LiteralPath (Join-Path $legacy $relative) -Algorithm SHA256).Hash
}

& powershell -NoProfile -ExecutionPolicy Bypass -File $runner `
    -SkipBuild `
    -StorageDataRoot $target `
    -StorageLegacyRoot $legacy `
    -RequireStorageMigration `
    -RunSeconds $RunSeconds
if ($LASTEXITCODE -ne 0) { throw "Migration simulator failed: $LASTEXITCODE" }

foreach ($relative in $fixtures.Keys) {
    $after = (Get-FileHash -LiteralPath (Join-Path $legacy $relative) -Algorithm SHA256).Hash
    if ($after -ne $sourceHashes[$relative]) {
        throw "Migration changed legacy source: $relative"
    }
}
if ((Get-FileHash -LiteralPath $collisionPath -Algorithm SHA256).Hash -ne $collisionHash) {
    throw 'Migration overwrote a pre-existing destination file.'
}

$backupRoots = @(Get-ChildItem -LiteralPath (Join-Path $target 'MigrationBackups') -Directory -Filter 'legacy-*')
if ($backupRoots.Count -ne 1) {
    throw "Expected one legacy backup directory, found $($backupRoots.Count)."
}
foreach ($relative in $fixtures.Keys) {
    $backupPath = Join-Path $backupRoots[0].FullName $relative
    if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
        throw "Migration backup is missing: $relative"
    }
    if ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -ne $sourceHashes[$relative]) {
        throw "Migration backup hash mismatch: $relative"
    }
}

$markers = @(Get-ChildItem -LiteralPath $target -File -Filter '.migration-from-exe-*.ini')
if ($markers.Count -ne 1) {
    throw "Expected one completed migration marker, found $($markers.Count)."
}
$markerText = Get-Content -LiteralPath $markers[0].FullName -Raw -Encoding UTF8
if (-not $markerText.Contains('Complete=1')) {
    throw 'Migration marker is not complete.'
}
$temps = @(Get-ChildItem -LiteralPath $target -Recurse -File -Filter '*.halljoy-new-*')
if ($temps.Count -ne 0) {
    throw "Migration left transaction temp files: $($temps.FullName -join ', ')"
}

# A second launch must consume the marker and skip the one-time migration.
& powershell -NoProfile -ExecutionPolicy Bypass -File $runner `
    -SkipBuild `
    -StorageDataRoot $target `
    -StorageLegacyRoot $legacy `
    -RunSeconds $RunSeconds
if ($LASTEXITCODE -ne 0) { throw "Migration replay simulator failed: $LASTEXITCODE" }
$secondTrace = Get-Content -LiteralPath $trace -Raw -Encoding UTF8
if (-not $secondTrace.Contains('[component=storage][event=migration.skip] reason=complete')) {
    throw 'Second launch did not skip the completed one-time migration.'
}

# Every transactional stage must abort startup without changing the source,
# committing the destination, or leaving a temporary file behind.
foreach ($stage in @('prepare', 'write', 'flush', 'validate', 'replace')) {
    $faultLegacy = Join-Path $runRoot "fault-$stage\legacy"
    $faultTarget = Join-Path $runRoot "fault-$stage\local"
    New-Item -ItemType Directory -Path $faultLegacy,$faultTarget -Force | Out-Null
    $faultSource = Join-Path $faultLegacy 'settings.ini'
    [IO.File]::WriteAllText(
        $faultSource,
        "[Main]`r`nMigrationFaultStage=$stage`r`n",
        [Text.UTF8Encoding]::new($false))
    $faultSourceHash = (Get-FileHash -LiteralPath $faultSource -Algorithm SHA256).Hash

    & powershell -NoProfile -ExecutionPolicy Bypass -File $runner `
        -SkipBuild `
        -StorageDataRoot $faultTarget `
        -StorageLegacyRoot $faultLegacy `
        -RequireStorageMigrationFailure `
        -InjectPersistenceFailure $stage `
        -RunSeconds $RunSeconds
    if ($LASTEXITCODE -ne 0) { throw "Migration $stage fault simulator failed: $LASTEXITCODE" }

    if ((Get-FileHash -LiteralPath $faultSource -Algorithm SHA256).Hash -ne $faultSourceHash) {
        throw "Migration $stage fault changed the legacy source."
    }
    if (Test-Path -LiteralPath (Join-Path $faultTarget 'settings.ini') -PathType Leaf) {
        throw "Migration $stage fault committed the destination settings.ini."
    }
    $faultTemps = @(Get-ChildItem -LiteralPath $faultTarget -Recurse -File -Filter '*.halljoy-new-*')
    if ($faultTemps.Count -ne 0) {
        throw "Migration $stage fault left transaction temp files: $($faultTemps.FullName -join ', ')"
    }
}

# Exercise real marker-selected portable mode beside the simulator executable.
$portableMarker = Join-Path $output 'HallJoy.portable'
$createdPortableMarker = -not (Test-Path -LiteralPath $portableMarker)
if ($createdPortableMarker) {
    [IO.File]::WriteAllText($portableMarker, "HallJoy portable mode`r`n", [Text.UTF8Encoding]::new($false))
}
try {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $runner `
        -SkipBuild `
        -UsePortableStorage `
        -RunSeconds $RunSeconds
    if ($LASTEXITCODE -ne 0) { throw "Portable-mode simulator failed: $LASTEXITCODE" }
}
finally {
    if ($createdPortableMarker -and (Test-Path -LiteralPath $portableMarker)) {
        Remove-Item -LiteralPath $portableMarker -Force
    }
}

$resolvedRunsRoot = [IO.Path]::GetFullPath($runsRoot).TrimEnd('\') + '\'
$resolvedRunRoot = [IO.Path]::GetFullPath($runRoot)
if (-not $resolvedRunRoot.StartsWith($resolvedRunsRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe storage-test cleanup target: $resolvedRunRoot"
}
Remove-Item -LiteralPath $resolvedRunRoot -Recurse -Force

Write-Host 'HallJoy storage migration, five-stage atomic failure, replay, portable marker and filename policy scenarios: PASS' -ForegroundColor Green
