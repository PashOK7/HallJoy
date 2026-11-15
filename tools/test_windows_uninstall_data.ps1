[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PayloadPath,[Parameter(Mandatory=$true)][string]$NsisPath)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
# Runs the unchanged production recipe in its private TEST_ROOT first.
. (Join-Path $PSScriptRoot 'test_windows_installer.ps1') -PayloadPath $PayloadPath -NsisPath $NsisPath
$userData=Join-Path $sandbox 'UserData/HallJoy'
$tempData=Join-Path $sandbox 'Temp'
$dumpData=Join-Path $sandbox 'CrashDumps'
$outside=Join-Path $sandbox 'Unrelated data'
$otherPortable=Join-Path $sandbox 'Separate portable copy'
$sharedSeed=@('settings.ini','GlobalProfiles/song.ini',('GlobalProfiles/'+[char]0x041C+'elody.ini'),'Layouts/custom.ini','CurvePresets/gentle.ini','MigrationBackups/old/settings.ini','FactoryResetBackups/old/settings.ini','.internal/recovery.ini','Runtime/v1/host.dll','HallJoy.log','HallJoyCrash.txt')
function Seed([string]$Base,[string[]]$Names) {
 foreach($name in $Names){$file=Join-Path $Base $name;New-Item -ItemType Directory -Path (Split-Path -Parent $file) -Force | Out-Null;[IO.File]::WriteAllText($file,'fixture: '+$name)}
}
$extractName='HallJoy-ViGEm-'+('A'*32)
Seed $userData $sharedSeed
Seed $tempData @("$extractName/ViGEmBus.exe",'other-product/keep.txt','HallJoy-ViGEm-not-an-extract/keep.txt')
Seed $dumpData @('HallJoy.exe.1234.dmp','Unrelated.exe.1234.dmp')
Seed $tempData @('HallJoy-ViGEm-'+('B'*32))
Seed $dumpData @('HallJoy.exe.7777.dmp/keep.txt')
Seed $outside @('keep.txt')
Seed $otherPortable @('HallJoy.portable','settings.ini','HallJoy.log')
$preserved=@($sharedSeed | ForEach-Object {Join-Path $userData $_})+@((Join-Path $tempData "$extractName/ViGEmBus.exe"),(Join-Path $dumpData 'HallJoy.exe.1234.dmp'))
$preservedHashes=@{}
foreach($file in $preserved){$preservedHashes[$file]=Hash $file}
Assert ((Run-Setup $userData) -eq 7) 'Installation cannot overwrite shared data root'
Assert ((Run-Setup (Join-Path $userData 'Program')) -eq 7) 'Installation cannot be nested in shared data root'
$installed=Join-Path $sandbox 'Default data uninstall'
Assert ((Run-Setup $installed) -eq 0) 'Default data fixture installs'
$invalid=Start-Process -FilePath "$installed\Uninstall.exe" -ArgumentList ('/S /REMOVEUSERDATA=garbage _?='+$installed) -WindowStyle Hidden -Wait -PassThru
Assert ($invalid.ExitCode -eq 6) 'Invalid cleanup argument rejected'
Assert (Test-Path -LiteralPath "$installed\HallJoy.exe") 'Invalid argument leaves app intact'
Uninstall
foreach($file in $preserved){Assert ((Hash $file) -eq $preservedHashes[$file]) ('Default removal preserves: '+[IO.Path]::GetFileName($file))}

