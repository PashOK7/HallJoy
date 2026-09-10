[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$ExePath = [IO.Path]::GetFullPath($ExePath)
$output = Split-Path -Parent $ExePath
$report = Join-Path $output 'HallJoyProviderV2Qualification.txt'
$temporary = Join-Path $output 'HallJoyProviderV2Qualification.tmp'

foreach ($path in @($report, $temporary)) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Force
    }
}

& (Join-Path $PSScriptRoot 'run_production_smoke.ps1') `
    -ExePath $ExePath -RunSeconds 3
if (-not (Test-Path -LiteralPath $report -PathType Leaf)) {
    throw 'Qualification build did not create its single summary report.'
}
if (Test-Path -LiteralPath $temporary) {
    throw 'Qualification transaction left a temporary report behind.'
}

$text = Get-Content -LiteralPath $report -Raw
foreach ($required in @(
    'schema=2',
    'duration_is_informational=1',
    'finalized=1',
    'verdict=INCOMPLETE',
    'policy_requires_every_configured_field_activation_and_release=1',
    'coverage_source=provider_v2_raw_gated_legacy_shadow',
    'activation_policy=provider_raw_at_least_0.5_and_shadow_non_neutral',
    'release_policy=latched_until_provider_raw_zero_and_shadow_neutral',
    'coverage_mask_layout=bit_(pad_index*7+field_index)',
    'production_route=provider_v2_authoritative',
    'legacy_dense_shadow_submitted_to_vigem=0'
)) {
    if (-not $text.Contains($required)) {
        throw "Qualification report is missing: $required"
    }
}
if ($text.Contains('verdict=PASS')) {
    throw 'Three-second idle smoke produced a false qualification PASS.'
}

Write-Host 'Provider V2 qualification exact-artifact smoke: PASS' `
    -ForegroundColor Green
Write-Host 'Idle/short run verdict: INCOMPLETE; temporary file: absent' `
    -ForegroundColor Green
