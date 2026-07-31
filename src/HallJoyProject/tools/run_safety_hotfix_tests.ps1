param(
    [switch]$SkipCpp
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot

$python = Get-Command py -ErrorAction SilentlyContinue
if ($python) {
    & $python.Source -3 (Join-Path $PSScriptRoot 'validate_safety_hotfix.py')
} else {
    $python = Get-Command python -ErrorAction Stop
    & $python.Source (Join-Path $PSScriptRoot 'validate_safety_hotfix.py')
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipCpp) {
    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($cl) {
        $outDir = Join-Path $env:TEMP 'HallJoySafetyHotfixTests'
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null
        $source = Join-Path $ProjectRoot 'tests\hid_io_operation_lifecycle_test.cpp'
        $exe = Join-Path $outDir 'hid_io_operation_lifecycle_test.exe'
        & $cl.Source /nologo /std:c++20 /W4 /WX /EHsc $source /Fe:$exe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $exe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Host 'HidIoOperation lifecycle test: PASS'
    } else {
        Write-Warning 'cl.exe not found; portable C++ lifecycle test was skipped. Run from a Visual Studio Developer PowerShell.'
    }
}
