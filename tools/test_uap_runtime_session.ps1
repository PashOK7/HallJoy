[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path $root ('build/obj/uap-lifecycle-test-' + [guid]::NewGuid().ToString('N'))
$fixtureTools = Join-Path $fixture 'tools'
$target = Join-Path $fixture '.local/portable/HallJoy.exe'
$null = New-Item -ItemType Directory -Path $fixtureTools, (Split-Path -Parent $target) -Force
Set-Content -LiteralPath $target -Value 'Non-executable test bytes; no real HallJoy is launched or closed.'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'uap_runtime_session.ps1') -Destination $fixtureTools
# Exercise the production orchestration with controlled OS/process boundaries.
@'
param([string]$TargetPath, [switch]$InspectOnly)
if (-not $InspectOnly) {
    $context.Closes++
    if (-not $context.KeepRunning) { $context.Running = $false; $context.Owners = @() }
    if ($context.CloseFailure) { throw 'injected close failure' }
}
[pscustomobject]@{ WasRunning=$context.Running; Count=$context.Owners.Count; WindowStyle='Minimized' }
'@ | Set-Content -LiteralPath (Join-Path $fixtureTools 'close_project_halljoy.ps1')
. (Join-Path $fixtureTools 'uap_runtime_session.ps1')
function Get-HallJoyUapProcesses {
    $context.Reads++
    if ($context.EnumerationFailure) { throw 'injected enumeration failure' }
    if ($context.Race -and $context.Reads -eq 2) { $context.Owners[0].CreationDate = 'changed' }
    $context.Owners
}
function Start-Process {
    param($FilePath, $WorkingDirectory, $WindowStyle, [switch]$PassThru)
    if ($FilePath -ne $target -or $WorkingDirectory -ne (Split-Path -Parent $target) -or $WindowStyle -ne 'Minimized') {
        throw 'Restoration lost the executable, working directory or visible window style.'
    }
    $context.Starts++
    $context.Running = $true
    $process = [pscustomobject]@{}
    $process | Add-Member ScriptMethod WaitForExit { param($Milliseconds) return $false }
    $process
}
function New-Owner($Id, $Parent, $Arguments = '') {
    [pscustomobject]@{
        ProcessId=$Id; ParentProcessId=$Parent; SessionId=(Get-Process -Id $PID).SessionId
        ExecutablePath=$target; CommandLine=('"' + $target + '" ' + $Arguments); CreationDate='original'
    }
}
function Reset-Case {
    $script:context = [pscustomobject]@{
        Running=$true; Owners=@((New-Owner 100 99), (New-Owner 101 100 '--diagnostic-watch=100'),
            (New-Owner 102 100 '--halljoy-analog-host'), (New-Owner 103 100 '--halljoy-vigem-output-host'),
            (New-Owner 104 100 '--halljoy-shark-worker'), (New-Owner 105 100 '--halljoy-mini60-worker'))
        Closes=0; Starts=0; Checks=0; Reads=0; KeepRunning=$false; CloseFailure=$false
        EnumerationFailure=$false; Race=$false
    }
}
function Assert([bool]$Condition, [string]$Reason) { if (-not $Condition) { throw $Reason } }
function Invoke-Case([string]$ExpectedError = '') {
    $errorText = ''
    try {
        Invoke-HallJoyIsolatedUapCheck -Check {
            Assert (-not $context.Running) 'The physical check ran alongside the app.'
            $context.Checks++
            if ($ExpectedError -eq 'injected check failure') { throw $ExpectedError }
        }
    } catch { $errorText = $_.Exception.Message }
    if ($ExpectedError) { Assert ($errorText -like "*$ExpectedError*") "Wrong failure: $errorText" }
    else { Assert (-not $errorText) "Unexpected failure: $errorText" }
}
$passed = New-Object System.Collections.Generic.List[string]
Reset-Case
$buildRootTarget = [IO.Path]::GetFullPath((Join-Path $fixture 'build/HallJoy.exe'))
foreach ($owner in $context.Owners) {
    $owner.CommandLine = $owner.CommandLine.Replace($owner.ExecutablePath, $buildRootTarget)
    $owner.ExecutablePath = $buildRootTarget
}
$accepted = Get-HallJoyUapSuspendTarget -Owners $context.Owners -Root $fixture -SessionId (Get-Process -Id $PID).SessionId
Assert ($accepted -eq $buildRootTarget) 'Build-root app was not accepted as one verified application family.'
$passed.Add('build-root application and its five workers accepted')
Reset-Case
Invoke-Case
Assert ($context.Closes -eq 1 -and $context.Starts -eq 1 -and $context.Checks -eq 1 -and $context.Running) 'Success did not restore exactly once.'
$passed.Add('one application with five workers: close, check, restore')
Reset-Case
Invoke-Case 'injected check failure'
Assert ($context.Closes -eq 1 -and $context.Starts -eq 1 -and $context.Running) 'Failed check did not restore.'
$passed.Add('failed check restores and preserves failure')
Reset-Case
$context.CloseFailure = $true
Invoke-Case 'injected close failure'
Assert ($context.Starts -eq 1 -and $context.Checks -eq 0) 'Partial shutdown failure did not restore.'
$passed.Add('partial shutdown failure restores without checking hardware')
Reset-Case
$context.KeepRunning = $true
Invoke-Case 'Physical-UAP isolation failed'
Assert ($context.Starts -eq 0 -and $context.Checks -eq 0) 'Active app was duplicated or physical check ran.'
$passed.Add('remaining process blocks hardware access without duplicate restart')
Reset-Case
$context.Running = $false; $context.Owners = @()
Invoke-Case
Assert ($context.Closes -eq 0 -and $context.Starts -eq 0 -and $context.Checks -eq 1) 'Inactive case launched or closed an app.'
$passed.Add('inactive application stays inactive')
foreach ($case in @('foreign path','other session','unknown role','multiple apps','unrelated worker','missing command','enumeration failure','process race')) {
    Reset-Case
    $expected = 'Cannot safely suspend'
    switch ($case) {
        'foreign path' { $context.Owners[0].ExecutablePath = 'C:\unrelated\HallJoy.exe' }
        'other session' { $context.Owners[0].SessionId = -1 }
        'unknown role' { $context.Owners[1].CommandLine = '"HallJoy.exe" --unknown'; $expected = 'Unrecognized HallJoy arguments' }
        'multiple apps' { $context.Owners += New-Owner 200 99; $expected = 'Expected one local HallJoy' }
        'unrelated worker' { $context.Owners[1].ParentProcessId = 99; $expected = 'not one verified application family' }
        'missing command' { $context.Owners[0].CommandLine = $null }
        'enumeration failure' { $context.EnumerationFailure = $true; $expected = 'injected enumeration failure' }
        'process race' { $context.Race = $true; $expected = 'processes changed before shutdown' }
    }
    Invoke-Case $expected
    Assert ($context.Closes -eq 0 -and $context.Starts -eq 0 -and $context.Checks -eq 0) "Unsafe side effect in $case"
    $passed.Add("$case rejected before shutdown")
}
$passed | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture 'results.json')
Write-Output "UAP_RUNTIME_SESSION=PASS ($($passed.Count) scenarios)"
Write-Output "Evidence: $fixture"
