[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$fixtureRoot = Join-Path $root ('build\obj\replacement-test-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $fixtureRoot
$otherDir = Join-Path $fixtureRoot 'other'
$null = New-Item -ItemType Directory -Path $otherDir
$target = Join-Path $fixtureRoot 'HallJoyLifecycleFixture.exe'
$other = Join-Path $otherDir 'HallJoyLifecycleFixture.exe'
$candidate = Join-Path $fixtureRoot 'candidate.exe'
$publisher = Join-Path $PSScriptRoot 'publish_halljoy_build.ps1'
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
function Find-Fixture([string]$path) {
    @(Get-Process -Name 'HallJoyLifecycleFixture' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $path })
}
function Check([bool]$condition, [string]$reason) { if (-not $condition) { throw $reason } }
try {
    foreach ($version in 1,2) {
        $source = Join-Path $fixtureRoot "v$version.cs"
        [IO.File]::WriteAllText($source, "[assembly:System.Reflection.AssemblyVersion(`"$version.0.0.0`")] public class Fixture { public static void Main() { System.Threading.Thread.Sleep(120000); } }")
        $output = if ($version -eq 1) { $target } else { $candidate }
        & $csc /nologo /target:winexe "/out:$output" $source
        if ($LASTEXITCODE -ne 0) { throw 'Fixture compiler failed.' }
    }
    Copy-Item -LiteralPath $target -Destination $other
    $initial = Start-Process -FilePath $target -WindowStyle Hidden -PassThru
    $unrelated = Start-Process -FilePath $other -WindowStyle Hidden -PassThru
    & $publisher -CandidatePath $other -TargetPath $target
    Check (-not $initial.HasExited) 'Unchanged image closed a running process.'
    $failed = $false
    try { & $publisher -CandidatePath (Join-Path $fixtureRoot 'missing.exe') -TargetPath $target } catch { $failed = $true }
    Check ($failed -and -not $initial.HasExited) 'Missing candidate changed the running process.'
    & $publisher -CandidatePath $candidate -TargetPath $target
    Check ($initial.WaitForExit(5000)) 'Old target survived replacement.'
    $restarted = @(Find-Fixture $target)
    Check ($restarted.Count -eq 1 -and $restarted[0].Id -ne $initial.Id) 'Previously running app was not restored exactly once.'
    Check (-not $unrelated.HasExited) 'Another path was closed.'
    Check ((Get-FileHash $target).Hash -eq (Get-FileHash $candidate).Hash) 'Installed bytes differ.'
    # Deny atomic replacement after shutdown: the old verified file must remain
    # and its app must be restored by finally, without launching the candidate.
    $lock = [IO.File]::Open($target, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $failed = $false
        try { & $publisher -CandidatePath $other -TargetPath $target } catch { $failed = $true }
        Check $failed 'Locked replacement unexpectedly succeeded.'
        Check ((Get-FileHash $target).Hash -eq (Get-FileHash $candidate).Hash) 'Failed replacement damaged old bytes.'
        Check (@(Find-Fixture $target).Count -eq 1) 'Old app was not restored after replacement failure.'
    } finally { $lock.Dispose() }
    foreach ($process in @(Find-Fixture $target)) { Stop-Process -InputObject $process -Force; $null = $process.WaitForExit(5000) }
    & $publisher -CandidatePath $other -TargetPath $target
    Check (@(Find-Fixture $target).Count -eq 0) 'Inactive app was launched without a previous instance.'
    Write-Output 'BUILD_REPLACEMENT=PASS unchanged missing_candidate exact_path success_restart failure_restore inactive_no_launch'
} finally {
    foreach ($path in @($target,$other)) {
        foreach ($process in @(Find-Fixture $path)) { Stop-Process -InputObject $process -Force; $null = $process.WaitForExit(5000) }
    }
    # Keep the fixture/evidence directory; no broad recursive cleanup.
}
