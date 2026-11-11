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
    if (-not $PythonPath) {
        $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
        if ($pythonCommand -and $pythonCommand.Source -notlike '*\Microsoft\WindowsApps\*') {
            $PythonPath = $pythonCommand.Source
        }
    }
    if (-not $PythonPath -or -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
        $problems.Add('Python is missing. Install Python 3.10+ on PATH, or pass -PythonPath <python.exe>.')
    } else {
        $PythonPath = (Get-Item -LiteralPath $PythonPath).FullName
        if ([IO.Path]::GetFileName($PythonPath) -ine 'python.exe') {
            throw '-PythonPath must point to python.exe; existing build tools invoke that name.'
        }
        & $PythonPath -B -c 'import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)'
        if ($LASTEXITCODE -ne 0) { $problems.Add('A working Python 3.10+ is required.') }
        $env:PATH = (Split-Path -Parent $PythonPath) + ';' + $env:PATH
    }

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
        $clangCommand = Get-Command clang.exe -ErrorAction SilentlyContinue
        $clangCandidates = @()
        if ($clangCommand) { $clangCandidates += $clangCommand.Source }
        if ($vsRoot) { $clangCandidates += Join-Path $vsRoot 'VC/Tools/Llvm/x64/bin/clang.exe' }
        $clangCandidates += Join-Path $env:ProgramFiles 'LLVM/bin/clang.exe'
        $clang = $clangCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
        if (-not $clang) {
            $problems.Add('Install C++ Clang tools for Windows in Visual Studio Installer (clang.exe and clang++.exe).')
        } else {
            $env:PATH = (Split-Path -Parent $clang) + ';' + $env:PATH
        }
        if ($PythonPath -and (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
            Push-Location $root
            try {
                & $PythonPath -B -c 'import shutil, subprocess, sys; from tools.run_native_backend_checks import find_cxx; c=find_cxx(); sys.exit(1 if not c or not shutil.which(c) else subprocess.run([c,"--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode)'
                if ($LASTEXITCODE -ne 0) { $problems.Add('Portable C++ checks need working clang++ or g++. Check PATH and CXX.') }
            } finally { Pop-Location }
        }
    } elseif (-not (Test-Path -LiteralPath (Join-Path $root 'build/bin/Release/x64/HallJoy.exe') -PathType Leaf)) {
        $problems.Add('No built HallJoy.exe found. Run Build-Windows.cmd without -PackageOnly first.')
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
        Write-Host 'Step 1/2: building HallJoy and pinned dependencies with the existing full build checks.'
        # A child process contains build.ps1's exit statement and preserves its failure status.
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'HallJoy build failed. No installer was created from an older executable.' }
    }
    Write-Host 'Step 2/2: packaging the verified application as a Windows installer.'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'package_windows_installer.ps1') -NsisPath $NsisPath -OutputDirectory $OutputDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Installer packaging failed. See the error above.' }
    Write-Host "Installer ready in: $OutputDirectory"
    Write-Host 'Open the setup.exe in that folder to install HallJoy. Installation has not been started automatically.'
} finally {
    $env:PATH = $originalPath
}
