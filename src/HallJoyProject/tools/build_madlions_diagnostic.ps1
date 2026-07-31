param(
    [ValidateSet("x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$packageRoot = Split-Path -Parent $repo
$project = Join-Path $repo "HallJoy\HallJoy.vcxproj"
$outDir = Join-Path $repo "x64\MadlionsDiagnostic"
$exe = Join-Path $outDir "HallJoyMadlionsSafeHID.exe"
$pdb = Join-Path $outDir "HallJoyMadlionsSafeHID.pdb"
$map = Join-Path $outDir "HallJoyMadlionsSafeHID.map"
$sendDir = Join-Path $outDir "SEND_TO_MADLIONS_TESTER"

$required = @(
    $project,
    (Join-Path $repo "HallJoy\app.cpp"),
    (Join-Path $repo "HallJoy\app.h"),
    (Join-Path $repo "HallJoy\backend.cpp"),
    (Join-Path $repo "HallJoy\embedded_analog_stack.cpp"),
    (Join-Path $repo "HallJoy\analog_host_client.cpp"),
    (Join-Path $repo "HallJoy\analog_host_shared.h"),
    (Join-Path $packageRoot "UniversalAnalogPluginFixed\halljoy_dense_snapshot.h"),
    (Join-Path $repo "third_party\ViGEmClient\include\ViGEm\Client.h"),
    (Join-Path $repo "third_party\ViGEmClient\lib\release\x64\ViGEmClient.lib"),
    (Join-Path $repo "third_party\WootingAnalogSDK091\include\wooting-analog-sdk.h"),
    (Join-Path $repo "runtime\universal_analog_abiv1.dll")
)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_) })
if ($missing.Count -ne 0) {
    Write-Host "The source tree is incomplete. Missing:" -ForegroundColor Red
    $missing | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    Write-Host "Extract the complete HallJoy Madlions V6 source archive into a new folder." -ForegroundColor Yellow
    exit 2
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = $null
if (Test-Path -LiteralPath $vswhere) {
    $msbuild = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "MSBuild\**\Bin\amd64\MSBuild.exe" | Select-Object -First 1
}
if (-not $msbuild) {
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
    )
    $msbuild = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $msbuild) {
    throw "MSBuild x64 was not found. Install Visual Studio 2022 with Desktop development with C++."
}

Write-Host "Using MSBuild: $msbuild"
& $msbuild $project `
    "/t:Rebuild" `
    "/p:Configuration=Release" `
    "/p:Platform=$Platform" `
    "/p:HallJoyDiagnostic=true" `
    "/p:HallJoyMadlionsDiagnostic=true" `
    "/m"
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Diagnostic executable was not produced: $exe"
}

if (Test-Path -LiteralPath $sendDir) {
    Remove-Item -LiteralPath $sendDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $sendDir | Out-Null
Copy-Item -LiteralPath $exe -Destination $sendDir -Force
if (Test-Path -LiteralPath $map) { Copy-Item -LiteralPath $map -Destination $sendDir -Force }
$symbolsDir = Join-Path $sendDir 'DEBUG_SYMBOLS_RETURN_ON_CRASH'
New-Item -ItemType Directory -Force -Path $symbolsDir | Out-Null
if (Test-Path -LiteralPath $pdb) { Copy-Item -LiteralPath $pdb -Destination $symbolsDir -Force }
Copy-Item -LiteralPath (Join-Path $repo 'runtime\universal_analog_abiv1.dll') -Destination $symbolsDir -Force
$latencyFiles = @(
    'LATENCY_TRACE_TEST_RU.md',
    'run_latency_trace.ps1',
    'collect_latency_trace.ps1',
    'V11_0_DENSE_DEVICE_SNAPSHOT_RU.md'
)
foreach ($name in $latencyFiles) {
    $source = Join-Path $packageRoot $name
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $sendDir $name) -Force
    }
}
@"
This diagnostic EXE contains the exact SafeHID V6 private ABI1 plugin.

V6 does NOT load the Wooting SDK and does NOT modify Program Files. The EXE
extracts HallJoyUniversalAnalogHost.dll next to itself and loads it only in a
crash-isolated child process. No UAC request is expected.

SAFEHID SELF-TEST
1. Run:
     .\HallJoyMadlionsSafeHID.exe --diagnostic-analog-host-crash-test
2. HallJoy must stay open.
3. One intentional HallJoyAnalogHostCrashLatest.txt/.dmp must be created.
4. The child host must restart and analog input must resume.
5. Delete the intentional crash files before the real stress test.

MADLIONS ROOT-CAUSE STRESS TEST
1. Run HallJoy normally and verify analog values.
2. Keep it running for at least 60-120 minutes.
3. During the test, repeatedly:
   - start and close Steam;
   - connect and disconnect Steam Controller;
   - connect and disconnect another USB/Bluetooth gamepad;
   - lock/unlock Windows and change foreground games;
   - press 1-6 Madlions keys together at different travel depths.
4. Do not run the intentional crash-test argument during this stage.

LATENCY TRACE (TEMPORARY V10.1)
1. Start with:
     .\run_latency_trace.ps1
   or:
     .\HallJoyMadlionsSafeHID.exe --latency-trace
2. Set Polling interval to 1 ms.
3. Move one bound analogue key continuously for 20-30 seconds.
4. Close HallJoy normally.
5. Run .\collect_latency_trace.ps1.
6. Return HallJoyDiagnostic.log and HallJoyLatencyTraceExtract.txt.
See LATENCY_TRACE_TEST_RU.md for field definitions.

PASS CRITERIA
- HallJoy never closes or freezes.
- Analog values remain responsive and no key remains stuck.
- No new real HallJoyAnalogHostCrash_*.txt/.dmp is created.
- A controlled transport reconnect is acceptable only if the log contains
  "persistent plugin transport error" followed by a fresh host start and
  analog recovery. It must not create a crash dump.

RETURN AFTER TEST
- HallJoyDiagnostic.log
- HallJoyDiagnosticVectored.txt
- HallJoyDiagnosticCrash.txt
- HallJoyDiagnosticExit.txt
- HallJoyAnalogHost.log
- every HallJoyAnalogHostCrash_*.txt/.dmp, if any
- HallJoyMadlionsSafeHID.map
- DEBUG_SYMBOLS_RETURN_ON_CRASH\ (entire folder)

The private plugin is embedded. The PDB, map and exact plugin DLL are copied
only so a remaining failure can be symbolicated.
"@ | Set-Content -LiteralPath (Join-Path $sendDir "README_TEST.txt") -Encoding UTF8

Write-Host ""
Write-Host "Built successfully." -ForegroundColor Green
Write-Host "Send this folder to the Madlions tester:" -ForegroundColor Green
Write-Host "  $sendDir"
Write-Host ""
Write-Host "Only HallJoyMadlionsSafeHID.exe is required at runtime; the private plugin is embedded."
if (Test-Path -LiteralPath $pdb) {
    Write-Host "PDB and the exact plugin DLL were copied into the tester debug-symbols folder."
}
