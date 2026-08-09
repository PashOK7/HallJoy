[CmdletBinding()]
param(
    [string]$HallJoyRoot = '',
    [switch]$ExcludeMad68ProRNative,
    [string]$DependencyLock = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Root = Split-Path -Parent $PSScriptRoot
$SoupRoot = Join-Path $Root 'Soup'
$DistRoot = Join-Path $Root 'dist'
$PluginOut = Join-Path $DistRoot 'universal-analog-plugin'
$BuildToolsRoot = Join-Path $Root '.build-tools'
$SunRoot = Join-Path $BuildToolsRoot 'Sun'

if (-not $DependencyLock) {
    $DependencyLock = Join-Path $Root '..\..\tools\dependency-lock.json'
}
if (-not (Test-Path -LiteralPath $DependencyLock -PathType Leaf)) {
    throw "Dependency lock is missing: $DependencyLock"
}
$lock = Get-Content -LiteralPath $DependencyLock -Raw | ConvertFrom-Json
$SunRepository = [string]$lock.sources.sun.repository
$SunCommit = [string]$lock.sources.sun.commit
$SoupRepository = [string]$lock.sources.soup.repository
$SoupCommit = [string]$lock.sources.soup.commit
$SoupOverlayRoot = Join-Path $Root 'overlay\Soup\soup'
$SoupOverlayFiles = @($lock.sources.soup.patchedOverlayFiles.PSObject.Properties)

function Get-NormalizedTextSha256([string]$Path) {
    $text = [IO.File]::ReadAllText($Path).Replace("`r`n", "`n").Replace("`r", "`n")
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes($text)
    return [BitConverter]::ToString(
        [Security.Cryptography.SHA256]::Create().ComputeHash($bytes)).Replace('-', '')
}

function Require-Command([string]$Name, [string]$Message) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw $Message
    }
}

function Find-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $msbuild = $null

    if (Test-Path -LiteralPath $vswhere) {
        $msbuild = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -find 'MSBuild\**\Bin\amd64\MSBuild.exe' |
            Select-Object -First 1
    }

    if (-not $msbuild) {
        $candidates = @(
            'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe',
            'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe',
            'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe',
            'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
        )
        $msbuild = $candidates |
            Where-Object { Test-Path -LiteralPath $_ } |
            Select-Object -First 1
    }

    if (-not $msbuild) {
        throw 'MSBuild x64 was not found. Install Visual Studio 2022 with Desktop development with C++.'
    }

    return [string]$msbuild
}


function Resolve-ClangX64 {
    $candidates = New-Object System.Collections.Generic.List[string]

    $fromPath = Get-Command 'clang.exe' -ErrorAction SilentlyContinue
    if ($fromPath -and $fromPath.Path) {
        $candidates.Add([string]$fromPath.Path)
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        @(& $vswhere -latest -products * -find 'VC\Tools\Llvm\x64\bin\clang.exe') |
            Where-Object { $_ } | ForEach-Object { $candidates.Add([string]$_) }
    }

    @(
        'C:\BuildTools\VC\Tools\Llvm\x64\bin\clang.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\x64\bin\clang.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\x64\bin\clang.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin\clang.exe',
        'C:\Program Files\LLVM\bin\clang.exe'
    ) | ForEach-Object { $candidates.Add($_) }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { continue }
        try {
            $probe = & $candidate --version 2>&1
            if ($LASTEXITCODE -eq 0 -and ($probe -join "`n") -match 'clang version') {
                $dir = Split-Path -Parent $candidate
                $env:Path = "$dir;$env:Path"
                Write-Host "Using x64 Clang: $candidate" -ForegroundColor DarkGray
                return [string]$candidate
            }
        }
        catch {
            Write-Host "Ignoring unusable Clang candidate: $candidate" -ForegroundColor DarkYellow
        }
    }

    throw 'A working x64 clang.exe was not found. Install C++ Clang tools for Windows in Visual Studio Installer.'
}

