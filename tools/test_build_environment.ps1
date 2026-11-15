$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'build_environment.ps1')
$originalPath=$env:PATH
try {
 $python=Resolve-HallJoyPython
 if($python -like '*\Microsoft\WindowsApps\*'){throw 'Store alias accepted'}
 if((Resolve-HallJoyPython -PythonPath $python) -ne $python){throw 'Explicit interpreter changed'}
 $rejected=$false
 try {$null=Resolve-HallJoyPython -PythonPath (Join-Path $PSScriptRoot 'missing/python.exe')} catch {$rejected=$true}
 if(-not $rejected){throw 'Missing explicit interpreter accepted'}
 # Hide all Clang candidates for this isolated process. Normal discovery must
 # succeed; an explicitly requested full-runtime compiler must fail clearly.
 function Get-Command {param($Name,[switch]$All,$ErrorAction);if($Name -like 'clang*'){return};Microsoft.PowerShell.Core\Get-Command -Name $Name -All:$All -ErrorAction SilentlyContinue}
 function Test-Path {param($LiteralPath,$Path,$PathType);$p=if($LiteralPath){$LiteralPath}else{$Path};if($p -like '*clang*.exe'){return $false};if($PathType){Microsoft.PowerShell.Management\Test-Path -LiteralPath $p -PathType $PathType}else{Microsoft.PowerShell.Management\Test-Path -LiteralPath $p}}
 $environment=Initialize-HallJoyBuildEnvironment -PythonPath $python
 if($environment.PythonPath -ne $python -or $environment.ClangPath){throw 'Compiler-free discovery failed'}
 $rejected=$false
 try {$null=Initialize-HallJoyBuildEnvironment -PythonPath $python -RequireClang} catch {$rejected=$_.Exception.Message -like '*Clang*'}
 if(-not $rejected){throw 'Explicit unavailable Clang was not rejected'}
 Write-Output 'BUILD_ENVIRONMENT_TESTS=PASS explicit_python invalid_python store_alias compiler_absent_incremental explicit_compiler_failure'
} finally {$env:PATH=$originalPath}
