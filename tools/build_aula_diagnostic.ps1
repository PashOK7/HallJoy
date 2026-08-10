[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent $PSScriptRoot
$projectRoot = Join-Path $repo 'src\HallJoyProject'
$project = Join-Path $projectRoot 'HallJoy\HallJoy.vcxproj'
$compiled = Join-Path $projectRoot 'x64\AulaAggressiveTrace\HallJoy.exe'
$package = Join-Path $repo 'build\aula-diagnostic'
$expectedPackage = [IO.Path]::GetFullPath((Join-Path $repo 'build\aula-diagnostic')).TrimEnd('\')

foreach ($required in @(
    $project,
    (Join-Path $projectRoot 'runtime\universal_analog_abiv0.dll'),
    (Join-Path $projectRoot 'runtime\universal_analog_abiv1.dll')
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required diagnostic input is missing: $required"
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path -LiteralPath $vswhere) {
    $msbuild = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
}
if (-not $msbuild) {
    $msbuild = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $msbuild) { throw 'MSBuild x64 was not found.' }

$buildOutput = @(& $msbuild $project /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:HallJoyMad68ProRNative=true /p:HallJoyDiagnostic=true /p:HallJoySingleLogDiagnostic=true /p:HallJoyAulaAggressiveTrace=true /m 2>&1)
$buildExitCode = $LASTEXITCODE
$buildOutput | Out-Host
if ($buildExitCode -ne 0) { throw "Aula diagnostic build failed: $buildExitCode" }
$warnings = @($buildOutput | Where-Object { [string]$_ -match ': warning (?:C|LNK)\d+:' })
$unexpected = @($warnings | Where-Object { [string]$_ -notmatch 'warning LNK4099:.*ViGEmClient\.pdb' })
if ($unexpected.Count -ne 0) {
    $unexpected | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    throw 'Unexpected diagnostic compiler/linker warnings were emitted.'
}
if (-not (Test-Path -LiteralPath $compiled -PathType Leaf)) {
    throw "Diagnostic executable was not produced: $compiled"
}
$diagnosticBytes = [IO.File]::ReadAllBytes($compiled)
$diagnosticWideStrings = [Text.Encoding]::Unicode.GetString($diagnosticBytes)
foreach ($requiredDiagnosticMarker in @(
    'matrix.health',
    'matrix.activity',
    'matrix.session_summary',
    'matrix.coverage',
    'ten_key_gate=1',
    'reconnect.success',
    'protocol.cancelled'
    '[aula.w669.raw]'
    '[aula.w669.telemetry]'
    '[aula.w669.diagnostic]'
    '[aula.w669.session_summary]'
    '[aula.w669.coverage]'
    'redragon_k673_br_81'
    '[mad68pr]'
)) {
    if (-not $diagnosticWideStrings.Contains($requiredDiagnosticMarker)) {
        throw "Diagnostic executable is missing telemetry marker: $requiredDiagnosticMarker"
    }
}
$diagnosticWideStrings = $null
$diagnosticAsciiStrings = [Text.Encoding]::ASCII.GetString($diagnosticBytes)
foreach ($requiredK673Identity in @(
    '7272BRHEXYXK673JCARGB'
    '7272UKHEXYXBJCARGB'
    '7272USHEXYXK673JCARGB'
)) {
    if (-not $diagnosticAsciiStrings.Contains($requiredK673Identity)) {
        throw "Diagnostic executable is missing K673 identity: $requiredK673Identity"
    }
}
$diagnosticAsciiStrings = $null
$diagnosticBytes = $null

if (Test-Path -LiteralPath $package) {
    $resolved = [IO.Path]::GetFullPath($package).TrimEnd('\')
    if (-not $resolved.Equals($expectedPackage, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe diagnostic package target: $resolved"
    }
    Get-ChildItem -LiteralPath $resolved -Force | Remove-Item -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $package -Force | Out-Null
}

Copy-Item -LiteralPath $compiled -Destination (Join-Path $package 'HallJoy.exe') -Force

$hash = Get-FileHash -LiteralPath (Join-Path $package 'HallJoy.exe') -Algorithm SHA256
Write-Host "Single-file Aula diagnostic: $(Join-Path $package 'HallJoy.exe')" -ForegroundColor Green
Write-Host "HallJoy.exe SHA256: $($hash.Hash)" -ForegroundColor Green
Write-Host 'Telemetry schema: Aula diagnostic v2 (5 s health, activity/10-key/coverage/reconnect).' -ForegroundColor Green