function Resolve-SunExecutable {
    Require-Command 'git' 'Git is required to bootstrap the Sun build tool and download Soup.'
    $msbuild = Find-MSBuild

    $currentSunCommit = ''
    if (Test-Path -LiteralPath (Join-Path $SunRoot '.git')) {
        $currentSunCommit = (& git -C $SunRoot rev-parse HEAD 2>$null | Select-Object -First 1)
    }
    if (-not (Test-Path -LiteralPath (Join-Path $SunRoot 'Sun.sln')) -or
        $currentSunCommit -ne $SunCommit) {
        if (Test-Path -LiteralPath $SunRoot) {
            Remove-Item -LiteralPath $SunRoot -Recurse -Force
        }
        New-Item -ItemType Directory -Path $BuildToolsRoot -Force | Out-Null

        Write-Host "Downloading pinned Sun revision $SunCommit..." -ForegroundColor Cyan
        New-Item -ItemType Directory -Path $SunRoot -Force | Out-Null
        & git -C $SunRoot init | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'git init for Sun failed.' }
        & git -C $SunRoot remote add origin $SunRepository
        if ($LASTEXITCODE -ne 0) { throw 'git remote add for Sun failed.' }
        & git -C $SunRoot fetch --depth 1 origin $SunCommit | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'git fetch for pinned Sun revision failed.' }
        & git -C $SunRoot checkout --detach FETCH_HEAD | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'git checkout for pinned Sun revision failed.' }
        & git -C $SunRoot submodule update --init --recursive --depth 1 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'Sun submodule initialization failed.' }
    }
    else {
        Write-Host "Using pinned Sun revision $SunCommit." -ForegroundColor DarkGray
        $sunTrackedChanges = @(& git -C $SunRoot status --porcelain --untracked-files=no)
        if ($sunTrackedChanges.Count -ne 0) {
            throw 'Pinned Sun source has local tracked changes. Refusing a non-reproducible build.'
        }
        & git -C $SunRoot submodule update --init --recursive --depth 1 | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "Could not initialise Sun submodules. Git exit code: $LASTEXITCODE"
        }
    }

    Write-Host 'Building Sun with Visual Studio 2022/MSVC...' -ForegroundColor Cyan
    & $msbuild (Join-Path $SunRoot 'Sun.sln') `
        '/t:Rebuild' `
        '/p:Configuration=Release' `
        '/p:Platform=x64' `
        '/m' | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Sun bootstrap build failed with exit code $LASTEXITCODE"
    }

    $sunCandidates = @(
        (Join-Path $SunRoot 'bin\Release\Sun.exe'),
        (Join-Path $SunRoot 'bin\Release\sun.exe')
    )
    $sunExe = $sunCandidates |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1

    if (-not $sunExe) {
        $sunExe = Get-ChildItem -LiteralPath (Join-Path $SunRoot 'bin') `
            -Filter 'Sun.exe' -File -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
    }

    if (-not $sunExe -or -not (Test-Path -LiteralPath $sunExe)) {
        throw "Sun was built, but Sun.exe was not found below: $SunRoot\bin"
    }

    Write-Host "Bootstrapped Sun: $sunExe" -ForegroundColor Green
    return [string]$sunExe
}

Require-Command 'git' 'Git is required to download the Soup source.'
$SunExe = Resolve-SunExecutable
$null = Resolve-ClangX64

if ($SoupOverlayFiles.Count -eq 0) {
    throw 'The dependency lock contains no Soup overlay files.'
}
foreach ($entry in $SoupOverlayFiles) {
    $overlayPath = Join-Path $SoupOverlayRoot $entry.Name
    if (-not (Test-Path -LiteralPath $overlayPath -PathType Leaf)) {
        throw "Soup overlay file is missing: $overlayPath"
    }
    $actualHash = Get-NormalizedTextSha256 $overlayPath
    if ($actualHash -ne [string]$entry.Value) {
        throw "Soup overlay integrity failed for $($entry.Name). Expected=$($entry.Value) Actual=$actualHash"
    }
}

$currentSoupCommit = ''
if (Test-Path -LiteralPath (Join-Path $SoupRoot '.git')) {
    $currentSoupCommit = (& git -C $SoupRoot rev-parse HEAD 2>$null | Select-Object -First 1)
}
if (-not (Test-Path -LiteralPath (Join-Path $SoupRoot 'soup\AnalogueKeyboard.cpp')) -or
    $currentSoupCommit -ne $SoupCommit) {
    if (Test-Path -LiteralPath $SoupRoot) {
        Remove-Item -LiteralPath $SoupRoot -Recurse -Force
    }
    Write-Host "Downloading pinned Soup revision $SoupCommit..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path $SoupRoot -Force | Out-Null
    & git -C $SoupRoot init
    if ($LASTEXITCODE -ne 0) { throw 'git init for Soup failed.' }
    & git -C $SoupRoot remote add origin $SoupRepository
    if ($LASTEXITCODE -ne 0) { throw 'git remote add for Soup failed.' }
    & git -C $SoupRoot fetch --depth 1 origin $SoupCommit
    if ($LASTEXITCODE -ne 0) { throw 'git fetch for pinned Soup revision failed.' }
    & git -C $SoupRoot checkout --detach FETCH_HEAD
    if ($LASTEXITCODE -ne 0) { throw 'git checkout for pinned Soup revision failed.' }
}

foreach ($entry in $SoupOverlayFiles) {
    Copy-Item -LiteralPath (Join-Path $SoupOverlayRoot $entry.Name) `
        -Destination (Join-Path $SoupRoot "soup\$($entry.Name)") -Force
}

