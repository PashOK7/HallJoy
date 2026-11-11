[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PythonPath,
    [Parameter(Mandatory = $true)][string]$NsisPath,
    [Parameter(Mandatory = $true)][string]$ExecutablePath,
    [Parameter(Mandatory = $true)][string]$RuntimePath
)
# Exercise the real packager in an isolated checkout; never execute HallJoy.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$sandbox = Join-Path $root ('build/tests/build entry ' + [Guid]::NewGuid().ToString('N'))
$checks = New-Object System.Collections.Generic.List[string]
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAILED: $Message" }
    $checks.Add($Message)
    Write-Host "PASS: $Message"
}
function Copy-Input([string]$Source, [string]$Relative) {
    $target = Join-Path $sandbox $Relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $target
}
foreach ($relative in @('BUILD.cmd', 'tools/build_windows_installer.ps1', 'tools/build_environment.ps1',
    'tools/package_windows_installer.ps1', 'tools/verify_embedded_licenses.py', 'tools/run_native_backend_checks.py',
    'tools/publish_windows_outputs.ps1', 'tools/publish_halljoy_build.ps1',
    'tools/close_project_halljoy.ps1', 'tools/halljoy_process_identity.ps1',
    'tools/installer/windows.nsi', 'tools/installer/INSTALLATION.txt', 'tools/installer/PRIVACY_AND_SECURITY.txt',
    'src/HallJoyProject/HallJoy/version.h', 'src/HallJoyProject/HallJoy/HallJoy.ico',
    'LICENSE', 'THIRD_PARTY_NOTICES.md', 'tools/dependency-lock.json')) {
    Copy-Input (Join-Path $root $relative) $relative
}
$PythonPath = (Get-Item -LiteralPath $PythonPath).FullName
$NsisPath = (Get-Item -LiteralPath $NsisPath).FullName
foreach ($path in @($sandbox, $PythonPath, $NsisPath)) {
    if ($path -match '["\r\n]') { throw 'Unsupported test argument path.' }
}
$runNumber = 0
function Run-Entry([string]$Extra = '') {
    $script:runNumber++
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $sandbox 'tools/build_windows_installer.ps1') +
        '" -PackageOnly -PythonPath "' + $PythonPath + '" -NsisPath "' + $NsisPath + '" ' + $Extra
    $process = Start-Process powershell.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait `
        -RedirectStandardOutput (Join-Path $sandbox "run-$script:runNumber.log") `
        -RedirectStandardError (Join-Path $sandbox "run-$script:runNumber.err")
    return $process.ExitCode
}
Assert ((Run-Entry '-CheckOnly') -ne 0) 'Package-only rejects a missing built executable'
Assert (-not (Test-Path (Join-Path $sandbox 'build/packages'))) 'Failed preflight creates no package directory'
Copy-Input $ExecutablePath 'build/bin/Release/x64/HallJoy.exe'
Copy-Input $RuntimePath 'build/runtime/universal_analog_abiv1.dll'
$exeHash = (Get-FileHash -LiteralPath (Join-Path $sandbox 'build/bin/Release/x64/HallJoy.exe')).Hash
Assert ((Run-Entry '-CheckOnly') -eq 0) 'Package-only preflight accepts existing tools and executable'
Assert (-not (Test-Path (Join-Path $sandbox 'build/packages'))) 'CheckOnly does not package an executable'

