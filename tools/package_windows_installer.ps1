[CmdletBinding()]
param(
    [string]$ExecutablePath,
    [string]$OutputDirectory,
    [string]$NsisPath
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
if (-not $ExecutablePath) { $ExecutablePath = Join-Path $root 'build/bin/Release/x64/HallJoy.exe' }
$exe = (Get-Item -LiteralPath $ExecutablePath).FullName
$version = (Get-Item -LiteralPath $exe).VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw 'Unexpected executable version.' }
$versionHeader = Get-Content -LiteralPath (Join-Path $root 'src/HallJoyProject/HallJoy/version.h') -Raw
if ($versionHeader -notmatch '#define HALLJOY_VERSION_STRING_FULL "([0-9.]+)"' -or $version -ne $Matches[1]) {
    throw 'Executable version does not match this checkout. Build the current source first.'
}
$publicVersion = if ($version.EndsWith('.0')) { $version.Substring(0, $version.Length - 2) } else { $version }
if (-not $NsisPath) {
    $candidates = @($env:NSIS_EXE, (Join-Path ${env:ProgramFiles(x86)} 'NSIS/makensis.exe'))
    $command = Get-Command makensis.exe -ErrorAction SilentlyContinue
    if ($command) { $candidates += $command.Source }
    $NsisPath = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
}
if (-not $NsisPath) { throw 'NSIS 3 is required. Use -NsisPath or NSIS_EXE; nothing is installed automatically.' }
$NsisPath = (Get-Item -LiteralPath $NsisPath).FullName
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root ('build/packages/HallJoy-' + $publicVersion + '-Windows-x64-setup') }
$destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
if (Test-Path -LiteralPath $destination) { throw 'Output already exists; never overwrite a release package.' }
if ($root -match '[\$"\r\n]' -or $destination -match '[\$"\r\n]') { throw 'NSIS paths must not contain dollar signs, quotes or newlines.' }

# Use the same exact embedded-runtime/license gate as package_release.ps1.
# This reads PE resources without launching HallJoy or probing a keyboard.
& python (Join-Path $root 'tools/verify_embedded_licenses.py') $exe
if ($LASTEXITCODE -ne 0) { throw 'Executable resources do not match this checkout; rebuild first.' }
$payload = Join-Path $root ('build/obj/windows-installer/' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $payload -Force | Out-Null
$inputs = [ordered]@{
    'HallJoy.exe' = $exe
    'HallJoy.ico' = (Join-Path $root 'src/HallJoyProject/HallJoy/HallJoy.ico')
    'LICENSE' = (Join-Path $root 'LICENSE')
    'THIRD_PARTY_NOTICES.md' = (Join-Path $root 'THIRD_PARTY_NOTICES.md')
    'dependency-lock.json' = (Join-Path $root 'tools/dependency-lock.json')
    'INSTALLATION.txt' = (Join-Path $root 'tools/installer/INSTALLATION.txt')
}
$inventory = @()
foreach ($name in $inputs.Keys) {
    $source = $inputs[$name]
    $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $target = Join-Path $payload $name
    Copy-Item -LiteralPath $source -Destination $target
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hash) { throw "Staged file mismatch: $name" }
    $inventory += @{ path = $name; sha256 = $hash.ToLowerInvariant() }
}
$name = 'HallJoy-' + $publicVersion + '-Windows-x64-setup.exe'
$temporaryOutput = Join-Path $payload $name
& $NsisPath /V2 /WX "/DPAYLOAD=$payload" "/DOUTPUT=$temporaryOutput" "/DVERSION=$version" (Join-Path $PSScriptRoot 'installer/windows.nsi')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $temporaryOutput)) { throw 'NSIS compilation failed.' }
# Publish locally only after successful compilation; never overwrite an older package.
New-Item -ItemType Directory -Path $destination | Out-Null
$output = Join-Path $destination $name
Move-Item -LiteralPath $temporaryOutput -Destination $output
$digest = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($output + '.sha256'), "$digest  $name`r`n", [Text.Encoding]::ASCII)
@{ installer = $name; version = $version; sha256 = $digest; payload = $inventory } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath ($output + '.json') -Encoding UTF8
Write-Output "PACKAGE=$output"
Write-Output "PAYLOAD=$payload"
Write-Output "SHA256=$digest"
Write-Output 'Unsigned package built. No application, driver installer or publication was started.'