$expectedChangedFiles = @($SoupOverlayFiles | ForEach-Object { "soup/$($_.Name)" } | Sort-Object)
$actualChangedFiles = @(& git -C $SoupRoot diff --name-only HEAD -- | Sort-Object)
if (($expectedChangedFiles -join "`n") -ne ($actualChangedFiles -join "`n")) {
    throw "Patched Soup file set differs from the locked overlay. Actual=$($actualChangedFiles -join ',')"
}
$soupUntracked = @(& git -C $SoupRoot status --porcelain --untracked-files=all |
    Where-Object { [string]$_ -match '^\?\?' })
if ($soupUntracked.Count -ne 0) {
    throw "Pinned Soup source has unexpected untracked files: $($soupUntracked -join ',')"
}
foreach ($entry in $SoupOverlayFiles) {
    $patchedPath = Join-Path $SoupRoot "soup\$($entry.Name)"
    $actualHash = Get-NormalizedTextSha256 $patchedPath
    if ($actualHash -ne [string]$entry.Value) {
        throw "Patched Soup integrity failed for $($entry.Name). Expected=$($entry.Value) Actual=$actualHash"
    }
}
Write-Host "Verified $($SoupOverlayFiles.Count) locked Soup overlay files." -ForegroundColor DarkGray

if (Test-Path -LiteralPath $DistRoot) {
    Remove-Item -LiteralPath $DistRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $PluginOut -Force | Out-Null

Push-Location $Root
try {
    if ($ExcludeMad68ProRNative) {
        Write-Host 'Building UAP with runtime-validated MADLIONS PID exclusion for the native HallJoy backend.' -ForegroundColor DarkGray
        $targets = @(
            @{ Sun = 'abiv0-mad68native'; Output = 'abiv0' },
            # The private HallJoy host bypasses the Wooting SDK, so ABI1 itself must
            # include Wooting-device support in addition to Madlions/Soup devices.
            @{ Sun = 'abiv1-pluswooting-mad68native'; Output = 'abiv1' }
        )
    }
    else {
        # Normal HallJoy/plugin builds remain bit-for-bit source-compatible and do
        # not exclude any device merely because the native MAD68 backend exists in
        # this source tree.
        $targets = @(
            @{ Sun = 'abiv0'; Output = 'abiv0' },
            @{ Sun = 'abiv1-pluswooting'; Output = 'abiv1' }
        )
    }
    foreach ($entry in $targets) {
        $target = [string]$entry.Sun
        $output = [string]$entry.Output
        Write-Host "Building $target.dll as $output.dll..." -ForegroundColor Cyan
        & $SunExe $target
        if ($LASTEXITCODE -ne 0) {
            throw "Sun build for $target failed with exit code $LASTEXITCODE"
        }
        $dll = Join-Path $Root "$target.dll"
        if (-not (Test-Path -LiteralPath $dll)) {
            throw "Expected output not found: $dll"
        }
        Move-Item -LiteralPath $dll -Destination (Join-Path $PluginOut "$output.dll") -Force
        Remove-Item -LiteralPath (Join-Path $Root "$target.exp") -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath (Join-Path $Root "$target.lib") -Force -ErrorAction SilentlyContinue
    }
}
finally {
    Pop-Location
}

$package = Join-Path $DistRoot 'UniversalAnalogPlugin-HallJoy-Madlions-Fix.zip'
Compress-Archive -Path $PluginOut -DestinationPath $package -Force

Write-Host ''
Write-Host 'Plugin build completed:' -ForegroundColor Green
Write-Host "  $PluginOut\abiv0.dll"
Write-Host "  $PluginOut\abiv1.dll"
Write-Host "  $package"

if ($HallJoyRoot) {
    $HallJoyRoot = (Resolve-Path -LiteralPath $HallJoyRoot).Path
    $runtime = Join-Path $HallJoyRoot 'runtime'
    $halljoyBuild = Join-Path $HallJoyRoot 'tools\build_madlions_diagnostic.ps1'
    if (-not (Test-Path -LiteralPath $runtime) -or -not (Test-Path -LiteralPath $halljoyBuild)) {
        throw "The supplied HallJoyRoot is not the complete HallJoy SDK 0.9.1 project: $HallJoyRoot"
    }

    Copy-Item -LiteralPath (Join-Path $PluginOut 'abiv0.dll') -Destination (Join-Path $runtime 'universal_analog_abiv0.dll') -Force
    Copy-Item -LiteralPath (Join-Path $PluginOut 'abiv1.dll') -Destination (Join-Path $runtime 'universal_analog_abiv1.dll') -Force

    Write-Host ''
    Write-Host 'Updated HallJoy embedded plugin DLLs. Building HallJoy...' -ForegroundColor Cyan
    & powershell -NoProfile -ExecutionPolicy Bypass -File $halljoyBuild
    if ($LASTEXITCODE -ne 0) {
        throw "HallJoy build failed with exit code $LASTEXITCODE"
    }
}
