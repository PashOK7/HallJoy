[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'build_environment.ps1')
$originalPath = $env:PATH
$checks = 0
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAIL: $Message" }
    $script:checks++
    Write-Output "PASS: $Message"
}
try {
    $python = Resolve-HallJoyPython
    Assert ($python -notlike '*\Microsoft\WindowsApps\*') 'Auto-discovery skips the Store alias'
    Assert ((Resolve-HallJoyPython -PythonPath $python) -eq $python) 'Explicit existing interpreter is respected'
    foreach ($invalid in @((Join-Path $PSScriptRoot 'missing/python.exe'), (Join-Path $env:LOCALAPPDATA 'Microsoft/WindowsApps/python.exe'))) {
        $rejected = $false
        try { $null = Resolve-HallJoyPython -PythonPath $invalid } catch { $rejected = $true }
        Assert $rejected 'Invalid explicit interpreter is rejected without fallback'
    }
    $environment = Initialize-HallJoyBuildEnvironment -PythonPath $python -RequireClang
    & python -I -c 'import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)'
    Assert ($LASTEXITCODE -eq 0) 'Child Python resolves to a working interpreter'
    & clang++ --version | Out-Null
    Assert ($LASTEXITCODE -eq 0) 'Child compiler is available on process PATH'
    $root = Split-Path -Parent $PSScriptRoot
    $fixture = Join-Path $root ('build/tests/build environment ' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path (Join-Path $fixture 'tools') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root 'BUILD.cmd') -Destination $fixture
    # Test the real CMD wrapper with an isolated build stage, including spaces and failure status.
    @'
param([string]$PythonPath, [switch]$CheckOnly)
. (Join-Path $PSScriptRoot '../../../../tools/build_environment.ps1')
$hash = Get-FileHash -LiteralPath (Join-Path $PSScriptRoot '../BUILD.cmd')
if ($hash.Hash.Length -ne 64) { exit 38 }
$signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $PSHOME 'powershell.exe')
if ($signature.Status -ne 'Valid') { exit 39 }
if ($CheckOnly -and $PythonPath -eq 'path with spaces') { exit 0 }
exit 37
'@ | Set-Content -LiteralPath (Join-Path $fixture 'tools/build_windows_installer.ps1') -Encoding ASCII
    & (Join-Path $fixture 'BUILD.cmd') -PythonPath 'path with spaces' -CheckOnly
    Assert ($LASTEXITCODE -eq 0) 'BUILD.cmd forwards spaced arguments and exits after successful CheckOnly'
    & (Join-Path $fixture 'BUILD.cmd') -PythonPath 'wrong value'
    Assert ($LASTEXITCODE -eq 37) 'BUILD.cmd preserves a failing build-stage exit code'
    Write-Output "BUILD_ENVIRONMENT_TESTS=PASS assertions=$checks"
} finally { $env:PATH = $originalPath }
