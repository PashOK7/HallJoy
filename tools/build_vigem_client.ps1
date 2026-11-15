[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$spec = (Get-Content -LiteralPath (Join-Path $root 'tools/dependency-lock.json') -Raw | ConvertFrom-Json).binaryInputs.vigemClient
$source = Join-Path $root '.cache/ViGEmClient'
if (-not (Test-Path -LiteralPath $source)) {
    & git clone ([string]$spec.sourceRepository) $source
    if ($LASTEXITCODE -ne 0) { throw 'ViGEmClient source acquisition failed.' }
    & git -C $source checkout --detach ([string]$spec.sourceCommit)
    if ($LASTEXITCODE -ne 0) { throw 'Pinned ViGEmClient commit not found.' }
}
$commit = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -cne [string]$spec.sourceCommit) { throw 'ViGEmClient source revision differs from the lock; preserve and review the cache.' }
$changes = @(& git -C $source status --porcelain --untracked-files=all)
if ($LASTEXITCODE -ne 0 -or $changes.Count -ne 0) { throw 'ViGEmClient source is not clean.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$builder = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild/**/Bin/amd64/MSBuild.exe' | Select-Object -First 1
if (-not $builder) { throw 'Visual Studio MSBuild is required.' }
$output = Join-Path $root 'build/obj/ViGEmClientVerified'
$debugProps = Join-Path $PSScriptRoot 'vigem_client_embedded_debug.props'
# Import after the upstream item definitions without changing the pinned source.
# /Z7 embeds debug information in the object, avoiding a missing compiler PDB.
& $builder (Join-Path $source 'src/ViGEmClient.vcxproj') /t:Rebuild /p:Configuration=Release_LIB /p:Platform=x64 /p:PlatformToolset=v143 "/p:VCToolsVersion=$($spec.vcToolsVersion)" "/p:ForceImportBeforeCppTargets=$debugProps" "/p:OutDir=$output\" "/p:IntDir=$output\obj\" /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Pinned ViGEmClient source build failed.' }
# Never replace the locked distributable implicitly. Debug metadata/tool paths may
# change its binary hash; explicit review updates provenance and the lock together.
Get-FileHash -LiteralPath (Join-Path $output 'ViGEmClient.lib') -Algorithm SHA256
Write-Output 'Source-built candidate ready. The installed library and dependency lock were not changed.'
