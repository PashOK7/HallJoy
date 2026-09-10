[CmdletBinding()]
param(
    [string]$ExePath = '',
    [ValidateRange(1, 10)]
    [int]$RunSeconds = 3
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $root `
        'build\packages\HallJoy-DrunkDeer-Diagnostic.exe'
}
$ExePath = [IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "DrunkDeer diagnostic executable was not found: $ExePath"
}

$testRoot = Join-Path $env:TEMP `
    ('halljoy-drunkdeer-abnormal-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$testExe = Join-Path $testRoot 'HallJoy-DrunkDeer-Diagnostic.exe'
Copy-Item -LiteralPath $ExePath -Destination $testExe
New-Item -ItemType File -Path (Join-Path $testRoot 'HallJoy.portable') | Out-Null

$process = Start-Process -FilePath $testExe -WorkingDirectory $testRoot `
    -WindowStyle Hidden -PassThru
try {
    Start-Sleep -Seconds $RunSeconds
    $process.Refresh()
    if ($process.HasExited) {
        throw "Diagnostic exited before the forced-exit test: $($process.ExitCode)"
    }
    Stop-Process -Id $process.Id -Force
    $process.WaitForExit(5000) | Out-Null
    Start-Sleep -Milliseconds 500

    $trace = Join-Path $testRoot 'HallJoyStabilityTrace.log'
    if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
        throw 'Forced-exit test did not retain HallJoyStabilityTrace.log.'
    }
    $bytes = [IO.File]::ReadAllBytes($trace)
    if ($bytes.Length -eq 0) {
        throw 'Forced-exit trace is empty.'
    }
    if ([Array]::IndexOf($bytes, [byte]0) -ge 0) {
        throw 'Forced-exit trace contains a preallocated NUL tail.'
    }
    $text = [Text.Encoding]::UTF8.GetString($bytes)
    foreach ($marker in @(
        '[component=main][event=session.start] schema=2',
        'storage=direct_append',
        'critical_flush=1',
        'preallocation=0'
    )) {
        if (-not $text.Contains($marker)) {
            throw "Forced-exit trace is missing durable marker: $marker"
        }
    }
    if ($text.Contains('[component=main][event=session.end]')) {
        throw 'Forced-exit trace falsely claims a normal session end.'
    }
    if ($bytes.Length -lt 2 -or $bytes[-2] -ne 13 -or $bytes[-1] -ne 10) {
        throw 'Forced-exit trace ends in a partial line.'
    }
    Write-Host "DrunkDeer forced-exit log durability: PASS ($($bytes.Length) bytes)" `
        -ForegroundColor Green
}
finally {
    $process.Refresh()
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
    $resolved = [IO.Path]::GetFullPath($testRoot)
    $temp = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
    if ($resolved.StartsWith($temp, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force `
            -ErrorAction SilentlyContinue
    }
}
