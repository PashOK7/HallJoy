param(
    [switch]$SkipBuild,
    [switch]$RuntimeStress
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

$beforeHostIds = @(Get-CimInstance Win32_Process `
    -Filter "Name='HallJoyV14Simulator.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like '*--halljoy-vigem-output-host*' } |
    ForEach-Object { [uint32]$_.ProcessId })

$testArgument = if ($RuntimeStress) {
    '--halljoy-test-vigem-output-runtime-stress'
} else {
    '--halljoy-test-vigem-output-self-host'
}
$process = Start-Process -FilePath $exe `
    -ArgumentList $testArgument `
    -WindowStyle Hidden -PassThru
if ($RuntimeStress) {
    $completed = $process.WaitForExit(180000)
} else {
    $completed = $process.WaitForExit(30000)
}
if (-not $completed) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    $process.WaitForExit(5000) | Out-Null
    throw 'Exact-EXE output suite exceeded its hard time bound.'
}
if ($process.ExitCode -ne 0) {
    throw "Exact-EXE output suite failed with exit code $($process.ExitCode)."
}

Start-Sleep -Milliseconds 200
$survivors = @(Get-CimInstance Win32_Process `
    -Filter "Name='HallJoyV14Simulator.exe'" -ErrorAction SilentlyContinue |
    Where-Object {
        $_.CommandLine -like '*--halljoy-vigem-output-host*' -and
        $beforeHostIds -notcontains [uint32]$_.ProcessId
    })
if ($survivors.Count -ne 0) {
    $survivors | Select-Object ProcessId,ParentProcessId,CommandLine | Format-List | Out-Host
    throw 'One or more exact-test output children survived their owning suite.'
}

$artifact = Get-Item -LiteralPath $exe
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash
if ($RuntimeStress) {
    Write-Host ("VIGEM_OUTPUT_RUNTIME_STRESS_EXACT_EXE=PASS " +
        "publications_min=100000 generations=101 topology_changes=50 " +
        "disable_enable=10 survivors=0 bytes={0} sha256={1}" -f `
        $artifact.Length, $hash)
} else {
    Write-Host ("VIGEM_OUTPUT_SELF_HOST_EXACT_EXE=PASS generations=9 " +
        "o3_reap_restart=1 o4_boundaries=6 clean_stop_qualified=1 " +
        "survivors=0 bytes={0} sha256={1}" -f $artifact.Length, $hash)
}
