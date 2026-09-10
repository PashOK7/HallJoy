[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent $PSScriptRoot
$projectRoot = Join-Path $repo 'src\HallJoyProject'
$project = Join-Path $projectRoot 'HallJoy\HallJoy.vcxproj'
$compiled = Join-Path $projectRoot `
    '..\..\build\bin\DrunkDeerDiagnostic\Release\x64\HallJoy-DrunkDeer-Diagnostic.exe'
$delivery = Join-Path $repo 'build\packages\HallJoy-DrunkDeer-Diagnostic.exe'

foreach ($required in @(
    $project,
    (Join-Path $projectRoot '..\..\build\runtime\universal_analog_abiv0.dll'),
    (Join-Path $projectRoot '..\..\build\runtime\universal_analog_abiv1.dll'),
    (Join-Path $repo 'docs\v1.4\DRUNKDEER_DIAGNOSTIC.md')
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required diagnostic input is missing: $required"
    }
}

& python (Join-Path $repo 'tools\run_native_backend_checks.py') `
    --require-compiler
if ($LASTEXITCODE -ne 0) {
    throw "Native backend behavioral checks failed: $LASTEXITCODE"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path -LiteralPath $vswhere) {
    $msbuild = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
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

$buildOutput = @(& $msbuild $project /t:Rebuild /p:Configuration=Release `
    /p:Platform=x64 /p:HallJoyDrunkDeerDiagnostic=true /m 2>&1)
$buildExitCode = $LASTEXITCODE
$buildOutput | Out-Host
if ($buildExitCode -ne 0) {
    throw "DrunkDeer diagnostic build failed: $buildExitCode"
}
$warnings = @($buildOutput | Where-Object {
    [string]$_ -match ': warning (?:C|LNK)\d+:'
})
$unexpected = @($warnings | Where-Object {
    [string]$_ -notmatch 'ViGEmClient\.lib\(ViGEmClient\.obj\)\s*: warning LNK4099:'
})
if ($unexpected.Count -ne 0) {
    $unexpected | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    throw 'Unexpected diagnostic compiler/linker warnings were emitted.'
}
if (-not (Test-Path -LiteralPath $compiled -PathType Leaf)) {
    throw "Diagnostic executable was not produced: $compiled"
}

$bytes = [IO.File]::ReadAllBytes($compiled)
$ascii = [Text.Encoding]::ASCII.GetString($bytes)
$wide = [Text.Encoding]::Unicode.GetString($bytes)
foreach ($marker in @(
    'drunkdeer-matrix-b6-diagnostic'
)) {
    if (-not $ascii.Contains($marker)) {
        throw "Diagnostic executable is missing ASCII marker: $marker"
    }
}
foreach ($marker in @(
    '[drunkdeer.raw_cell.press]',
    '[drunkdeer.raw_cell.release]',
    '[drunkdeer.raw_cell.summary]',
    '[drunkdeer.report_headers.initial]',
    '[drunkdeer.rollup]',
    'digital.press',
    'digital.release',
    'payload.snapshot',
    'payload.offset_summary',
    'transport=per_frame_uap_transaction',
    'request_commands_per_frame=1',
    'matrix_chunks=59+59+8',
    'storage=direct_append',
    'critical_flush=1',
    'privacy_redaction=userprofile',
    'digital_mapping_dependency=0',
    'g65_antler_nav_v3',
    'WASD Only',
    'transport.recovered',
    'transport.reopen_required',
    'volume_control=event_aggregation',
    '04-B6-03-01'
)) {
    if (-not $wide.Contains($marker)) {
        throw "Diagnostic executable is missing telemetry marker: $marker"
    }
}
$bytes = $null
$ascii = $null
$wide = $null

New-Item -ItemType Directory -Path (Split-Path -Parent $delivery) -Force | Out-Null
Copy-Item -LiteralPath $compiled -Destination $delivery -Force
$hash = Get-FileHash -LiteralPath $delivery -Algorithm SHA256
if ($hash.Hash -ne (Get-FileHash -LiteralPath $compiled -Algorithm SHA256).Hash) {
    throw 'The delivered diagnostic EXE does not match the compiled EXE.'
}

foreach ($gate in @(
    'run_drunkdeer_diagnostic_smoke.ps1',
    'run_drunkdeer_log_growth_test.ps1',
    'run_drunkdeer_abnormal_exit_log_test.ps1'
)) {
    & (Join-Path $PSScriptRoot $gate) -ExePath $delivery
    if ($LASTEXITCODE -ne 0) {
        throw "DrunkDeer final-artifact gate failed: $gate ($LASTEXITCODE)"
    }
}

Write-Host "DrunkDeer diagnostic EXE: $delivery" -ForegroundColor Green
Write-Host "HallJoy-DrunkDeer-Diagnostic.exe SHA256: $($hash.Hash)" `
    -ForegroundColor Green
