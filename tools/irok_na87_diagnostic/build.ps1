[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$stamp += '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$evidence = Join-Path $root ('build\evidence\na87-diagnostic-' + $stamp)
if (Test-Path -LiteralPath $evidence) { throw 'Build evidence already exists.' }
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$compiler = (Get-Command g++.exe -ErrorAction Stop).Source
$compiled = Join-Path $evidence 'HallJoy-NA87-Diagnostic.exe'
$include = Join-Path $root 'src\HallJoyProject\HallJoy'
$arguments = @('-std=c++20','-O2','-Wall','-Wextra','-Werror','-pedantic',
    '-municode','-static','-static-libgcc','-static-libstdc++',('-I' + $include),
    (Join-Path $PSScriptRoot 'main.cpp'),(Join-Path $include 'irok_nd75_protocol.cpp'),
    '-lsetupapi','-lhid','-luuid','-o',$compiled)
$buildOutput = @(& $compiler @arguments 2>&1)
$buildExit = $LASTEXITCODE
$buildOutput | Set-Content -LiteralPath (Join-Path $evidence 'compile.log') -Encoding UTF8
if ($buildExit -ne 0) { $buildOutput | Out-Host; throw 'NA87 diagnostic compilation failed.' }
& $compiled --self-test
if ($LASTEXITCODE -ne 0) { throw 'NA87 protocol/metrics self-test failed.' }
$watchdogNote = Join-Path $evidence 'watchdog-test.txt'
& $compiled --watchdog-self-test $watchdogNote
if ($LASTEXITCODE -ne 102 -or -not (Test-Path -LiteralPath $watchdogNote)) { throw 'Worker watchdog test failed.' }
$parentTest = Join-Path $evidence 'parent-timeout'
[IO.Directory]::CreateDirectory($parentTest) | Out-Null
& $compiled --parent-timeout-self-test $parentTest
if ($LASTEXITCODE -ne 0) { throw 'Parent timeout test failed.' }
$inventory = Join-Path $evidence 'inventory-only'
[IO.Directory]::CreateDirectory($inventory) | Out-Null
& $compiled --inventory $inventory
$inventoryExit = $LASTEXITCODE
if ($inventoryExit -notin @(0,2)) { throw 'Metadata-only inventory test failed.' }
if (Select-String -LiteralPath (Join-Path $inventory 'session.jsonl') -SimpleMatch '"kind":"tx"' -Quiet) {
    throw 'Inventory mode unexpectedly transmitted a report.'
}
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'tools\tests\test_irok_na87_inventory.ps1')
if ($LASTEXITCODE -ne 0) { throw 'PnP inventory fixture test failed.' }
$imports = @(& (Join-Path (Split-Path -Parent $compiler) 'objdump.exe') -p $compiled |
    Select-String 'DLL Name:' | ForEach-Object { $_.Line.Trim() })
if ($imports -match 'libstdc\+\+|libgcc|libwinpthread') { throw 'Unexpected external compiler runtime DLL.' }
$imports | Set-Content -LiteralPath (Join-Path $evidence 'imports.txt') -Encoding UTF8
$package = Join-Path $root ('build\packages\HallJoy-NA87-Diagnostic-' + $stamp)
if (Test-Path -LiteralPath $package) { throw 'Package already exists.' }
[IO.Directory]::CreateDirectory($package) | Out-Null
Copy-Item -LiteralPath $compiled -Destination $package
foreach ($name in @('Run-NA87-Test.ps1','START-NA87-TEST.cmd','README-NA87-TEST.md')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $package
}
Copy-Item -LiteralPath (Join-Path $root 'tools\collect_irok_na87_inventory.ps1') -Destination $package
$manifest = @()
foreach ($file in @(Get-ChildItem -LiteralPath $package -File)) {
    $manifest += (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash + '  ' + $file.Name
}
$manifest | Set-Content -LiteralPath (Join-Path $package 'SHA256SUMS.txt') -Encoding ASCII
# Exercise the shipped launcher and automatic result archive without keyboard I/O.
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $package 'Run-NA87-Test.ps1') -InventoryOnly
if ($LASTEXITCODE -ne 0) { throw 'Packaged launcher/archive self-test failed.' }
$launcherEvidence = @(Get-ChildItem -LiteralPath $package | Where-Object { $_.Name -like 'NA87-results-*' })
foreach ($item in $launcherEvidence) {
    # The enumerated immediate children stay in the new package directory; the
    # destination is the new build-evidence directory. Preserve, do not delete.
    $resolved = [IO.Path]::GetFullPath($item.FullName)
    if (-not $resolved.StartsWith([IO.Path]::GetFullPath($package) + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw 'Launcher evidence is outside package.' }
    $destination = [IO.Path]::GetFullPath($evidence)
    $evidenceRoot = [IO.Path]::GetFullPath((Join-Path $root 'build\evidence')).TrimEnd('\') + '\'
    if (-not $destination.StartsWith($evidenceRoot,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Launcher evidence destination is outside build/evidence.'
    }
    Move-Item -LiteralPath $resolved -Destination $destination
}
$zip = $package + '.zip'
if (Test-Path -LiteralPath $zip) { throw 'Package ZIP already exists.' }
$packageFiles = @((Get-ChildItem -LiteralPath $package -File).FullName)
Compress-Archive -LiteralPath $packageFiles -DestinationPath $zip
$canonical = Join-Path $root 'build\bin\IrokNa87Diagnostic\Release\x64\HallJoy-NA87-Diagnostic.exe'
[IO.Directory]::CreateDirectory((Split-Path -Parent $canonical)) | Out-Null
if (Test-Path -LiteralPath $canonical) {
    $beforeHash = (Get-FileHash -LiteralPath $canonical -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $canonical -Destination (Join-Path $evidence 'previous-canonical.exe')
    if ((Get-FileHash -LiteralPath $canonical -Algorithm SHA256).Hash -ne $beforeHash) {
        throw 'Canonical executable changed during build; not replacing it.'
    }
}
Copy-Item -LiteralPath $compiled -Destination $canonical -Force
Write-Host ('Package: ' + $zip)
Write-Host ('Evidence: ' + $evidence)
