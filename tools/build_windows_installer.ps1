[CmdletBinding()]
param([switch]$CheckOnly,[string]$PythonPath,[string]$NsisPath,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $PSScriptRoot
$originalPath=$env:PATH
try {
    if($env:OS -ne 'Windows_NT' -or -not [Environment]::Is64BitOperatingSystem){throw 'Package the installer on Windows x64.'}
    . (Join-Path $PSScriptRoot 'build_environment.ps1')
    $environment=Initialize-HallJoyBuildEnvironment -PythonPath $PythonPath
    $exe=Join-Path $root 'build/bin/Release/x64/HallJoy.exe'
    if(-not (Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Build the portable HallJoy.exe with BUILD.cmd first.'}
    if(-not $NsisPath) {
        $candidates=@($env:NSIS_EXE)
        foreach($base in @(${env:ProgramFiles(x86)},$env:ProgramFiles)){if($base){$candidates+=Join-Path $base 'NSIS/makensis.exe'}}
        $command=Get-Command makensis.exe -ErrorAction SilentlyContinue
        if($command){$candidates+=$command.Source}
        if($env:LOCALAPPDATA){$candidates+=Join-Path $env:LOCALAPPDATA 'tauri/NSIS/makensis.exe'}
        $NsisPath=$candidates | Where-Object {$_ -and (Test-Path -LiteralPath $_ -PathType Leaf)} | Select-Object -First 1
    }
    if(-not $NsisPath){throw 'Optional installer packaging needs NSIS 3; pass -NsisPath or set NSIS_EXE. Portable builds do not require it.'}
    $NsisPath=(Get-Item -LiteralPath $NsisPath).FullName
    $version=& $NsisPath /VERSION
    if($LASTEXITCODE -ne 0 -or ($version -join '') -notmatch '^v?3\.'){throw 'A working NSIS 3 compiler is required for optional packaging.'}
    if($OutputDirectory -and (Test-Path -LiteralPath $OutputDirectory)){throw 'Output already exists; choose a new package directory.'}
    if($CheckOnly){Write-Output 'OPTIONAL_INSTALLER_CHECK=PASS no compilation, package creation or application launch';return}
    if(-not $OutputDirectory){$OutputDirectory=Join-Path $root ('build/packages/windows-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))}
    # One new package only: never republish/replace the portable application.
    & (Join-Path $PSScriptRoot 'package_windows_installer.ps1') -ExecutablePath $exe -NsisPath $NsisPath -OutputDirectory $OutputDirectory
    if(-not $? -or $LASTEXITCODE -ne 0){throw 'Optional installer packaging failed.'}
    Write-Output ('Optional installer package: '+$OutputDirectory)
} finally {$env:PATH=$originalPath}
