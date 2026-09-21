[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$CandidatePath,
    [Parameter(Mandatory = $true)][string]$TargetPath
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$candidate = (Resolve-Path -LiteralPath $CandidatePath).Path
$target = [IO.Path]::GetFullPath($TargetPath)
$root = Split-Path -Parent $PSScriptRoot
$prefix = [IO.Path]::GetFullPath($root).TrimEnd('\') + '\'
if (-not $target.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or $target -eq $candidate) {
    throw 'Use a distinct candidate and a target inside the checkout.'
}
$hash = (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash
if ((Test-Path -LiteralPath $target) -and (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -eq $hash) {
    Write-Output 'Candidate matches installed EXE; running HallJoy left untouched.'
    return
}
$parent = Split-Path -Parent $target
$null = New-Item -ItemType Directory -Path $parent -Force
$pending = Join-Path $parent ('.HallJoy-' + [guid]::NewGuid().ToString('N') + '.pending')
$backupDir = Join-Path $root 'build\obj\replacement-backups'
$null = New-Item -ItemType Directory -Path $backupDir -Force
$backup = Join-Path $backupDir ('HallJoy-' + [guid]::NewGuid().ToString('N') + '.exe')
$state = $null
$shutdownAttempted = $false
try {
    # Prepare and verify the replacement before touching a running app.
    Copy-Item -LiteralPath $candidate -Destination $pending
    if ((Get-FileHash -LiteralPath $pending -Algorithm SHA256).Hash -ne $hash) { throw 'Candidate copy hash mismatch.' }
    $state = & (Join-Path $PSScriptRoot 'close_project_halljoy.ps1') -TargetPath $target -InspectOnly
    $shutdownAttempted = $true
    $null = & (Join-Path $PSScriptRoot 'close_project_halljoy.ps1') -TargetPath $target
    if (Test-Path -LiteralPath $target) {
        [IO.File]::Replace($pending, $target, $backup)
    } else {
        [IO.File]::Move($pending, $target)
    }
    Write-Output "Installed verified build: $target"
} finally {
    if (Test-Path -LiteralPath $pending) { Remove-Item -LiteralPath $pending }
    # Also restore the old app after an atomic replacement failure. A failed
    # compile never reaches this script; orphaned workers do not trigger launch.
    if ($shutdownAttempted -and $state -and $state.WasRunning -and (Test-Path -LiteralPath $target)) {
        $now = & (Join-Path $PSScriptRoot 'close_project_halljoy.ps1') -TargetPath $target -InspectOnly
        if (-not $now.WasRunning) {
            $null = Start-Process -FilePath $target -WorkingDirectory $parent -WindowStyle Normal -PassThru
            Write-Output 'Restored the previously running HallJoy window.'
        }
    }
}
