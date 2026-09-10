param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $repoRoot 'src\HallJoyProject\HallJoy\HallJoy.vcxproj'
$exe = Join-Path $repoRoot 'build\bin\AnalogSimulator\Release\x64\HallJoyV14Simulator.exe'

if (-not $SkipBuild) {
    $msbuildCandidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
    )
    $msbuild = $msbuildCandidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
    if (-not $msbuild) {
        throw 'MSBuild 2022 was not found.'
    }

    $buildOutput = @(& $msbuild $project /t:Rebuild /p:Configuration=Release `
        /p:Platform=x64 /p:HallJoyAnalogSimulator=true /m 2>&1)
    $buildExitCode = $LASTEXITCODE
    $buildOutput | Out-Host
    if ($buildExitCode -ne 0) {
        throw "HallJoy simulator build failed: $buildExitCode"
    }
    $warnings = @($buildOutput | Where-Object {
        [string]$_ -match ': warning (?:C|LNK)\d+:'
    })
    $unexpectedWarnings = @($warnings | Where-Object {
        [string]$_ -notmatch 'ViGEmClient\.lib\(ViGEmClient\.obj\)\s*: warning LNK4099:'
    })
    if ($unexpectedWarnings.Count -ne 0) {
        $unexpectedWarnings | ForEach-Object { Write-Host $_ -ForegroundColor Red }
        throw 'Unexpected simulator compiler/linker warnings were emitted.'
    }
}

if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Simulator executable was not produced: $exe"
}

$service = Get-Service -Name 'ViGEmBus' -ErrorAction SilentlyContinue
if (-not $service -or $service.Status -ne 'Running') {
    throw 'The exact real-child gate requires a running ViGEmBus service.'
}

function Get-VigemX360DeviceIds {
    return @(Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
        Where-Object {
            $_.InstanceId -match 'VID_045E&PID_028E' -or
            $_.FriendlyName -match 'Xbox 360 Controller for Windows'
        } |
        ForEach-Object { [string]$_.InstanceId } |
        Sort-Object -Unique)
}

$baselineDevices = @(Get-VigemX360DeviceIds)
$baselineDeviceKey = $baselineDevices -join "`n"
$beforeHostIds = @(Get-CimInstance Win32_Process `
    -Filter "Name='HallJoyV14Simulator.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like '*--halljoy-vigem-output-host*' } |
    ForEach-Object { [uint32]$_.ProcessId })

$process = Start-Process -FilePath $exe `
    -ArgumentList '--halljoy-test-vigem-output-real-child' `
    -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(30000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    $process.WaitForExit(5000) | Out-Null
    throw 'Exact real-child suite exceeded its 30 second hard bound.'
}
if ($process.ExitCode -ne 0) {
    throw "Exact real-child suite failed with exit code $($process.ExitCode)."
}

$pnpRestored = $false
$deadline = [DateTime]::UtcNow.AddSeconds(10)
do {
    $afterDevices = @(Get-VigemX360DeviceIds)
    if (($afterDevices -join "`n") -ceq $baselineDeviceKey) {
        $pnpRestored = $true
        break
    }
    Start-Sleep -Milliseconds 100
} while ([DateTime]::UtcNow -lt $deadline)
if (-not $pnpRestored) {
    throw 'ViGEm X360 PnP device set did not return to its pre-test baseline.'
}

$survivors = @(Get-CimInstance Win32_Process `
    -Filter "Name='HallJoyV14Simulator.exe'" -ErrorAction SilentlyContinue |
    Where-Object {
        $_.CommandLine -like '*--halljoy-vigem-output-host*' -and
        $beforeHostIds -notcontains [uint32]$_.ProcessId
    })
if ($survivors.Count -ne 0) {
    $survivors | Select-Object ProcessId,ParentProcessId,CommandLine |
        Format-List | Out-Host
    throw 'One or more real output children survived their owning suite.'
}

$artifact = Get-Item -LiteralPath $exe
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash
Write-Host (("VIGEM_OUTPUT_REAL_CHILD_EXACT_EXE=PASS generations=1 pads=4 " +
    "apply=1 neutral=4 remove=4 pnp_baseline={0} survivors=0 bytes={1} " +
    "sha256={2}") -f $baselineDevices.Count, $artifact.Length, $hash)
