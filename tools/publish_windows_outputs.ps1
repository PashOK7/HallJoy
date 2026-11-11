[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$PackageDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
$packages = @(Get-ChildItem -LiteralPath $PackageDirectory -Filter '*-setup.exe' -File)
if ($packages.Count -ne 1) { throw 'Expected exactly one completed installer package.' }
$package = $packages[0]
$inventory = Get-Content -LiteralPath ($package.FullName + '.json') -Raw | ConvertFrom-Json
$appRecord = @($inventory.payload | Where-Object { $_.path -eq 'HallJoy.exe' })
if ($inventory.installer -ne $package.Name -or $inventory.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
    $appRecord.Count -ne 1 -or $appRecord[0].sha256 -notmatch '^[0-9a-fA-F]{64}$') {
    throw 'Invalid completed-package inventory.'
}
$stage = Join-Path $build ('obj/final-output/' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $stage -Force
$setupCandidate = Join-Path $stage 'HallJoy-setup.exe'
$appCandidate = Join-Path $stage 'HallJoy.exe'
$setupTarget = Join-Path $build 'HallJoy-setup.exe'
$appTarget = Join-Path $build 'HallJoy.exe'
try {
    # Snapshot both inputs, then verify the bytes that will actually be delivered.
    Copy-Item -LiteralPath $package.FullName -Destination $setupCandidate
    Copy-Item -LiteralPath (Join-Path $build 'bin/Release/x64/HallJoy.exe') -Destination $appCandidate
    if ((Get-FileHash -LiteralPath $setupCandidate -Algorithm SHA256).Hash -ne $inventory.sha256 -or
        (Get-FileHash -LiteralPath $appCandidate -Algorithm SHA256).Hash -ne $appRecord[0].sha256) {
        throw 'Final-output hash mismatch. Existing build-root outputs were not changed.'
    }
    # Detect an already open/locked setup before replacing the application.
    if (Test-Path -LiteralPath $setupTarget) {
        $handle = [IO.File]::Open($setupTarget, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        $handle.Dispose()
    }
    & (Join-Path $PSScriptRoot 'publish_halljoy_build.ps1') -CandidatePath $appCandidate -TargetPath $appTarget
    # Each output is replaced atomically. Keep the previous installer internally.
    if (Test-Path -LiteralPath $setupTarget) {
        [IO.File]::Replace($setupCandidate, $setupTarget, (Join-Path $stage 'previous-setup.exe'))
    } else {
        [IO.File]::Move($setupCandidate, $setupTarget)
    }
    Write-Host "Installer ready: $setupTarget"
    Write-Host "Application ready: $appTarget"
} finally {
    foreach ($temporary in @($setupCandidate, $appCandidate)) {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) { Remove-Item -LiteralPath $temporary }
    }
}
