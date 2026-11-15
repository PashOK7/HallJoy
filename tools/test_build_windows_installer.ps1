[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PythonPath,[Parameter(Mandatory=$true)][string]$NsisPath,[Parameter(Mandatory=$true)][string]$ExecutablePath,[Parameter(Mandatory=$true)][string]$RuntimePath)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$root=Split-Path -Parent $PSScriptRoot
$sandbox=Join-Path $root ('build/tests/optional installer '+[guid]::NewGuid().ToString('N'))
$checks=New-Object System.Collections.Generic.List[string]
function Assert([bool]$Condition,[string]$Reason){if(-not $Condition){throw "FAILED: $Reason"};$checks.Add($Reason);Write-Output "PASS: $Reason"}
function Copy-Input([string]$Source,[string]$Relative){$t=Join-Path $sandbox $Relative;$null=New-Item -ItemType Directory -Path (Split-Path -Parent $t) -Force;Copy-Item -LiteralPath $Source -Destination $t}
foreach($rel in @('BUILD.cmd','tools/build_windows_installer.ps1','tools/build_environment.ps1','tools/package_windows_installer.ps1','tools/verify_embedded_licenses.py','tools/installer/windows.nsi','tools/installer/data_cleanup.nsh','tools/installer/INSTALLATION.txt','tools/installer/PRIVACY_AND_SECURITY.txt','src/HallJoyProject/HallJoy/version.h','src/HallJoyProject/HallJoy/HallJoy.ico','LICENSE','THIRD_PARTY_NOTICES.md','tools/dependency-lock.json')){Copy-Input (Join-Path $root $rel) $rel}
$PythonPath=(Get-Item -LiteralPath $PythonPath).FullName;$NsisPath=(Get-Item -LiteralPath $NsisPath).FullName
$run=0
function Run-Entry([string]$Extra='', [string]$Compiler=$NsisPath){
 $script:run++
 $args='-NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $sandbox 'tools/build_windows_installer.ps1')+'" -PythonPath "'+$PythonPath+'" -NsisPath "'+$Compiler+'" '+$Extra
 $p=Start-Process powershell.exe -ArgumentList $args -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput (Join-Path $sandbox "run-$script:run.log") -RedirectStandardError (Join-Path $sandbox "run-$script:run.err")
 return $p.ExitCode
}
Assert ((Run-Entry '-CheckOnly') -ne 0) 'Missing portable EXE fails before packaging'
Assert (-not (Test-Path (Join-Path $sandbox 'build/packages'))) 'Failed preflight creates no packages'
Copy-Input $ExecutablePath 'build/bin/Release/x64/HallJoy.exe';Copy-Input $RuntimePath 'build/runtime/universal_analog_abiv1.dll'
$app=Join-Path $sandbox 'build/bin/Release/x64/HallJoy.exe';$original=(Get-FileHash -LiteralPath $app).Hash
Assert ((Run-Entry '-CheckOnly') -eq 0) 'Optional CheckOnly accepts existing EXE and tools without a compiler'
Assert (-not (Test-Path (Join-Path $sandbox 'build/packages'))) 'CheckOnly creates no packages'
$runtime=Join-Path $sandbox 'build/runtime/universal_analog_abiv1.dll';[IO.File]::WriteAllText($runtime,'invalid fixture')
Assert ((Run-Entry) -ne 0) 'Wrong embedded-runtime input fails closed'
Assert (-not (Test-Path (Join-Path $sandbox 'build/packages'))) 'Failed resource verification publishes nothing'
Copy-Input $RuntimePath 'build/runtime/universal_analog_abiv1.dll'
Assert ((Run-Entry) -eq 0) 'Explicit optional command builds a real NSIS installer'
$packages=@(Get-ChildItem -LiteralPath (Join-Path $sandbox 'build/packages') -Filter '*-setup.exe' -Recurse)
Assert ($packages.Count -eq 1) 'One optional installer produced'
$package=$packages[0];$digest=(Get-FileHash -LiteralPath $package.FullName).Hash.ToLowerInvariant();$inventory=Get-Content ($package.FullName+'.json') -Raw | ConvertFrom-Json
Assert ($inventory.sha256 -eq $digest -and $inventory.payload.Count -eq 7) 'Installer and seven payload hashes recorded'
Assert ((Get-Content ($package.FullName+'.sha256') -Raw).Trim() -eq ($digest+'  '+$package.Name)) 'Checksum sidecar matches exact setup'
Assert (@(Get-ChildItem -LiteralPath (Join-Path $sandbox 'build') -File).Count -eq 0) 'No duplicate application or setup output in build root'
Assert ((Get-FileHash -LiteralPath $app).Hash -eq $original) 'Packaging preserves ordinary application bytes'
Assert ((Run-Entry ('-OutputDirectory "'+$package.DirectoryName+'"')) -ne 0) 'Existing package directory refused'
Assert ((Get-FileHash -LiteralPath $package.FullName).Hash.ToLowerInvariant() -eq $digest) 'Previous installer remains unchanged'
$fake=Join-Path $sandbox 'fake compiler.cmd'
[IO.File]::WriteAllText($fake,"@echo off`r`nif `"%~1`"==`"/VERSION`" (`r`necho v3.13`r`nexit /b 0`r`n)`r`nexit /b 23`r`n",[Text.Encoding]::ASCII)
$failedOutput=Join-Path $sandbox 'failed package'
Assert ((Run-Entry ('-OutputDirectory "'+$failedOutput+'"') $fake) -ne 0) 'Compiler failure is reported'
Assert (-not (Test-Path -LiteralPath $failedOutput)) 'Failed setup compilation publishes no directory'
Assert ((Get-FileHash -LiteralPath $app).Hash -eq $original -and (Get-FileHash -LiteralPath $package.FullName).Hash.ToLowerInvariant() -eq $digest) 'Failure preserves both ordinary EXE and previous package'
Assert ((Run-Entry) -eq 0) 'Repeated optional packaging succeeds into a fresh directory'
Assert (@(Get-ChildItem -LiteralPath (Join-Path $sandbox 'build/packages') -Filter '*-setup.exe' -Recurse).Count -eq 2) 'Previous packages retained'
Assert ((Get-FileHash -LiteralPath $app).Hash -eq $original) 'No application replacement at any stage'
@{checks=@($checks.ToArray());applicationLaunched=$false;sourceCompiled=$false} | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $sandbox 'results.json') -Encoding UTF8
Write-Output "RESULTS=$sandbox/results.json"