$installed=Join-Path $sandbox 'Explicit preserve uninstall'
Assert ((Run-Setup $installed) -eq 0) 'Explicit preserve fixture installs'
Uninstall '/REMOVEUSERDATA=0'
Assert ((Hash (Join-Path $userData 'settings.ini')) -eq $preservedHashes[(Join-Path $userData 'settings.ini')]) 'Explicit zero preserves data'
$installed=Join-Path $sandbox 'Clean uninstall'
Assert ((Run-Setup $installed) -eq 0) 'Cleanup fixture installation succeeds'
$installedData=@('settings.ini','settings.ini.bak','settings.ini.pre-bundle.bak','bindings.ini','GameProfiles.ini','HallJoy.portable','GlobalProfiles/music.ini','Layouts/custom.ini','CurvePresets/curve.ini','.internal/state.ini','MigrationBackups/old/settings.ini','FactoryResetBackups/old/settings.ini','.factory-reset-request.ini','HallJoy.log','HallJoy.log.tmp','HallJoyProviderV2Qualification.tmp','GameProfiles.ini.halljoy-new-123','.factory-reset-request.ini.halljoy-new-123','settings.ini.bak.halljoy-new-123','settings.ini.pre-bundle.bak.halljoy-new-123','.migration-from-exe-123.ini.halljoy-new-123','HallJoyAnalogHostCrashLatest.dmp','HallJoyAnalogHostCrash_20261115.dmp','HallJoyDiagnosticCrash.txt','settings.ini.halljoy-new-123','.halljoy-write-probe-123.tmp','.migration-from-exe-123.ini')
Seed $installed $installedData
Seed $installed @('personal-note.txt','settings.ini.halljoy-new-directory/keep.txt')
New-Item -ItemType Junction -Path (Join-Path $userData 'linked-folder') -Target $outside | Out-Null
Uninstall '/REMOVEUSERDATA=1'
Assert (-not (Test-Path -LiteralPath $userData)) 'Opt-in removes shared data, cache, backups, logs and junction itself'
foreach($file in $installedData){Assert (-not (Test-Path -LiteralPath (Join-Path $installed $file))) ('Opt-in removes installed data: '+$file)}
Assert (Test-Path -LiteralPath (Join-Path $outside 'keep.txt')) 'Owned-tree junction target preserved'
Assert (Test-Path -LiteralPath (Join-Path $installed 'personal-note.txt')) 'Unknown installation file preserved'
Assert (-not (Test-Path -LiteralPath (Join-Path $tempData $extractName))) 'Exact abandoned temp extract removed'
Assert (Test-Path -LiteralPath (Join-Path $tempData 'HallJoy-ViGEm-not-an-extract/keep.txt')) 'Unowned temp name preserved'
Assert (Test-Path -LiteralPath (Join-Path $tempData 'other-product/keep.txt')) 'Other product temp files preserved'
Assert (-not (Test-Path -LiteralPath (Join-Path $dumpData 'HallJoy.exe.1234.dmp'))) 'HallJoy WER dump removed'
Assert (Test-Path -LiteralPath (Join-Path $dumpData 'Unrelated.exe.1234.dmp')) 'Other process crash dump preserved'
Assert (Test-Path -LiteralPath (Join-Path $otherPortable 'settings.ini')) 'Separate portable copy preserved'
Assert (Test-Path -LiteralPath (Join-Path $tempData ('HallJoy-ViGEm-'+('B'*32)))) 'File resembling a temp directory preserved'
Assert (Test-Path -LiteralPath (Join-Path $dumpData 'HallJoy.exe.7777.dmp/keep.txt')) 'Directory resembling a crash dump preserved'
Assert (Test-Path -LiteralPath (Join-Path $installed 'settings.ini.halljoy-new-directory/keep.txt')) 'Directory resembling an atomic-write file preserved'

$installed=Join-Path $sandbox 'Linked root uninstall'
Assert ((Run-Setup $installed) -eq 0) 'Linked-root fixture installs'
New-Item -ItemType Junction -Path $userData -Target $outside | Out-Null
Uninstall '/REMOVEUSERDATA=1'
Assert (-not (Test-Path -LiteralPath $userData)) 'Linked data root itself removed'
Assert (Test-Path -LiteralPath (Join-Path $outside 'keep.txt')) 'Linked root target preserved'

$installed=Join-Path $sandbox 'Exact name lookalikes'
Assert ((Run-Setup $installed) -eq 0) 'Exact name fixture installs'
Seed $installed @('HallJoy.log/keep.txt','GlobalProfiles')
$lookalikeLog = Join-Path $installed 'HallJoy.log/keep.txt'
$lookalikeProfile = Join-Path $installed 'GlobalProfiles'
$lookalikeLogHash = Hash $lookalikeLog
$lookalikeProfileHash = Hash $lookalikeProfile
Uninstall '/REMOVEUSERDATA=1'
Assert ((Hash $lookalikeLog) -eq $lookalikeLogHash) 'Directory with exact log filename is preserved'
Assert ((Hash $lookalikeProfile) -eq $lookalikeProfileHash) 'File with exact profiles directory name is preserved'

$installed=Join-Path $sandbox 'Locked data uninstall'
Assert ((Run-Setup $installed) -eq 0) 'Locked-data fixture installs'
Seed $installed @('HallJoy.log')
$lockedFile=Join-Path $installed 'HallJoy.log'
$lockedHash=Hash $lockedFile
$lock=[IO.File]::Open($lockedFile,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try {
 $failure=Start-Process -FilePath "$installed\Uninstall.exe" -ArgumentList ('/S /REMOVEUSERDATA=1 _?='+$installed) -WindowStyle Hidden -Wait -PassThru
 Assert ($failure.ExitCode -eq 6) 'Cleanup failure returns nonzero'
 Assert (Test-Path -LiteralPath "$installed\HallJoy.exe") 'Failed cleanup retains app'
 Assert (Test-Path -LiteralPath "$installed\Uninstall.exe") 'Failed cleanup retains uninstaller'
 Assert (Test-Path -LiteralPath $registry) 'Failed cleanup retains registration'
} finally {$lock.Dispose()}
Assert ((Hash $lockedFile) -eq $lockedHash) 'Locked data unchanged'
Uninstall '/REMOVEUSERDATA=1'
Assert (-not (Test-Path -LiteralPath $installed)) 'Cleanup succeeds after unlock'
@{checks=@($checks.ToArray());applicationLaunched=$false;driverInstallerLaunched=$false} | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $sandbox 'cleanup-results.json') -Encoding UTF8
Write-Output "CLEANUP_RESULTS=$sandbox/cleanup-results.json"
