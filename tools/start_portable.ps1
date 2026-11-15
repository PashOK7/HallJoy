[CmdletBinding()]
param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
try {
    $directory = Join-Path (Split-Path -Parent $PSScriptRoot) '.local/portable'
    $exe = Join-Path $directory 'HallJoy.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
        throw 'Portable HallJoy is missing. Run Update.cmd or follow docs/PORTABLE.md.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $directory 'HallJoy.portable') -PathType Leaf)) {
        throw 'HallJoy.portable is missing. Restore the marker before starting to keep your data in this folder.'
    }
    if ($CheckOnly) { Write-Output 'Portable executable and storage marker are present.'; return }
    Start-Process -FilePath $exe -WorkingDirectory $directory
} catch {
    Write-Error $_.Exception.Message
    exit 1
}