$runtime = Join-Path $sandbox 'build/runtime/universal_analog_abiv1.dll'
[IO.File]::WriteAllText($runtime, 'invalid runtime fixture')
Assert ((Run-Entry) -ne 0) 'Embedded-resource mismatch propagates as build-entry failure'
Assert (-not (Test-Path (Join-Path $sandbox 'build/packages'))) 'Failed resource validation publishes no installer'
Assert (@(Get-ChildItem -LiteralPath (Join-Path $sandbox 'build') -File).Count -eq 0) 'Failed packaging leaves no final files in build root'
Copy-Input $RuntimePath 'build/runtime/universal_analog_abiv1.dll'
Assert ((Run-Entry) -eq 0) 'Package-only builds the real Windows installer'
$packages = @(Get-ChildItem (Join-Path $sandbox 'build/packages') -Filter '*-setup.exe' -Recurse)
Assert ($packages.Count -eq 1) 'Exactly one installer is produced'
$package = $packages[0]
$hash = (Get-FileHash -LiteralPath $package.FullName).Hash.ToLowerInvariant()
$sidecar = (Get-Content -LiteralPath ($package.FullName + '.sha256') -Raw).Trim()
Assert ($sidecar -eq ($hash + '  ' + $package.Name)) 'SHA-256 sidecar matches the final installer'
$inventory = Get-Content -LiteralPath ($package.FullName + '.json') -Raw | ConvertFrom-Json
Assert ($inventory.sha256 -eq $hash -and $inventory.payload.Count -eq 7) 'Package inventory records the installer and public payload'
$finalApp = Join-Path $sandbox 'build/HallJoy.exe'
$finalSetup = Join-Path $sandbox 'build/HallJoy-setup.exe'
Assert ((Get-FileHash -LiteralPath $finalApp).Hash -eq $exeHash) 'Build-root application matches the packaged application'
Assert ((Get-FileHash -LiteralPath $finalSetup).Hash -eq $hash) 'Build-root installer matches the archived package'
Assert (@(Get-ChildItem -LiteralPath (Join-Path $sandbox 'build') -File).Count -eq 2) 'Only the two final executables are placed in build root'
# A corrupt manifest must not change either previously delivered executable.
$manifestPath = $package.FullName + '.json'
$manifestText = Get-Content -LiteralPath $manifestPath -Raw
$inventory.sha256 = ('0' * 64)
$inventory | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$rejected = $false
try { & (Join-Path $sandbox 'tools/publish_windows_outputs.ps1') -PackageDirectory $package.DirectoryName }
catch { $rejected = $_.Exception.Message -like '*hash mismatch*' }
Assert $rejected 'Corrupt package inventory is rejected before publishing'
Assert ((Get-FileHash -LiteralPath $finalApp).Hash -eq $exeHash -and (Get-FileHash -LiteralPath $finalSetup).Hash -eq $hash) 'Rejected package preserves both final executables'
$manifestText | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Assert ((Run-Entry ('-OutputDirectory "' + $package.DirectoryName + '"')) -ne 0) 'Existing output directory is rejected'
Assert ((Get-FileHash -LiteralPath $package.FullName).Hash.ToLowerInvariant() -eq $hash) 'Rejected output leaves the earlier installer intact'
Assert ((Run-Entry) -eq 0) 'A repeated build gets a new output directory'
Assert (@(Get-ChildItem (Join-Path $sandbox 'build/packages') -Filter '*-setup.exe' -Recurse).Count -eq 2) 'Both build results are retained'
Assert (@(Get-ChildItem -LiteralPath (Join-Path $sandbox 'build') -File).Count -eq 2) 'Repeated packaging replaces stable final filenames without clutter'
$hash = (Get-FileHash -LiteralPath $finalSetup).Hash
Assert ((Get-FileHash -LiteralPath (Join-Path $sandbox 'build/bin/Release/x64/HallJoy.exe')).Hash -eq $exeHash) 'Packaging does not modify the input executable'
# The default CMD must invoke the source stage, then package only on success.
# Keep the compiler stage controlled; real resource validation and NSIS packaging
# still run against the independently supplied, verified application bytes.
$sourceStub = Join-Path $sandbox 'tools/build.ps1'
$sourceMarker = Join-Path $sandbox 'source-stage-ran.txt'
$fullOutput = Join-Path $sandbox 'default build result'
function Run-DefaultEntry {
    $script:runNumber++
    $arguments = '/d /s /c ""' + (Join-Path $sandbox 'BUILD.cmd') + '" -PythonPath "' + $PythonPath +
        '" -NsisPath "' + $NsisPath + '" -OutputDirectory "' + $fullOutput + '""'
    $process = Start-Process cmd.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait `
        -RedirectStandardOutput (Join-Path $sandbox "run-$script:runNumber.log") `
        -RedirectStandardError (Join-Path $sandbox "run-$script:runNumber.err")
    return $process.ExitCode
}
@'
Set-Content -LiteralPath (Join-Path $PSScriptRoot '../source-stage-ran.txt') -Value 'called'
exit 23
'@ | Set-Content -LiteralPath $sourceStub -Encoding ASCII
Assert ((Run-DefaultEntry) -ne 0) 'Default BUILD.cmd propagates a source-stage failure'
Assert (Test-Path -LiteralPath $sourceMarker) 'Default BUILD.cmd invokes the source stage without PackageOnly'
Assert (-not (Test-Path -LiteralPath $fullOutput)) 'Source-stage failure does not package an older EXE'
Assert ((Get-FileHash -LiteralPath $finalApp).Hash -eq $exeHash -and (Get-FileHash -LiteralPath $finalSetup).Hash -eq $hash) 'Source-stage failure preserves the previous final files'
'exit 0' | Set-Content -LiteralPath $sourceStub -Encoding ASCII
Assert ((Run-DefaultEntry) -eq 0) 'Default BUILD.cmd packages after successful source-stage completion'
Assert (@(Get-ChildItem -LiteralPath $fullOutput -Filter '*-setup.exe').Count -eq 1) 'Default build result is one setup executable'
Assert ((Get-FileHash -LiteralPath $finalSetup).Hash -eq (Get-FileHash -LiteralPath @(Get-ChildItem -LiteralPath $fullOutput -Filter '*-setup.exe')[0].FullName).Hash) 'Default build publishes its exact installer to build root'
@{ checks = @($checks.ToArray()); applicationLaunched = $false; sourceCompiled = $false } |
    ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $sandbox 'results.json') -Encoding UTF8
Write-Output "RESULTS=$sandbox\results.json"
