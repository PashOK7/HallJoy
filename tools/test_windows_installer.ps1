[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PayloadPath,
    [Parameter(Mandatory = $true)][string]$NsisPath
)
# Compile the production recipe with isolated files, shortcuts and HKCU registration.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$payload = (Get-Item -LiteralPath $PayloadPath).FullName
$version = (Get-Item -LiteralPath (Join-Path $payload 'HallJoy.exe')).VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw 'Unexpected test payload version.' }
$id = [Guid]::NewGuid().ToString('N')
$sandbox = Join-Path $root ('build/tests/windows installer ' + $id)
foreach ($path in @($payload, $sandbox)) {
    if ($path -match '[\$"\r\n]') { throw 'Unsupported NSIS test path.' }
}
New-Item -ItemType Directory -Path $sandbox -Force | Out-Null
$exe = Join-Path $sandbox 'HallJoy-test-setup.exe'
$installed = Join-Path $sandbox 'Install'
$registry = 'HKCU:\Software\HallJoyInstallerTests\' + $id
$checks = New-Object System.Collections.Generic.List[string]
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAILED: $Message" }
    $checks.Add($Message)
    Write-Host "PASS: $Message"
}
function Hash([string]$File) { (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash }
function Run-Setup([string]$Destination) {
    $process = Start-Process -FilePath $exe -ArgumentList ('/S /D=' + $Destination) -WindowStyle Hidden -Wait -PassThru
    return $process.ExitCode
}
function Uninstall {
    $process = Start-Process -FilePath "$installed\Uninstall.exe" -ArgumentList '/S' -WindowStyle Hidden -Wait -PassThru
    Assert ($process.ExitCode -eq 0) 'Test uninstaller starts successfully'
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while ((Test-Path -LiteralPath "$installed\Uninstall.exe") -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 200 }
    Assert (-not (Test-Path -LiteralPath "$installed\Uninstall.exe")) 'Uninstaller finishes and removes itself'
}
$names = @('HallJoy.exe', 'HallJoy.ico', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'dependency-lock.json', 'INSTALLATION.txt')
$expected = @{}
foreach ($name in $names) { $expected[$name] = Hash (Join-Path $payload $name) }
& $NsisPath /V2 /WX "/DPAYLOAD=$payload" "/DOUTPUT=$exe" "/DVERSION=$version" "/DTEST_ROOT=$sandbox" "/DTEST_ID=$id" /DTEST_DESKTOP (Join-Path $PSScriptRoot 'installer/windows.nsi')
if ($LASTEXITCODE -ne 0) { throw 'Test installer compilation failed.' }

$foreign = Join-Path $sandbox 'Unrelated portable copy'
New-Item -ItemType Directory -Path $foreign | Out-Null
'keep me' | Set-Content -LiteralPath (Join-Path $foreign 'settings.ini')
Assert ((Run-Setup $foreign) -eq 2) 'Nonempty unrelated folder is rejected'
Assert (@(Get-ChildItem -LiteralPath $foreign -Force).Count -eq 1) 'Rejected folder receives no files'
Assert (-not (Test-Path -LiteralPath $registry)) 'Rejected destination creates no registration'
New-Item -ItemType Directory -Path (Join-Path $sandbox 'Desktop') | Out-Null
$desktopShortcut = Join-Path $sandbox 'Desktop/HallJoy.lnk'
'unrelated shortcut' | Set-Content -LiteralPath $desktopShortcut
$foreignHash = Hash $desktopShortcut
Assert ((Run-Setup $installed) -eq 0) 'Installation succeeds in a path with spaces'
foreach ($name in $names) { Assert ((Hash (Join-Path $installed $name)) -eq $expected[$name]) ("Installed bytes: $name") }
Assert (-not (Test-Path -LiteralPath "$installed\HallJoy.portable")) 'No portable marker is installed'
Assert (-not (Test-Path -LiteralPath "$installed\settings.ini")) 'No personal settings are bundled'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut((Join-Path $sandbox 'Start Menu/HallJoy/HallJoy.lnk'))
Assert ($shortcut.TargetPath -eq "$installed\HallJoy.exe") 'Start menu shortcut targets the installed app'
Assert ($shortcut.IconLocation.StartsWith("$installed\HallJoy.ico", [StringComparison]::OrdinalIgnoreCase)) 'Shortcut uses the existing HallJoy icon'
Assert ((Hash $desktopShortcut) -eq $foreignHash) 'Selected desktop component preserves a foreign shortcut'
$registration = Get-ItemProperty -LiteralPath $registry
Assert ($registration.InstallLocation -eq $installed -and $registration.DisplayVersion -eq $version) 'Registration contains path and version'
Assert ($registration.UninstallString -eq ('"' + $installed + '\Uninstall.exe"')) 'Uninstall command is quoted'
Assert ((Run-Setup (Join-Path $sandbox 'Second install')) -eq 5) 'A second installation location is rejected'
Assert (-not (Test-Path -LiteralPath (Join-Path $sandbox 'Second install'))) 'Rejected second installation creates no directory'

'private settings' | Set-Content -LiteralPath "$installed\settings.ini"
New-Item -ItemType Directory -Path "$installed\GlobalProfiles" | Out-Null
'private profile' | Set-Content -LiteralPath "$installed\GlobalProfiles\keep.ini"
$settingsHash = Hash "$installed\settings.ini"
$profileHash = Hash "$installed\GlobalProfiles\keep.ini"
$lock = [IO.File]::Open("$installed\HallJoy.exe", [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
try { Assert ((Run-Setup $installed) -eq 3) 'Locked executable blocks replacement' } finally { $lock.Dispose() }
Assert ((Hash "$installed\HallJoy.exe") -eq $expected['HallJoy.exe']) 'Rejected replacement leaves the EXE intact'
Assert ((Run-Setup $installed) -eq 0) 'Reinstallation succeeds'
Assert ((Hash "$installed\settings.ini") -eq $settingsHash -and (Hash "$installed\GlobalProfiles\keep.ini") -eq $profileHash) 'Reinstallation preserves user files'
Uninstall
foreach ($name in $names) { Assert (-not (Test-Path -LiteralPath (Join-Path $installed $name))) ("Removed owned file: $name") }
Assert (-not (Test-Path -LiteralPath $registry)) 'Uninstall removes test registration'
Assert (-not (Test-Path -LiteralPath (Join-Path $sandbox 'Start Menu/HallJoy'))) 'Uninstall removes test menu shortcuts'
Assert ((Hash $desktopShortcut) -eq $foreignHash) 'Uninstall preserves the foreign desktop shortcut'
Assert ((Hash "$installed\settings.ini") -eq $settingsHash -and (Hash "$installed\GlobalProfiles\keep.ini") -eq $profileHash) 'Uninstall preserves user settings and profiles'

# A fresh location exercises creation and removal of an owned desktop shortcut.
# Remove only the fake shortcut created by this test, after verifying its bytes.
if ((Hash $desktopShortcut) -ne $foreignHash) { throw 'Test shortcut changed unexpectedly.' }
Remove-Item -LiteralPath $desktopShortcut
$installed = Join-Path $sandbox 'Fresh install'
Assert ((Run-Setup $installed) -eq 0) 'Fresh installation with desktop component succeeds'
$desktop = $shell.CreateShortcut($desktopShortcut)
Assert ($desktop.TargetPath -eq "$installed\HallJoy.exe") 'Optional desktop shortcut targets HallJoy'
Assert ($desktop.IconLocation.StartsWith("$installed\HallJoy.ico", [StringComparison]::OrdinalIgnoreCase)) 'Desktop shortcut uses the HallJoy icon'
Uninstall
Assert (-not (Test-Path -LiteralPath $desktopShortcut)) 'Uninstall removes its own desktop shortcut'
Assert (-not (Test-Path -LiteralPath $installed)) 'An installation without user files is removed completely'
@{ checks = @($checks.ToArray()); applicationLaunched = $false; driverInstallerLaunched = $false } |
    ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $sandbox 'results.json') -Encoding UTF8
Write-Output "RESULTS=$sandbox\results.json"
