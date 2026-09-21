[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$candidateDir = Join-Path $root 'build\obj\DiagnosticCandidate\x64'
$objectDir = Join-Path $root 'build\obj\AjazzDiagnostic\x64'
$target = Join-Path $root 'build\bin\Release\x64\HallJoy.exe'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild x64 was not found.' }
& $msbuild $project /m /p:Configuration=Release /p:Platform=x64 /p:HallJoyMad68ProRNative=true /p:HallJoyAjazzDiagnostic=true /p:HallJoyAttackSharkProDiagnostic=false /p:HallJoyInputPathDiagnostic=false "/p:OutDir=$candidateDir\" "/p:IntDir=$objectDir\" /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'AJAZZ build failed; running HallJoy was not touched.' }
$candidate = Join-Path $candidateDir 'HallJoy.exe'
foreach ($check in @('--halljoy-shark-self-test', '--halljoy-mini60-self-test', '--halljoy-na87-native-self-test', '--halljoy-verify-embedded-vigem-installer')) {
    $process = Start-Process -FilePath $candidate -ArgumentList $check -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(30000)) { Stop-Process -InputObject $process -Force; throw "Candidate check timed out: $check" }
    if ($process.ExitCode -ne 0) { throw "Candidate check failed: $check ($($process.ExitCode))" }
    if ($check -eq '--halljoy-na87-native-self-test') {
        $log = Get-Content -LiteralPath (Join-Path $candidateDir 'HallJoy.log') -Raw
        if (-not $log.Contains('ajazz_log_route_self_test=PASS') -or -not $log.Contains('event=log_route.self_test') -or -not $log.Contains('key_code=250 samples=4095,2048,0')) {
            throw 'AJAZZ diagnostic log routing check failed.'
        }
    }
}
& (Join-Path $PSScriptRoot 'publish_halljoy_build.ps1') -CandidatePath $candidate -TargetPath $target
Get-FileHash -LiteralPath $target -Algorithm SHA256
