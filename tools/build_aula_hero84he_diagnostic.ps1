param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$outDir = Join-Path $root 'build\bin\AulaHero84HeDiagnostic\Release\x64'
$exe = Join-Path $outDir 'HallJoy-AULA-HERO84HE-Diagnostic.exe'

Write-Host 'Running AULA HERO84 HE source and protocol checks...' -ForegroundColor Cyan
& python (Join-Path $root 'tools\run_native_backend_checks.py') --require-compiler
if ($LASTEXITCODE -ne 0) { throw "Native backend checks failed: $LASTEXITCODE" }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = if (Test-Path -LiteralPath $vswhere) { & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1 }
if (-not $msbuild) { $msbuild = @('C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe','C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe','C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe') | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1 }
if (-not $msbuild) { throw 'MSBuild x64 was not found.' }

& $msbuild $project /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:HallJoyAulaHero84HeDiagnostic=true /m
if ($LASTEXITCODE -ne 0) { throw "AULA HERO84 HE diagnostic build failed: $LASTEXITCODE" }
if (-not (Test-Path -LiteralPath $exe)) { throw "Diagnostic executable was not produced: $exe" }
$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
Write-Host "Built: $exe" -ForegroundColor Green
Write-Host "SHA256: $hash" -ForegroundColor Green
