[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$noticeCheck = Join-Path $root 'tools\support_notice_catalog.py'
& python $noticeCheck
if ($LASTEXITCODE -ne 0) { throw 'Support notice catalog is stale.' }
$project = Join-Path $root 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$candidateDir = Join-Path $root 'build\obj\DiagnosticCandidate\x64'
$target = Join-Path $root 'build\bin\Release\x64\HallJoy.exe'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild x64 was not found.' }
# Compilation and linked-image checks never touch the installed EXE.
& $msbuild $project /m /p:Configuration=Release /p:Platform=x64 /p:HallJoyMad68ProRNative=true /p:HallJoyKeychronOnboardExperimental=true /p:HallJoyAttackSharkR85Diagnostic=true /p:HallJoyInputPathDiagnostic=false "/p:OutDir=$candidateDir\" "/p:IntDir=$root\build\obj\AttackSharkR85Diagnostic\x64\" /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Release build failed; running HallJoy was not touched.' }
$candidate = Join-Path $candidateDir 'HallJoy.exe'
foreach ($check in @('--halljoy-require-k4-onboard', '--halljoy-support-self-test', '--halljoy-shark-self-test', '--halljoy-mini60-self-test', '--halljoy-na87-native-self-test', '--halljoy-verify-embedded-vigem-installer')) {
    $process = Start-Process -FilePath $candidate -ArgumentList $check -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(30000)) { Stop-Process -InputObject $process -Force; throw "Candidate check timed out: $check" }
    if ($process.ExitCode -ne 0) { throw "Candidate check failed: $check ($($process.ExitCode)); running HallJoy was not touched." }
}
# Read the actual detailed log written by the same-image diagnostic route.
$process = Start-Process -FilePath $candidate -ArgumentList '--halljoy-r85-log-self-test' -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(30000)) { Stop-Process -InputObject $process -Force; throw 'R85 log test timed out.' }
if ($process.ExitCode -ne 0) { throw 'R85 log test failed.' }
$log = Get-Content -LiteralPath (Join-Path $candidateDir 'HallJoy.log') -Raw
foreach ($required in @('R85_LOG_SELF_TEST=PASS','SHARK_DELAY_TEST=PASS timer_failure_continues=1 cancellation_preserved=1','SHARK_OPEN_TEST=PASS real_win32_errors=32,2','expected_id=3123','r85_wire direction=rx command=143','bytes=008f330c','r85_wire direction=rx command=229','zero_depth_despite_digital_presses','r85_wire direction=tx command=143','bytes=008f','exclusive_open_failed error=32','get_feature_failed error=5','r85_identity_mismatch expected=3123 actual=9999','page_rejected page=2')) {
    if (-not $log.Contains($required)) { throw "R85 actual-file log lost evidence: $required" }
}
Write-Host 'R85_ACTUAL_LOG=PASS'
& (Join-Path $PSScriptRoot 'publish_halljoy_build.ps1') -CandidatePath $candidate -TargetPath $target
