param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$outDir = Join-Path $root 'build\bin\Titan68TurboExperimental\Release\x64'
$exe = Join-Path $outDir 'HallJoy-Madlions-Titan68-Turbo-Experimental.exe'

& python (Join-Path $root 'src\HallJoyProject\tests\titan68_turbo_experimental_static_audit.py')
if ($LASTEXITCODE -ne 0) { throw "Titan68 Turbo experimental static audit failed: $LASTEXITCODE" }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = if (Test-Path -LiteralPath $vswhere) { & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1 }
if (-not $msbuild) { $msbuild = @('C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe','C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe','C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe') | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1 }
if (-not $msbuild) { throw 'MSBuild x64 was not found.' }

& $msbuild $project /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:HallJoyTitan68TurboExperimental=true /p:UseMultiToolTask=false /m:1
if ($LASTEXITCODE -ne 0) { throw "Titan68 Turbo experimental build failed: $LASTEXITCODE" }
if (-not (Test-Path -LiteralPath $exe)) { throw "Experimental executable was not produced: $exe" }
$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
Write-Host "Built: $exe" -ForegroundColor Green
Write-Host "SHA256: $hash" -ForegroundColor Green
