[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent $PSScriptRoot
$projectRoot = Join-Path $repo 'src\HallJoyProject'
$project = Join-Path $projectRoot 'HallJoy\HallJoy.vcxproj'
$compiled = Join-Path $projectRoot `
    '..\..\build\bin\ProviderV2Qualification\Release\x64\HallJoy.exe'
$deliveryDir = Join-Path $repo 'build\packages\provider-v2-qualification'
$delivery = Join-Path $deliveryDir 'HallJoy.exe'

& python (Join-Path $repo 'tools\run_native_backend_checks.py') `
    --require-compiler
if ($LASTEXITCODE -ne 0) {
    throw "Native backend checks failed: $LASTEXITCODE"
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
    /p:Platform=x64 /p:HallJoyProviderV2Qualification=true `
    /p:HallJoyStabilityTrace=false /m 2>&1)
$buildExitCode = $LASTEXITCODE
$buildOutput | Out-Host
if ($buildExitCode -ne 0) {
    throw "Provider V2 qualification build failed: $buildExitCode"
}
$warnings = @($buildOutput | Where-Object {
    [string]$_ -match ': warning (?:C|LNK)\d+:'
})
$unexpected = @($warnings | Where-Object {
    [string]$_ -notmatch 'ViGEmClient\.lib\(ViGEmClient\.obj\)\s*: warning LNK4099:'
})
if ($unexpected.Count -ne 0) {
    $unexpected | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    throw 'Unexpected qualification compiler/linker warnings were emitted.'
}
if (-not (Test-Path -LiteralPath $compiled -PathType Leaf)) {
    throw "Qualification executable was not produced: $compiled"
}

$bytes = [IO.File]::ReadAllBytes($compiled)
$ascii = [Text.Encoding]::ASCII.GetString($bytes)
$wide = [Text.Encoding]::Unicode.GetString($bytes)
if (-not $wide.Contains('HallJoyProviderV2Qualification.txt')) {
    throw 'Qualification image is missing its UTF-16 report filename marker.'
}
foreach ($marker in @(
    'schema=2',
    'duration_is_informational=1',
    'policy_requires_every_configured_field_activation_and_release=1',
    'coverage_source=provider_v2_raw_gated_legacy_shadow',
    'activation_policy=provider_raw_at_least_0.5_and_shadow_non_neutral',
    'release_policy=latched_until_provider_raw_zero_and_shadow_neutral',
    'coverage_mask_layout=bit_(pad_index*7+field_index)',
    'production_route=provider_v2_authoritative',
    'legacy_dense_shadow_submitted_to_vigem=0'
)) {
    if (-not $ascii.Contains($marker)) {
        throw "Qualification image is missing marker: $marker"
    }
}
$bytes = $null
$ascii = $null
$wide = $null

$expectedDeliveryDir = [IO.Path]::GetFullPath(
    (Join-Path $repo 'build\packages\provider-v2-qualification')).TrimEnd('\')
if (Test-Path -LiteralPath $deliveryDir) {
    $resolvedDeliveryDir = [IO.Path]::GetFullPath($deliveryDir).TrimEnd('\')
    if (-not $resolvedDeliveryDir.Equals($expectedDeliveryDir,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe qualification cleanup target: $resolvedDeliveryDir"
    }
    Get-ChildItem -LiteralPath $resolvedDeliveryDir -Force |
        Remove-Item -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $deliveryDir -Force | Out-Null
}
Copy-Item -LiteralPath $compiled -Destination $delivery

$vigemSelfTest = Start-Process -FilePath $delivery `
    -ArgumentList '--halljoy-verify-embedded-vigem-installer' `
    -WindowStyle Hidden -Wait -PassThru
if ($vigemSelfTest.ExitCode -ne 0) {
    throw "Embedded ViGEm verification failed: $($vigemSelfTest.ExitCode)"
}

& (Join-Path $PSScriptRoot 'run_provider_v2_qualification_smoke.ps1') `
    -ExePath $delivery

$hash = Get-FileHash -LiteralPath $delivery -Algorithm SHA256
Write-Host "Provider V2 qualification EXE: $delivery" -ForegroundColor Green
Write-Host "HallJoy.exe SHA256: $($hash.Hash)" -ForegroundColor Green
