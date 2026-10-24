[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$vigem = (Get-Content -LiteralPath (Join-Path $root 'tools/dependency-lock.json') -Raw | ConvertFrom-Json).binaryInputs.vigemClient
$vigemPath = Join-Path $root $vigem.path
if ((Get-Item -LiteralPath $vigemPath).Length -ne $vigem.size -or (Get-FileHash -LiteralPath $vigemPath -Algorithm SHA256).Hash -ne $vigem.sha256) {
    throw 'ViGEmClient binary differs from the reviewed dependency lock.'
}
& python (Join-Path $root 'tools/verify_uap_link_closure.py') --dll (Join-Path $root 'build/runtime/universal_analog_abiv1.dll') --map (Join-Path $root 'build/runtime/universal_analog_abiv1.map') --record (Join-Path $root 'build/runtime/universal_analog_abiv1.json')
if ($LASTEXITCODE -ne 0) { throw 'Embedded UAP provenance verification failed; run tools/build.ps1 to rebuild dependencies.' }
& python (Join-Path $root 'tools\audit_keyboard_identities.py')
if ($LASTEXITCODE -ne 0) { throw 'Keyboard identity evidence audit failed.' }
$noticeCheck = Join-Path $root 'tools\support_notice_catalog.py'
& python $noticeCheck
if ($LASTEXITCODE -ne 0) { throw 'Support notice catalog is stale.' }
& python (Join-Path $root 'tools\unsupported_keyboard_catalog.py')
if ($LASTEXITCODE -ne 0) { throw 'Unsupported keyboard catalog is stale.' }
& python (Join-Path $root 'tools/check_support_diagnostics.py')
if ($LASTEXITCODE -ne 0) { throw 'Support diagnostics contract gate failed; running HallJoy was not touched.' }
$project = Join-Path $root 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$candidateDir = Join-Path $root 'build\obj\ReleaseCandidate\x64'
$target = Join-Path $root 'build\bin\Release\x64\HallJoy.exe'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild x64 was not found.' }
# Compilation and linked-image checks never touch the installed EXE.
& $msbuild $project /m /p:Configuration=Release /p:Platform=x64 /p:HallJoyMad68ProRNative=true /p:HallJoyKeychronOnboardExperimental=true /p:HallJoyAttackSharkR85Diagnostic=false /p:HallJoyAttackSharkProDiagnostic=false /p:HallJoyAjazzDiagnostic=false /p:HallJoyInputPathDiagnostic=false "/p:OutDir=$candidateDir\" /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Release build failed; running HallJoy was not touched.' }
$candidate = Join-Path $candidateDir 'HallJoy.exe'
foreach ($check in @('--halljoy-require-full-catalog', '--halljoy-require-k4-onboard', '--halljoy-support-self-test', '--halljoy-shark-self-test', '--halljoy-mini60-self-test', '--halljoy-redsquare-probe-self-test', '--halljoy-na87-native-self-test', '--halljoy-verify-embedded-vigem-installer')) {
    $process = Start-Process -FilePath $candidate -ArgumentList $check -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(30000)) { Stop-Process -InputObject $process -Force; throw "Candidate check timed out: $check" }
    if ($process.ExitCode -ne 0) { throw "Candidate check failed: $check ($($process.ExitCode)); running HallJoy was not touched." }
}
& python (Join-Path $root 'tools/verify_embedded_licenses.py') $candidate
if ($LASTEXITCODE -ne 0) { throw 'Embedded legal resources verification failed.' }
# Rebuilds the simulator from the same sources and checks the compiled layout
# catalog, including "no Supported keyboard without a layout".
& (Join-Path $PSScriptRoot 'run_profile_transaction_tests.ps1')
if (-not $?) { throw 'Production-linked profile/layout tests failed; running HallJoy was not touched.' }
& (Join-Path $PSScriptRoot 'publish_halljoy_build.ps1') -CandidatePath $candidate -TargetPath $target
