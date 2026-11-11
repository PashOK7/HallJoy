[CmdletBinding()]
param(
    [switch]$CheckOnly,
    [switch]$PackageOnly,
    [string]$PythonPath,
    [string]$NsisPath,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$originalPath = $env:PATH
$problems = New-Object System.Collections.Generic.List[string]

try {
    if ($env:OS -ne 'Windows_NT' -or -not [Environment]::Is64BitOperatingSystem) {
        throw 'Build this installer on Windows x64.'
    }
    . (Join-Path $PSScriptRoot 'build_environment.ps1')
    $buildEnvironment = Initialize-HallJoyBuildEnvironment -PythonPath $PythonPath -RequireClang:(-not $PackageOnly)
    $PythonPath = $buildEnvironment.PythonPath

    if (-not $NsisPath) {
        $candidates = @($env:NSIS_EXE)
        foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
            if ($base) { $candidates += Join-Path $base 'NSIS/makensis.exe' }
        }
        $nsisCommand = Get-Command makensis.exe -ErrorAction SilentlyContinue
        if ($nsisCommand) { $candidates += $nsisCommand.Source }
        if ($env:LOCALAPPDATA) { $candidates += Join-Path $env:LOCALAPPDATA 'tauri/NSIS/makensis.exe' }
        $NsisPath = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
    }
    if (-not $NsisPath -or -not (Test-Path -LiteralPath $NsisPath -PathType Leaf)) {
        $problems.Add('NSIS 3 is missing. Install it or pass -NsisPath <makensis.exe>.')
    } else {
        $NsisPath = (Get-Item -LiteralPath $NsisPath).FullName
        $nsisVersion = & $NsisPath /VERSION
        if ($LASTEXITCODE -ne 0 -or ($nsisVersion -join '') -notmatch '^v?3\.') {
            $problems.Add('A working NSIS 3 compiler is required.')
        }
    }

    if (-not $PackageOnly) {
        if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) { $problems.Add('Git is required for pinned build dependencies.') }
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $vsRoot = $null
        if (Test-Path -LiteralPath $vswhere) {
            $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
        }
        if (-not $vsRoot) {
            $problems.Add('Install Visual Studio 2022 Desktop development with C++, including MSVC x64 and Windows SDK.')
        }
        if ($PythonPath -and (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
            Push-Location $root
            try {
                & $PythonPath -B -c 'import shutil, subprocess, sys; from tools.run_native_backend_checks import find_cxx; c=find_cxx(); sys.exit(1 if not c or not shutil.which(c) else subprocess.run([c,sys.argv[1]], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode)' --version
                if ($LASTEXITCODE -ne 0) { $problems.Add('Portable C++ checks need working clang++ or g++. Check PATH and CXX.') }
            } finally { Pop-Location }
        }
    } elseif (-not (Test-Path -LiteralPath (Join-Path $root 'build/bin/Release/x64/HallJoy.exe') -PathType Leaf)) {
        $problems.Add('No built HallJoy.exe found. Run BUILD.cmd without -PackageOnly first.')
    }
    if ($problems.Count) { throw ("Build prerequisites are incomplete:`n- " + ($problems -join "`n- ")) }
    if ($CheckOnly) {
        Write-Output 'Build prerequisites found. No compilation, packaging or installation was started.'
        return
    }
    if (-not $OutputDirectory) {
        $id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
        $OutputDirectory = Join-Path $root ('build/packages/windows-' + $id)
    }
    $OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
    if (Test-Path -LiteralPath $OutputDirectory) { throw 'Output already exists. Choose a new -OutputDirectory; earlier installers are preserved.' }
    if (-not $PackageOnly) {
        Write-Host 'Step 1/3: building HallJoy and pinned dependencies with the existing full build checks.'
        # A child process contains build.ps1's exit statement and preserves its failure status.
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'HallJoy build failed. No installer was created from an older executable.' }
    }
    Write-Host 'Step 2/3: packaging the verified application as a Windows installer.'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'package_windows_installer.ps1') -NsisPath $NsisPath -OutputDirectory $OutputDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Installer packaging failed. See the error above.' }
    Write-Host 'Step 3/3: placing the verified final executables directly in build.'
    & (Join-Path $PSScriptRoot 'publish_windows_outputs.ps1') -PackageDirectory $OutputDirectory
    Write-Host "Package archive and checksums: $OutputDirectory"
    Write-Host 'Open build\HallJoy-setup.exe to install HallJoy. Installation has not been started automatically.'
} finally {
    $env:PATH = $originalPath
}
