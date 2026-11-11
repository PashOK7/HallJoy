# Isolated file-behavior tests with fake GitHub responses; no network or app launch.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$sandbox = Join-Path $root ('build/tests/portable-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $sandbox -Force | Out-Null
$updater = Join-Path $PSScriptRoot 'update_portable.ps1'
$checks = New-Object System.Collections.Generic.List[string]
$fixture = @{}
$fixture.downloadCount = 0
$fixture.busy = $false
$fixture.startDuringDownload = $false
$fixture.corrupt = $false
$fixture.networkFailure = $false
$fixture.bytes = [Text.Encoding]::UTF8.GetBytes('new fixture binary; never execute')
$sha = [Security.Cryptography.SHA256]::Create()
try { $digest = [BitConverter]::ToString($sha.ComputeHash($fixture.bytes)).Replace('-', '').ToLowerInvariant() } finally { $sha.Dispose() }
$fixture.release = [pscustomobject]@{
    tag_name = 'v9.8.7'; draft = $false; prerelease = $false
    assets = @([pscustomobject]@{ name = 'HallJoy.exe'; digest = 'sha256:' + $digest; browser_download_url = 'https://github.com/PashOK7/HallJoy/releases/download/v9.8.7/HallJoy.exe' })
}
function Invoke-RestMethod { param($Uri, $Headers); if ($fixture.networkFailure) { throw 'Fixture network failure' }; return $fixture.release }
function Invoke-WebRequest {
    param($Uri, $OutFile, [switch]$UseBasicParsing)
    $fixture.downloadCount++
    [IO.File]::WriteAllBytes($OutFile, $(if ($fixture.corrupt) { [byte[]]@(1,2,3) } else { $fixture.bytes }))
    if ($fixture.startDuringDownload) { $fixture.busy = $true }
}
function Get-Process { param($Name, $ErrorAction); if ($fixture.busy) { [pscustomobject]@{ ProcessName = 'HallJoy' } } }
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAILED: $Message" }
    $checks.Add($Message); Write-Host "PASS: $Message"
}
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path).Hash }
function Expect-Failure([string]$Directory, [string]$Pattern) {
    $failed = $false
    try { & $updater -Directory $Directory | Out-Null } catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw }
        $failed = $true
    }
    Assert $failed ('Rejects ' + $Pattern)
}
$fresh = Join-Path $sandbox 'personal copy'
New-Item -ItemType Directory -Path (Join-Path $sandbox 'tools') | Out-Null
$defaultUpdater = Join-Path $sandbox 'tools/update_portable.ps1'
Copy-Item -LiteralPath $updater -Destination $defaultUpdater
& $defaultUpdater -CheckOnly | Out-Null
Assert (-not (Test-Path -LiteralPath (Join-Path $sandbox '.local')) -and $fixture.downloadCount -eq 0) 'Default path resolves without writing in CheckOnly mode'
& $updater -Directory $fresh -CheckOnly | Out-Null
Assert (-not (Test-Path -LiteralPath $fresh) -and $fixture.downloadCount -eq 0) 'CheckOnly makes no directory or download'
& $updater -Directory $fresh | Out-Null
Assert ((Hash "$fresh/HallJoy.exe").ToLowerInvariant() -eq $digest) 'First install verifies downloaded bytes'
Assert (Test-Path -LiteralPath "$fresh/HallJoy.portable" -PathType Leaf) 'First install enables portable storage'
Assert ((Get-Content "$fresh/release.json" -Raw | ConvertFrom-Json).sha256 -eq $digest) 'Release metadata records the installed digest'
& $updater -Directory $fresh | Out-Null
Assert ($fixture.downloadCount -eq 1) 'Up-to-date copy does not download again'

