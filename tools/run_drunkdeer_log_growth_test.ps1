[CmdletBinding()]
param([string]$ExePath = '')

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
    ('halljoy-drunkdeer-growth-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$testExe = Join-Path $testRoot 'HallJoy-DrunkDeer-Diagnostic.exe'
Copy-Item -LiteralPath $ExePath -Destination $testExe
try {
    $process = Start-Process -FilePath $testExe -WorkingDirectory $testRoot `
        -ArgumentList '--halljoy-test-drunkdeer-log-growth' `
        -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(30000)) {
        throw 'Unbounded log growth self-test did not exit within 30 seconds.'
    }
    if ($process.ExitCode -ne 0) {
        throw "Unbounded log growth self-test exited with $($process.ExitCode)."
    }
    $trace = Join-Path $testRoot 'HallJoyStabilityTrace.log'
    if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
        throw 'Unbounded log growth self-test did not create its trace.'
    }
    $traceFile = Get-Item -LiteralPath $trace
    if ($traceFile.Length -le 2MB) {
        throw "Trace did not grow beyond 2 MiB: $($traceFile.Length) bytes."
    }
    $tailBytes = [Math]::Min([int64]65536, $traceFile.Length)
    $stream = [IO.File]::Open($trace, [IO.FileMode]::Open,
        [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $stream.Seek(-$tailBytes, [IO.SeekOrigin]::End) | Out-Null
        $buffer = New-Object byte[] $tailBytes
        $read = $stream.Read($buffer, 0, $buffer.Length)
        $tail = [Text.Encoding]::UTF8.GetString($buffer, 0, $read)
    } finally {
        $stream.Dispose()
    }
    foreach ($marker in @('trace.growth_test.complete', 'hard_cap=0',
            '[component=main][event=session.end]')) {
        if (-not $tail.Contains($marker)) {
            throw "Growth trace tail is missing marker: $marker"
        }
    }
    $bytes = [IO.File]::ReadAllBytes($trace)
    if ([Array]::IndexOf($bytes, [byte]0) -ge 0) {
        throw 'Growth trace contains a preallocated NUL tail.'
    }
    Write-Host "DrunkDeer unbounded log growth: PASS ($($traceFile.Length) bytes)" `
        -ForegroundColor Green
}
finally {
    $resolved = [IO.Path]::GetFullPath($testRoot)
    $temp = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
    if ($resolved.StartsWith($temp, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force `
            -ErrorAction SilentlyContinue
    }
}
