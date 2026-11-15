[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$fixtureRoot = Join-Path $root ('build/obj/uap-isolation-test-' + [guid]::NewGuid().ToString('N'))
$foreignRoot = Join-Path ([IO.Path]::GetTempPath()) ('HallJoyUapIsolation-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $fixtureRoot, $foreignRoot
$targetA = Join-Path $fixtureRoot 'a/HallJoyUapFixture.exe'
$targetB = Join-Path $fixtureRoot 'b/HallJoyUapFixture.exe'
$foreign = Join-Path $foreignRoot 'HallJoyUapFixture.exe'
foreach ($target in @($targetA, $targetB)) {
    $null = New-Item -ItemType Directory -Path (Split-Path -Parent $target)
}
$source = Join-Path $fixtureRoot 'fixture.cs'
[IO.File]::WriteAllText($source, 'public class Fixture { public static void Main() { System.Threading.Thread.Sleep(300000); } }')
$csc = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $csc /nologo /target:winexe "/out:$targetA" $source
if ($LASTEXITCODE -ne 0) { throw 'UAP isolation fixture compilation failed.' }
Copy-Item -LiteralPath $targetA -Destination $targetB
Copy-Item -LiteralPath $targetA -Destination $foreign
. (Join-Path $PSScriptRoot 'build_uap_isolation.ps1')
. (Join-Path $PSScriptRoot 'halljoy_process_identity.ps1')
$script:paths = @($targetA, $targetB, $foreign)
$script:failClose = ''
$script:failEnumeration = $false
$checks = New-Object System.Collections.Generic.List[string]
function Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAILED: $Message" }
    $checks.Add($Message)
    Write-Output "PASS: $Message"
}
function Find-Fixture([string]$Path) {
    @(Get-Process -Name HallJoyUapFixture -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Path })
}
function Stop-Fixtures {
    foreach ($path in $script:paths) {
        foreach ($process in @(Find-Fixture $path)) {
            if ([HallJoyBuild.ProcessIdentity]::ImagePath($process.Id) -ne $path) { throw 'Fixture identity changed.' }
            Stop-Process -InputObject $process -Force
            $null = $process.WaitForExit(5000)
        }
    }
}
# Use real OS processes and the production identity/close/restore helpers.
# Only the enumeration boundary selects fixture names, so the owner's running
# HallJoy is never part of this regression. Failure injection stays test-local.
function Get-HallJoyUapProcesses {
    if ($script:failEnumeration) { throw 'intentional enumeration failure' }
    @(Get-CimInstance Win32_Process -Filter "Name='HallJoyUapFixture.exe'" |
        Where-Object { $_.ExecutablePath -in $script:paths })
}
function Close-HallJoyUapTarget([string]$Target) {
    if ($Target -eq $script:failClose) { throw 'intentional second-target close failure' }
    $null = & (Join-Path $PSScriptRoot 'close_project_halljoy.ps1') -TargetPath $Target -GraceMs 0
}
try {
    $first = Start-Process -FilePath $targetA -WindowStyle Hidden -PassThru
    $second = Start-Process -FilePath $targetB -WindowStyle Hidden -PassThru
    $worker = Start-Process -FilePath $targetA -ArgumentList '--worker' -WindowStyle Hidden -PassThru
    Invoke-HallJoyUapIsolation {
        Check (@(Get-HallJoyUapProcesses).Count -eq 0) 'ABI callback runs only after all fixture owners exit'
    }
    Check ($first.HasExited -and $second.HasExited -and $worker.HasExited) 'Both original app processes and worker were stopped'
    Check (@(Find-Fixture $targetA).Count -eq 1 -and @(Find-Fixture $targetB).Count -eq 1) 'Both original app paths restore exactly once; worker does not restart'
    Stop-Fixtures

    $first = Start-Process -FilePath $targetA -WindowStyle Hidden -PassThru
    $failed = $false
    try {
        Invoke-HallJoyUapIsolation {
            Check (@(Get-HallJoyUapProcesses).Count -eq 0) 'Failure callback also has exclusive process isolation'
            throw 'intentional ABI failure'
        }
    } catch { $failed = $_.Exception.Message -like '*intentional ABI failure*' }
    Check ($failed -and $first.HasExited) 'ABI error is preserved'
    Check (@(Find-Fixture $targetA).Count -eq 1) 'App is restored after ABI failure'
    Stop-Fixtures

    $first = Start-Process -FilePath $targetA -WindowStyle Hidden -PassThru
    $second = Start-Process -FilePath $targetB -WindowStyle Hidden -PassThru
    $script:failClose = $targetB
    $script:enteredCheck = $false
    $failed = $false
    try { Invoke-HallJoyUapIsolation { $script:enteredCheck = $true } }
    catch { $failed = $_.Exception.Message -like '*intentional second-target close failure*' }
    Check ($failed -and -not $script:enteredCheck) 'Partial shutdown failure never starts ABI work'
    Check ($first.HasExited -and -not $second.HasExited -and @(Find-Fixture $targetA).Count -eq 1) 'Partial shutdown restores the first app and preserves the second'
    $script:failClose = ''
    Stop-Fixtures

    $first = Start-Process -FilePath $targetA -WindowStyle Hidden -PassThru
    $other = Start-Process -FilePath $foreign -WindowStyle Hidden -PassThru
    $failed = $false
    try { Invoke-HallJoyUapIsolation { throw 'ABI callback must not be reached' } }
    catch { $failed = $_.Exception.Message -like '*outside this checkout or Windows session*' }
    Check ($failed -and -not $first.HasExited -and -not $other.HasExited) 'Foreign checkout blocks before any process is closed'
    Stop-Fixtures

    $first = Start-Process -FilePath $targetA -WindowStyle Hidden -PassThru
    $script:failEnumeration = $true
    $failed = $false
    try { Invoke-HallJoyUapIsolation { throw 'ABI callback must not be reached' } }
    catch { $failed = $_.Exception.Message -like '*intentional enumeration failure*' }
    Check ($failed -and -not $first.HasExited) 'Enumeration failure leaves the running app untouched'
    $script:failEnumeration = $false
    Stop-Fixtures

    $first = Start-Process -FilePath $targetA -WindowStyle Hidden -PassThru
    $failed = $false
    try {
        Invoke-HallJoyUapIsolation {
            $bytes = [IO.File]::ReadAllBytes($targetA)
            [IO.File]::WriteAllBytes($targetA, [byte[]]($bytes + 0))
        }
    } catch { $failed = $_.Exception.Message -like '*Original executable changed*' }
    Check ($failed -and $first.HasExited) 'Changed executable is rejected during restoration'
    Check (@(Find-Fixture $targetA).Count -eq 0) 'Changed executable is never launched'
    Copy-Item -LiteralPath $targetB -Destination $targetA -Force

    $worker = Start-Process -FilePath $targetA -ArgumentList '--worker' -WindowStyle Hidden -PassThru
    Invoke-HallJoyUapIsolation { Check (@(Get-HallJoyUapProcesses).Count -eq 0) 'Orphan worker is removed before ABI work' }
    Check ($worker.HasExited -and @(Find-Fixture $targetA).Count -eq 0) 'Orphan worker never triggers an interactive restart'
    Invoke-HallJoyUapIsolation { Check (@(Get-HallJoyUapProcesses).Count -eq 0) 'No-owner check runs normally' }
    Check (@(Get-HallJoyUapProcesses).Count -eq 0) 'No-owner check launches no application'
    @{ checks = @($checks.ToArray()) } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixtureRoot 'results.json') -Encoding utf8
    Write-Output "BUILD_UAP_ISOLATION=PASS checks=$($checks.Count) evidence=$fixtureRoot"
} finally {
    Stop-Fixtures
    # Retain uniquely named fixture/evidence directories; no recursive cleanup.
}
