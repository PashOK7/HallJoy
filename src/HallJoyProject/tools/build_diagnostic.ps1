param(
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
$project = Join-Path $repo "HallJoy\HallJoy.vcxproj"
$exe = Join-Path $repo "x64\Diagnostic\HallJoyDiagnostic.exe"

if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild not found: $msbuild"
}

& $msbuild $project "/p:Configuration=Release" "/p:Platform=$Platform" "/p:HallJoyDiagnostic=true" /m
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $exe)) {
    throw "Diagnostic executable not found: $exe"
}

Write-Host ""
Write-Host "Diagnostic executable:"
Write-Host "  $exe"
Write-Host ""
Write-Host "Give the tester HallJoyDiagnostic.exe. After a crash ask for:"
Write-Host "  HallJoyDiagnostic.log"
Write-Host "  HallJoyDiagnosticCrash.txt"
Write-Host "  HallJoyDiagnosticExit.txt"
