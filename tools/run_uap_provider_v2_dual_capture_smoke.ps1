[CmdletBinding()]
param(
    [string]$ExePath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $root 'build\release\HallJoy.exe'
}
$ExePath = [IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "Production executable was not found: $ExePath"
}

try {
    $existing = @(
        Get-CimInstance Win32_Process -Filter "Name='HallJoy.exe'" -ErrorAction Stop
    )
}
catch {
    throw 'Cannot prove an isolated physical-UAP session because HallJoy process enumeration failed.'
}
if ($existing.Count -ne 0) {
    $owners = ($existing | ForEach-Object {
        $path = if ($_.ExecutablePath) { [string]$_.ExecutablePath } else { '<unknown>' }
        "PID=$($_.ProcessId) path=$path"
    }) -join '; '
    throw "Physical-UAP self-test requires every other HallJoy instance to be closed: $owners"
}

$hashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $ExePath).Hash
$process = Start-Process -FilePath $ExePath `
    -ArgumentList '--halljoy-test-uap-provider-v2-dual-capture' `
    -WorkingDirectory (Split-Path -Parent $ExePath) `
    -WindowStyle Hidden -PassThru
try {
    if (-not $process.WaitForExit(20000)) {
        throw 'Exact EXE dual-capture self-test exceeded 20 seconds.'
    }
}
finally {
    $process.Refresh()
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
}

if ($process.ExitCode -ne 0) {
    throw "Exact EXE dual-capture self-test failed with exit code $($process.ExitCode)."
}

$deadline = [DateTime]::UtcNow.AddSeconds(3)
do {
    $remaining = @(
        Get-CimInstance Win32_Process -Filter "Name='HallJoy.exe'" -ErrorAction SilentlyContinue |
            Where-Object {
                $_.ExecutablePath -and
                [IO.Path]::GetFullPath([string]$_.ExecutablePath) -eq $ExePath
            }
    )
    if ($remaining.Count -eq 0) { break }
    Start-Sleep -Milliseconds 100
} while ([DateTime]::UtcNow -lt $deadline)
if ($remaining.Count -ne 0) {
    throw 'Exact EXE dual-capture self-test left a parent or child process.'
}

$hashAfter = (Get-FileHash -Algorithm SHA256 -LiteralPath $ExePath).Hash
if ($hashAfter -ne $hashBefore) {
    throw 'Exact EXE changed during dual-capture self-test.'
}

Write-Host "UAP_PROVIDER_V2_EXACT_EXE_DUAL_CAPTURE=PASS sha256=$hashAfter" `
    -ForegroundColor Green
