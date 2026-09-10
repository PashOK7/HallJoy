[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent $PSScriptRoot
$projectRoot = Join-Path $repo 'src\HallJoyProject'
$project = Join-Path $projectRoot 'HallJoy\HallJoy.vcxproj'
$compiled = Join-Path $projectRoot '..\..\build\bin\IrokNd75Diagnostic\Release\x64\HallJoy-IROK-ND75-Test.exe'
$package = Join-Path $repo 'build\packages\irok-nd75-test-v1.4'
$zip = Join-Path $repo 'build\packages\HallJoy-v1.4-IROK-ND75-TEST.zip'
$expectedPackage = [IO.Path]::GetFullPath($package).TrimEnd('\')

foreach ($required in @(
    $project,
    (Join-Path $projectRoot '..\..\build\runtime\universal_analog_abiv0.dll'),
    (Join-Path $projectRoot '..\..\build\runtime\universal_analog_abiv1.dll'),
    (Join-Path $repo 'docs\v1.4\IROK_ND75_OWNER_TEST.md')
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required diagnostic input is missing: $required"
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path -LiteralPath $vswhere) {
    $msbuild = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
}
if (-not $msbuild) {
    $msbuild = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $msbuild) { throw 'MSBuild x64 was not found.' }

$buildOutput = @(& $msbuild $project /t:Rebuild /p:Configuration=Release `
    /p:Platform=x64 /p:HallJoyIrokNd75Diagnostic=true /m 2>&1)
$buildExitCode = $LASTEXITCODE
$buildOutput | Out-Host
if ($buildExitCode -ne 0) {
    throw "IROK ND75 diagnostic build failed: $buildExitCode"
}
$warnings = @($buildOutput | Where-Object {
    [string]$_ -match ': warning (?:C|LNK)\d+:'
})
$unexpected = @($warnings | Where-Object {
    [string]$_ -notmatch 'ViGEmClient\.lib\(ViGEmClient\.obj\)\s*: warning LNK4099:'
})
if ($unexpected.Count -ne 0) {
    $unexpected | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    throw 'Unexpected diagnostic compiler/linker warnings were emitted.'
}
if (-not (Test-Path -LiteralPath $compiled -PathType Leaf)) {
    throw "Diagnostic executable was not produced: $compiled"
}

$bytes = [IO.File]::ReadAllBytes($compiled)
$ascii = [Text.Encoding]::ASCII.GetString($bytes)
$wide = [Text.Encoding]::Unicode.GetString($bytes)
foreach ($marker in @(
    'irok-nd75-m484-experimental',
    'M484',
    'X86HERGB'
)) {
    if (-not $ascii.Contains($marker)) {
        throw "Diagnostic executable is missing ASCII marker: $marker"
    }
}
foreach ($marker in @(
    '[irok.nd75.raw]',
    '[irok.nd75.telemetry]',
    '[irok.nd75.diagnostic]',
    '[irok.nd75.session_summary]',
    '[irok.nd75.coverage]',
    'protocol.cancelled',
    'hotplug_ready=1'
)) {
    if (-not $wide.Contains($marker)) {
        throw "Diagnostic executable is missing telemetry marker: $marker"
    }
}
$bytes = $null
$ascii = $null
$wide = $null

if (Test-Path -LiteralPath $package) {
    $resolved = [IO.Path]::GetFullPath($package).TrimEnd('\')
    if (-not $resolved.Equals($expectedPackage,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe diagnostic package target: $resolved"
    }
    Get-ChildItem -LiteralPath $resolved -Force | Remove-Item -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $package -Force | Out-Null
}

Copy-Item -LiteralPath $compiled `
    -Destination (Join-Path $package 'HallJoy-IROK-ND75-Test.exe') -Force
Copy-Item -LiteralPath (Join-Path $repo 'docs\v1.4\IROK_ND75_OWNER_TEST.md') `
    -Destination (Join-Path $package 'README-IROK-ND75-TEST.md') -Force

$hash = Get-FileHash -LiteralPath `
    (Join-Path $package 'HallJoy-IROK-ND75-Test.exe') -Algorithm SHA256
@(
    "$($hash.Hash) *HallJoy-IROK-ND75-Test.exe"
) | Set-Content -LiteralPath (Join-Path $package 'SHA256SUMS.txt') `
    -Encoding ascii

if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
$packageFiles = @(Get-ChildItem -LiteralPath $package -File | Sort-Object Name)
$expectedNames = @(
    'HallJoy-IROK-ND75-Test.exe',
    'README-IROK-ND75-TEST.md',
    'SHA256SUMS.txt'
)
if (($packageFiles.Name -join "`n") -ne (($expectedNames | Sort-Object) -join "`n")) {
    throw "Unexpected ND75 package contents: $($packageFiles.Name -join ', ')"
}
Compress-Archive -LiteralPath $packageFiles.FullName -DestinationPath $zip `
    -CompressionLevel Optimal
if (-not (Test-Path -LiteralPath $zip -PathType Leaf)) {
    throw "ND75 test archive was not created: $zip"
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $archiveNames = @($archive.Entries | ForEach-Object { $_.FullName } |
        Sort-Object)
    if (($archiveNames -join "`n") -ne (($expectedNames | Sort-Object) -join "`n")) {
        throw "Unexpected ND75 archive contents: $($archiveNames -join ', ')"
    }
} finally {
    $archive.Dispose()
}

Write-Host "IROK ND75 test package: $package" -ForegroundColor Green
Write-Host "IROK ND75 test archive: $zip" -ForegroundColor Green
Write-Host "HallJoy-IROK-ND75-Test.exe SHA256: $($hash.Hash)" `
    -ForegroundColor Green
Write-Host "Archive SHA256: $((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash)" `
    -ForegroundColor Green
