[CmdletBinding()]
param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$exe = Join-Path $root 'build\bin\AnalogSimulator\Release\x64\HallJoyV14Simulator.exe'
if (-not $SkipBuild) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
    if (-not $msbuild) { throw 'MSBuild x64 was not found.' }
    & $msbuild $project /t:Build /p:Configuration=Release /p:Platform=x64 /p:HallJoyAnalogSimulator=true /m /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw 'Profile test simulator build failed.' }
}
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('HJProfileTest-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
# Start-Process joins arguments; quote generated paths even when TEMP contains spaces.
$quotedRoot = '"' + $testRoot + '"'
$process = Start-Process -FilePath $exe -ArgumentList @('--halljoy-test-data-root', $quotedRoot,
    '--halljoy-test-legacy-root', $quotedRoot, '--halljoy-test-forbid-backend-init',
    '--halljoy-test-profile-transactions') -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(30000)) {
    Stop-Process -Id $process.Id
    throw "Isolated profile test timed out. Evidence: $testRoot"
}
$result = Join-Path $testRoot 'profile-test-result.txt'
if (-not (Test-Path -LiteralPath $result)) { throw "Profile test produced no evidence: $testRoot" }
$evidence = Get-Content -LiteralPath $result -Raw
Write-Output $evidence
Write-Output "Evidence: $testRoot"
if ($process.ExitCode -ne 0 -or -not $evidence.Contains('PROFILE_TRANSACTION_WINDOWS_TEST=PASS')) {
    throw 'Production-linked profile tests failed.'
}


# Recovery must start successfully and preserve original files in a verified backup.
$badRoot = Join-Path $testRoot 'rejected-startup'
New-Item -ItemType Directory -Path $badRoot | Out-Null
$badSettings = Join-Path $badRoot 'settings.ini'
$badBindings = Join-Path $badRoot 'bindings.ini'
[IO.File]::WriteAllText($badSettings, "[Main]`r`nPollingMs=3`r`n")
[IO.File]::WriteAllText($badBindings, "[Pad1_Axes]`r`nLX_Plus=65543`r`n")
$beforeSettings = (Get-FileHash -LiteralPath $badSettings).Hash
$beforeBindings = (Get-FileHash -LiteralPath $badBindings).Hash
$quotedBadRoot = '"' + $badRoot + '"'
$rejected = Start-Process -FilePath $exe -ArgumentList @('--halljoy-test-data-root', $quotedBadRoot,
    '--halljoy-test-legacy-root', $quotedBadRoot, '--halljoy-test-forbid-backend-init',
    '--halljoy-test-profile-startup-only') -WindowStyle Hidden -PassThru
if (-not $rejected.WaitForExit(30000)) {
    Stop-Process -Id $rejected.Id
    throw 'Rejected startup did not exit.'
}
if ($rejected.ExitCode -ne 0 -or (Get-FileHash -LiteralPath $badBindings).Hash -ne $beforeBindings) {
    throw 'Recovery failed to start or changed original legacy bindings.'
}
$recoverySettings = @(Get-ChildItem -LiteralPath (Join-Path $badRoot '.internal/ProfileRecovery') -Recurse -Filter settings.ini)
if ($recoverySettings.Count -ne 1 -or (Get-FileHash $recoverySettings[0].FullName).Hash -ne $beforeSettings) {
    throw 'Original settings were not preserved exactly.'
}
Write-Output 'RECOVERY_STARTUP_PRESERVES_FILES=PASS'
& python (Join-Path $root 'tools/test_profile_startup_recovery.py') --exe $exe
if ($LASTEXITCODE -ne 0) { throw 'Startup recovery regression tests failed.' }