'private settings' | Set-Content -LiteralPath "$fresh/settings.ini"
New-Item -ItemType Directory -Path "$fresh/GlobalProfiles" | Out-Null
'private profile' | Set-Content -LiteralPath "$fresh/GlobalProfiles/keep.ini"
$settingsHash = Hash "$fresh/settings.ini"
$profileHash = Hash "$fresh/GlobalProfiles/keep.ini"
'old binary' | Set-Content -LiteralPath "$fresh/HallJoy.exe"
$oldHash = Hash "$fresh/HallJoy.exe"
& $updater -Directory $fresh | Out-Null
$saved = @(Get-ChildItem "$fresh/backups" -Recurse -File -Filter HallJoy.exe)
Assert ($saved.Count -eq 1 -and (Hash $saved[0].FullName) -eq $oldHash) 'Replacement keeps a byte-identical rollback EXE'
Assert ((Hash "$fresh/settings.ini") -eq $settingsHash -and (Hash "$fresh/GlobalProfiles/keep.ini") -eq $profileHash) 'Replacement preserves settings and profiles'

'old binary' | Set-Content -LiteralPath "$fresh/HallJoy.exe"
$fixture.corrupt = $true
Expect-Failure $fresh 'SHA-256 mismatch'
$fixture.corrupt = $false
Assert ((Hash "$fresh/HallJoy.exe") -eq $oldHash) 'Corrupt download leaves the old EXE intact'
Assert (@(Get-ChildItem $fresh -Force -Filter '.update-*').Count -eq 0) 'Failed-download staging is removed'
$fixture.busy = $true
$downloadsBefore = $fixture.downloadCount
Expect-Failure $fresh 'Close HallJoy before updating'
Assert ($fixture.downloadCount -eq $downloadsBefore) 'Running application prevents download'
$fixture.busy = $false
$fixture.startDuringDownload = $true
Expect-Failure $fresh 'started during download'
$fixture.startDuringDownload = $false
$fixture.busy = $false
Assert ((Hash "$fresh/HallJoy.exe") -eq $oldHash) 'Application starting during download prevents replacement'

$fixture.release.assets[0].browser_download_url = 'https://example.invalid/HallJoy.exe'
Expect-Failure $fresh 'Unexpected release download URL'
$fixture.release.assets[0].browser_download_url = 'https://github.com/PashOK7/HallJoy/releases/download/v9.8.7/HallJoy.exe'
$fixture.release.assets[0].digest = ''
Expect-Failure $fresh 'No valid SHA-256'
$fixture.release.assets[0].digest = 'sha256:' + $digest
$fixture.release.prerelease = $true
Expect-Failure $fresh 'stable HallJoy release'
$fixture.release.prerelease = $false
$fixture.networkFailure = $true
Expect-Failure $fresh 'Fixture network failure'
$fixture.networkFailure = $false
$foreign = Join-Path $sandbox 'installed copy'
New-Item -ItemType Directory -Path $foreign | Out-Null
'unrelated' | Set-Content -LiteralPath "$foreign/settings.ini"
Expect-Failure $foreign 'not a portable HallJoy folder'
Assert (@(Get-ChildItem $foreign).Count -eq 1) 'Foreign destination is untouched'
Assert ((Hash "$fresh/settings.ini") -eq $settingsHash -and (Hash "$fresh/GlobalProfiles/keep.ini") -eq $profileHash) 'All failure paths preserve private data'
$launcher = Join-Path $sandbox 'tools/start_portable.ps1'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'start_portable.ps1') -Destination $launcher
$launchData = Join-Path $sandbox '.local/portable'
$failed = $false
try { & $launcher -CheckOnly | Out-Null } catch { $failed = $_.Exception.Message -match 'Portable HallJoy is missing' }
Assert $failed 'Launcher rejects a missing EXE'
New-Item -ItemType Directory -Path $launchData -Force | Out-Null
[IO.File]::WriteAllBytes((Join-Path $launchData 'HallJoy.exe'), $fixture.bytes)
$failed = $false
try { & $launcher -CheckOnly | Out-Null } catch { $failed = $_.Exception.Message -match 'HallJoy.portable is missing' }
Assert $failed 'Launcher rejects a missing portable marker'
New-Item -ItemType File -Path (Join-Path $launchData 'HallJoy.portable') | Out-Null
& $launcher -CheckOnly | Out-Null
Assert $true 'Launcher CheckOnly accepts an EXE with a portable marker without starting it'
@{ checks = @($checks.ToArray()); networkUsed = $false; applicationLaunched = $false } |
    ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $sandbox 'results.json') -Encoding UTF8
Write-Output "RESULTS=$sandbox/results.json"
