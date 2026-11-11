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
foreach ($relative in @('Build-Windows.cmd', 'tools/build_windows_installer.ps1',
    'tools/package_windows_installer.ps1', 'tools/verify_embedded_licenses.py',
    'tools/installer/windows.nsi', 'tools/installer/INSTALLATION.txt',
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
Copy-Input $RuntimePath 'build/runtime/universal_analog_abiv1.dll'
Assert ((Run-Entry) -eq 0) 'Package-only builds the real Windows installer'
$packages = @(Get-ChildItem (Join-Path $sandbox 'build/packages') -Filter '*-setup.exe' -Recurse)
Assert ($packages.Count -eq 1) 'Exactly one installer is produced'
$package = $packages[0]
$hash = (Get-FileHash -LiteralPath $package.FullName).Hash.ToLowerInvariant()
$sidecar = (Get-Content -LiteralPath ($package.FullName + '.sha256') -Raw).Trim()
Assert ($sidecar -eq ($hash + '  ' + $package.Name)) 'SHA-256 sidecar matches the final installer'
$inventory = Get-Content -LiteralPath ($package.FullName + '.json') -Raw | ConvertFrom-Json
Assert ($inventory.sha256 -eq $hash -and $inventory.payload.Count -eq 6) 'Package inventory records the installer and public payload'
Assert ((Run-Entry ('-OutputDirectory "' + $package.DirectoryName + '"')) -ne 0) 'Existing output directory is rejected'
Assert ((Get-FileHash -LiteralPath $package.FullName).Hash.ToLowerInvariant() -eq $hash) 'Rejected output leaves the earlier installer intact'
Assert ((Run-Entry) -eq 0) 'A repeated build gets a new output directory'
Assert (@(Get-ChildItem (Join-Path $sandbox 'build/packages') -Filter '*-setup.exe' -Recurse).Count -eq 2) 'Both build results are retained'
Assert ((Get-FileHash -LiteralPath (Join-Path $sandbox 'build/bin/Release/x64/HallJoy.exe')).Hash -eq $exeHash) 'Packaging does not modify the input executable'
@{ checks = @($checks.ToArray()); applicationLaunched = $false; sourceCompiled = $false } |
    ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $sandbox 'results.json') -Encoding UTF8
Write-Output "RESULTS=$sandbox\results.json"
