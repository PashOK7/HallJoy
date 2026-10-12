[CmdletBinding()]
param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$release = Join-Path $root 'build/bin/Release/x64'
$exe = Join-Path $release 'HallJoy.exe'
$version = (Get-Item -LiteralPath $exe).VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw 'Unexpected executable version' }
# X.Y.Z.0 is released as X.Y.Z; a hotfix X.Y.Z.N keeps all four numbers.
$publicVersion = if ($version.EndsWith('.0')) { $version.Substring(0, $version.Length - 2) } else { $version }
$names = @('HallJoy.exe', 'LICENSE', 'dependency-lock.json', 'THIRD_PARTY_NOTICES.md', 'SHA256SUMS.txt')
$sources = @{
    'HallJoy.exe' = $exe
    'LICENSE' = (Join-Path $root 'LICENSE')
    'dependency-lock.json' = (Join-Path $root 'tools/dependency-lock.json')
    'THIRD_PARTY_NOTICES.md' = (Join-Path $root 'THIRD_PARTY_NOTICES.md')
}
& python (Join-Path $root 'tools/verify_embedded_licenses.py') $exe
if ($LASTEXITCODE -ne 0) { throw 'Executable legal resources are missing or stale; rebuild first.' }
$before = @{}
foreach ($name in $names) {
    if ($name -ne 'SHA256SUMS.txt') { $before[$name] = (Get-FileHash -LiteralPath $sources[$name] -Algorithm SHA256).Hash }
}
# Generate the checksum from this exact executable, never a stale previous build.
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $root ('build/packages/HallJoy-' + $publicVersion + '-Windows-x64')
}
$destination = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $destination) { throw 'Output already exists; never overwrite a release package' }
New-Item -ItemType Directory -Path $destination | Out-Null
$stage = Join-Path $destination 'HallJoy'
New-Item -ItemType Directory -Path $stage | Out-Null
foreach ($name in $names) {
    if ($name -eq 'SHA256SUMS.txt') {
        $checksum = Join-Path $stage $name
        [IO.File]::WriteAllText($checksum, $before['HallJoy.exe'] + '  HallJoy.exe' + "`r`n", [Text.Encoding]::ASCII)
        $before[$name] = (Get-FileHash -LiteralPath $checksum -Algorithm SHA256).Hash
        continue
    }
    $source = $sources[$name]
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $before[$name]) { throw "Source changed: $name" }
    Copy-Item -LiteralPath $source -Destination $stage
    if ((Get-FileHash -LiteralPath (Join-Path $stage $name) -Algorithm SHA256).Hash -ne $before[$name]) {
        throw "Staged file mismatch: $name"
    }
}
$archive = Join-Path $destination ('HallJoy-' + $publicVersion + '-Windows-x64.zip')
Compress-Archive -LiteralPath $stage -DestinationPath $archive
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $entries = @($zip.Entries | Where-Object { $_.Name })
    $expected = @($names | ForEach-Object { 'HallJoy/' + $_ } | Sort-Object)
    $actual = @($entries | ForEach-Object { $_.FullName.Replace('\', '/') } | Sort-Object)
    if (@(Compare-Object $expected $actual).Count -ne 0) { throw 'Unexpected archive contents' }
    foreach ($entry in $entries) {
        $stream = $entry.Open(); $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
        finally { $stream.Dispose(); $sha.Dispose() }
        if ($hash -ne $before[$entry.Name]) { throw "Archive file mismatch: $($entry.Name)" }
    }
} finally { $zip.Dispose() }
Write-Output "PACKAGE=$archive"
$archiveHash = Get-FileHash -LiteralPath $archive -Algorithm SHA256
$checksumPath = Join-Path $destination 'SHA256SUMS-release.txt'
if (Test-Path -LiteralPath $checksumPath) { throw 'Checksum output unexpectedly exists' }
[IO.File]::WriteAllText($checksumPath, $archiveHash.Hash + '  ' + [IO.Path]::GetFileName($archive) + "`r`n", [Text.Encoding]::ASCII)
$archiveHash
