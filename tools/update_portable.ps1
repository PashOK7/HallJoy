[CmdletBinding()]
param(
    [switch]$CheckOnly,
    [string]$Directory = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$stage = $null
$replaced = $false
try {
    if (-not $Directory) { $Directory = Join-Path (Split-Path -Parent $PSScriptRoot) '.local/portable' }
    $directoryPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Directory)
    $target = Join-Path $directoryPath 'HallJoy.exe'
    $marker = Join-Path $directoryPath 'HallJoy.portable'
    # Refuse links and non-portable destinations, including an installed copy.
    $ancestor = $directoryPath
    while ($ancestor) {
        if (Test-Path -LiteralPath $ancestor) {
            if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Portable update paths must not use symlinks or junctions.'
            }
        }
        $ancestor = Split-Path -Parent $ancestor
    }
    if ((Test-Path -LiteralPath $directoryPath) -and
        @(Get-ChildItem -LiteralPath $directoryPath -Force).Count -gt 0 -and
        -not (Test-Path -LiteralPath $marker -PathType Leaf)) {
        throw 'Nonempty destination is not a portable HallJoy folder. Nothing was changed.'
    }
    foreach ($path in @($target, $marker, (Join-Path $directoryPath 'release.json'), (Join-Path $directoryPath 'backups'))) {
        if ((Test-Path -LiteralPath $path) -and
            ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Portable update files must not use symlinks or junctions.'
        }
    }
    $release = Invoke-RestMethod -Uri 'https://api.github.com/repos/PashOK7/HallJoy/releases/latest' -Headers @{ 'User-Agent' = 'HallJoy-Portable-Updater' }
    if ($release.tag_name -notmatch '^v\d+\.\d+\.\d+(\.\d+)?$' -or $release.draft -or $release.prerelease) {
        throw 'Expected a stable HallJoy release.'
    }
    $assets = @($release.assets | Where-Object { $_.name -ceq 'HallJoy.exe' })
    if ($assets.Count -ne 1) { throw 'Release must contain exactly one HallJoy.exe.' }
    $asset = $assets[0]
    if ($asset.digest -notmatch '^sha256:([a-fA-F0-9]{64})$') { throw 'No valid SHA-256 digest; update cancelled.' }
    $expected = $Matches[1].ToLowerInvariant()
    $uri = [Uri]$asset.browser_download_url
    if ($uri.Scheme -ne 'https' -or $uri.Host -ne 'github.com' -or $uri.UserInfo -or
        $uri.AbsolutePath -cne ('/PashOK7/HallJoy/releases/download/' + $release.tag_name + '/HallJoy.exe') -or $uri.Query -or $uri.Fragment) {
        throw 'Unexpected release download URL.'
    }
    Write-Output ('Latest release: ' + $release.tag_name)
    if ((Test-Path -LiteralPath $target -PathType Leaf) -and
        (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -eq $expected) {
        Write-Output 'Already up to date (SHA-256 verified).'
        return
    }
    if ($CheckOnly) { Write-Output 'Update available. Run Update.cmd to install it.'; return }
    if (Get-Process -Name HallJoy -ErrorAction SilentlyContinue) { throw 'Close HallJoy before updating.' }
    New-Item -ItemType Directory -Path $directoryPath -Force | Out-Null
    $stage = Join-Path $directoryPath ('.update-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage | Out-Null
    $download = Join-Path $stage 'HallJoy.exe'
    Invoke-WebRequest -Uri $uri.AbsoluteUri -OutFile $download -UseBasicParsing
    if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
        throw 'SHA-256 mismatch. Existing executable and settings were not changed.'
    }
    if (Get-Process -Name HallJoy -ErrorAction SilentlyContinue) { throw 'HallJoy started during download. Close it and retry.' }
    $backup = Join-Path $directoryPath ('backups/update-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    foreach ($name in @('HallJoy.exe', 'release.json')) {
        $old = Join-Path $directoryPath $name
        if (Test-Path -LiteralPath $old -PathType Leaf) { Copy-Item -LiteralPath $old -Destination (Join-Path $backup $name) }
    }
    if (-not (Test-Path -LiteralPath $marker)) { New-Item -ItemType File -Path $marker | Out-Null }
    # A typed null avoids PowerShell 5.1 treating the backup path as an empty string.
    if (Test-Path -LiteralPath $target) { [IO.File]::Replace($download, $target, [System.Management.Automation.Language.NullString]::Value) }
    else { [IO.File]::Move($download, $target) }
    $replaced = $true
    @{ version = $release.tag_name; sha256 = $expected; url = $uri.AbsoluteUri } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directoryPath 'release.json') -Encoding UTF8
    Write-Output ('Updated portable HallJoy. Previous files: ' + $backup)
    Write-Output 'Settings and profiles were preserved. Run Start.cmd.'
} catch {
    if ($replaced) { Write-Error ('The EXE was updated, but recording release metadata failed: ' + $_.Exception.Message) }
    else { Write-Error ('Portable update failed: ' + $_.Exception.Message) }
    exit 1
} finally {
    if ($stage -and (Test-Path -LiteralPath $stage)) {
        $remaining = Join-Path $stage 'HallJoy.exe'
        if (Test-Path -LiteralPath $remaining) { Remove-Item -LiteralPath $remaining }
        Remove-Item -LiteralPath $stage
    }
}
