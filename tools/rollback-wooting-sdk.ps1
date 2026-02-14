param(
    [string]$Tag = "",
    [switch]$ListOnly,
    [switch]$IncludePrerelease,
    [string]$RepoRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Write-Info([string]$msg) {
    Write-Host "[wooting-rollback] $msg"
}

function Resolve-RepoRoot([string]$given) {
    if ($given -and (Test-Path -LiteralPath $given)) {
        return (Resolve-Path -LiteralPath $given).Path
    }
    $fromScript = Split-Path -Parent $PSScriptRoot
    if (Test-Path -LiteralPath (Join-Path $fromScript "HallJoy.sln")) {
        return $fromScript
    }
    if (Test-Path -LiteralPath "HallJoy.sln") {
        return (Get-Location).Path
    }
    throw "Cannot resolve repository root. Run from repo root or pass -RepoRoot."
}

function Get-Releases([bool]$includePrerelease) {
    $uri = "https://api.github.com/repos/WootingKb/wooting-analog-sdk/releases?per_page=100"
    $releases = Invoke-RestMethod -Uri $uri -Headers @{ "User-Agent" = "HallJoy-RollbackScript" }
    if (-not $includePrerelease) {
        $releases = @($releases | Where-Object { -not $_.prerelease -and -not $_.draft })
    }
    return @($releases)
}

function Pick-Release($releases, [string]$tag) {
    if ($tag) {
        $one = $releases | Where-Object { $_.tag_name -eq $tag } | Select-Object -First 1
        if (-not $one) {
            throw "Tag '$tag' not found. Use -ListOnly to see available tags."
        }
        return $one
    }
    $stable = $releases | Select-Object -First 1
    if (-not $stable) {
        throw "No releases found from GitHub API."
    }
    return $stable
}

function Pick-WindowsTarAsset($release) {
    $asset = $release.assets | Where-Object {
        $_.name -match "x86_64-pc-windows-msvc\.tar\.gz$"
    } | Select-Object -First 1
    if (-not $asset) {
        throw "Could not find windows msvc tar.gz asset in release '$($release.tag_name)'."
    }
    return $asset
}

function Ensure-Dir([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) {
        New-Item -ItemType Directory -Path $path | Out-Null
    }
}

function Backup-IfExists([string]$src, [string]$backupDir) {
    if (Test-Path -LiteralPath $src) {
        Ensure-Dir $backupDir
        Copy-Item -LiteralPath $src -Destination (Join-Path $backupDir (Split-Path -Leaf $src)) -Force
    }
}

$root = Resolve-RepoRoot $RepoRoot
$runtimeDir = Join-Path $root "runtime"
$x64DebugDir = Join-Path $root "x64\Debug"
$x64ReleaseDir = Join-Path $root "x64\Release"

Ensure-Dir $runtimeDir

$releases = Get-Releases $IncludePrerelease
if ($ListOnly) {
    Write-Host "Available tags:"
    $releases | Select-Object -First 30 | ForEach-Object {
        "{0}  ({1})" -f $_.tag_name, $_.published_at
    }
    exit 0
}

$release = Pick-Release $releases $Tag
$asset = Pick-WindowsTarAsset $release
$tag = $release.tag_name

Write-Info "Selected release: $tag"
Write-Info "Asset: $($asset.name)"

$tmpRoot = Join-Path $env:TEMP ("HallJoy-WootingRollback-" + [Guid]::NewGuid().ToString("N"))
Ensure-Dir $tmpRoot

$archivePath = Join-Path $tmpRoot $asset.name
$extractDir = Join-Path $tmpRoot "extract"
Ensure-Dir $extractDir

Write-Info "Downloading..."
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $archivePath

Write-Info "Extracting..."
tar -xf $archivePath -C $extractDir

$wrapperDll = Join-Path $extractDir "wrapper\wooting_analog_wrapper.dll"
$sdkDll = Join-Path $extractDir "wrapper\sdk\wooting_analog_sdk.dll"
$pluginDll = Join-Path $extractDir "wrapper\sdk\wooting_analog_plugin.dll"
$testPluginDll = Join-Path $extractDir "wrapper\sdk\wooting_analog_test_plugin.dll"

if (-not (Test-Path -LiteralPath $wrapperDll)) {
    throw "Extracted archive does not contain wrapper DLL at expected path."
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backupDir = Join-Path $runtimeDir ("backup\$timestamp-$tag")
Ensure-Dir $backupDir

$targets = @(
    (Join-Path $runtimeDir "wooting_analog_wrapper.dll"),
    (Join-Path $runtimeDir "wooting_analog_sdk.dll"),
    (Join-Path $runtimeDir "wooting_analog_plugin.dll"),
    (Join-Path $runtimeDir "wooting_analog_test_plugin.dll")
)

foreach ($t in $targets) {
    Backup-IfExists $t $backupDir
}

Write-Info "Installing runtime DLLs into: $runtimeDir"
Copy-Item -LiteralPath $wrapperDll -Destination (Join-Path $runtimeDir "wooting_analog_wrapper.dll") -Force
if (Test-Path -LiteralPath $sdkDll) {
    Copy-Item -LiteralPath $sdkDll -Destination (Join-Path $runtimeDir "wooting_analog_sdk.dll") -Force
}
if (Test-Path -LiteralPath $pluginDll) {
    Copy-Item -LiteralPath $pluginDll -Destination (Join-Path $runtimeDir "wooting_analog_plugin.dll") -Force
}
if (Test-Path -LiteralPath $testPluginDll) {
    Copy-Item -LiteralPath $testPluginDll -Destination (Join-Path $runtimeDir "wooting_analog_test_plugin.dll") -Force
}

foreach ($binDir in @($x64DebugDir, $x64ReleaseDir)) {
    if (-not (Test-Path -LiteralPath $binDir)) { continue }
    Ensure-Dir $binDir
    Backup-IfExists (Join-Path $binDir "wooting_analog_wrapper.dll") $backupDir
    Backup-IfExists (Join-Path $binDir "wooting_analog_sdk.dll") $backupDir
    Backup-IfExists (Join-Path $binDir "wooting_analog_plugin.dll") $backupDir
    Backup-IfExists (Join-Path $binDir "wooting_analog_test_plugin.dll") $backupDir
    Copy-Item -LiteralPath $wrapperDll -Destination (Join-Path $binDir "wooting_analog_wrapper.dll") -Force
    if (Test-Path -LiteralPath $sdkDll) {
        Copy-Item -LiteralPath $sdkDll -Destination (Join-Path $binDir "wooting_analog_sdk.dll") -Force
    }
    if (Test-Path -LiteralPath $pluginDll) {
        Copy-Item -LiteralPath $pluginDll -Destination (Join-Path $binDir "wooting_analog_plugin.dll") -Force
    }
    if (Test-Path -LiteralPath $testPluginDll) {
        Copy-Item -LiteralPath $testPluginDll -Destination (Join-Path $binDir "wooting_analog_test_plugin.dll") -Force
    }
}

Write-Info "Done."
Write-Info "Installed tag: $tag"
Write-Info "Backup folder: $backupDir"
Write-Info "Tip: rebuild in VS (Release|x64) and run HallJoy again."
