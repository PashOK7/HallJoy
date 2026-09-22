# Experimental K4 HJO1 host integration; no forced logging or firmware flashing.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$candidateDir = Join-Path $root 'build\obj\ReleaseCandidate\x64'
$target = Join-Path $root 'build\bin\Release\x64\HallJoy.exe'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild x64 was not found.' }
# Compilation and linked-image checks never touch the installed EXE.
& $msbuild $project /m /p:Configuration=Release /p:Platform=x64 /p:HallJoyMad68ProRNative=true /p:HallJoyKeychronOnboardExperimental=true /p:HallJoyAttackSharkProDiagnostic=false /p:HallJoyInputPathDiagnostic=false "/p:OutDir=$candidateDir\" /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Release build failed; running HallJoy was not touched.' }
$candidate = Join-Path $candidateDir 'HallJoy.exe'
foreach ($check in @('--halljoy-shark-self-test', '--halljoy-mini60-self-test', '--halljoy-na87-native-self-test', '--halljoy-verify-embedded-vigem-installer')) {
    $process = Start-Process -FilePath $candidate -ArgumentList $check -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(30000)) { Stop-Process -InputObject $process -Force; throw "Candidate check timed out: $check" }
    if ($process.ExitCode -ne 0) { throw "Candidate check failed: $check ($($process.ExitCode)); running HallJoy was not touched." }
}
& (Join-Path $PSScriptRoot 'publish_halljoy_build.ps1') -CandidatePath $candidate -TargetPath $target
