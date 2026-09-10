[CmdletBinding()]
param(
    [string]$ExePath,
    [switch]$StartOverlay,
    [ValidateRange(1, 65535)]
    [int]$OverlayPort = 18765,
    [ValidateRange(1, 100000)]
    [int]$OverlayFuzzIterations = 2000,
    [ValidateRange(3, 60)]
    [int]$RunSeconds = 10,
    [string]$EvidenceRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-HallJoyStateSnapshot {
    param([string]$StateRoot)

    $snapshot = @{}
    if (-not (Test-Path -LiteralPath $StateRoot -PathType Container)) {
        return $snapshot
    }
    foreach ($file in Get-ChildItem -LiteralPath $StateRoot -File -Recurse | Sort-Object FullName) {
        $relative = $file.FullName.Substring($StateRoot.Length).TrimStart('\')
        $snapshot[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
    return $snapshot
}

function Write-StateSnapshot {
    param([hashtable]$Snapshot, [string]$Path)

    $entries = @($Snapshot.Keys | Sort-Object | ForEach-Object {
        [pscustomobject]@{ file = $_; sha256 = $Snapshot[$_] }
    })
    $entries | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Assert-SameStateSnapshot {
    param([hashtable]$Before, [hashtable]$After)

    $beforeKeys = @($Before.Keys | Sort-Object)
    $afterKeys = @($After.Keys | Sort-Object)
    if (($beforeKeys -join "`n") -ne ($afterKeys -join "`n")) {
        throw 'HallJoy user-state file set changed during production smoke.'
    }
    foreach ($key in $beforeKeys) {
        if ($Before[$key] -ne $After[$key]) {
            throw "HallJoy user-state file changed during production smoke: $key"
        }
    }
}

function Copy-ProfileStateToPortableRuntime {
    param([string]$SourceRoot, [string]$DestinationRoot)

    if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) { return }
    foreach ($item in @(Get-ChildItem -LiteralPath $SourceRoot -Force)) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to copy reparse-point user-state item: $($item.FullName)"
        }
        Copy-Item -LiteralPath $item.FullName -Destination $DestinationRoot -Recurse -Force
    }
}

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $root 'build\release\HallJoy.exe'
}
$ExePath = [IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "Production executable was not found: $ExePath"
}
$sourceExePath = $ExePath
$sourceExeHash = (Get-FileHash -LiteralPath $sourceExePath -Algorithm SHA256).Hash

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $root "build\evidence\production-smoke\$stamp"
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
$portableRuntimeRoot = Join-Path $EvidenceRoot 'portable-runtime'
$resolvedEvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot).TrimEnd('\') + '\'
$resolvedPortableRuntimeRoot = [IO.Path]::GetFullPath($portableRuntimeRoot)
if (-not $resolvedPortableRuntimeRoot.StartsWith($resolvedEvidenceRoot, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $portableRuntimeRoot)) {
    throw "Refusing to create an unexpected portable smoke runtime: $resolvedPortableRuntimeRoot"
}
$stateRoot = Join-Path $env:LOCALAPPDATA 'HallJoy'
$stateBefore = Get-HallJoyStateSnapshot -StateRoot $stateRoot
Write-StateSnapshot -Snapshot $stateBefore -Path (Join-Path $EvidenceRoot 'live-state-before.json')
New-Item -ItemType Directory -Path $portableRuntimeRoot | Out-Null
Copy-ProfileStateToPortableRuntime -SourceRoot $stateRoot -DestinationRoot $portableRuntimeRoot
$profileExePath = Join-Path $portableRuntimeRoot 'HallJoy.exe'
Copy-Item -LiteralPath $sourceExePath -Destination $profileExePath -Force
if ((Get-FileHash -LiteralPath $profileExePath -Algorithm SHA256).Hash -ne $sourceExeHash) {
    throw 'The isolated smoke executable does not match the requested production artifact.'
}
New-Item -ItemType File -Path (Join-Path $portableRuntimeRoot 'HallJoy.portable') | Out-Null
$ExePath = $profileExePath
try {
    $activeHallJoy = @(
        Get-CimInstance Win32_Process -Filter "Name='HallJoy.exe'" -ErrorAction Stop
    )
}
catch {
    throw 'Cannot prove an isolated physical-UAP production smoke because HallJoy process enumeration failed.'
}
if ($activeHallJoy.Count -ne 0) {
    $owners = ($activeHallJoy | ForEach-Object {
        $path = if ($_.ExecutablePath) { [string]$_.ExecutablePath } else { '<unknown>' }
        "PID=$($_.ProcessId) path=$path"
    }) -join '; '
    throw "Physical-UAP production smoke requires every HallJoy instance to be closed: $owners"
}
$preexistingArtifactPids = @()

$output = $portableRuntimeRoot
$trace = Join-Path $output 'HallJoyStabilityTrace.log'
$forbiddenProductionLogs = @(
    (Join-Path $output 'HallJoyDiagnostic.log'),
    (Join-Path $output 'HallJoyAddressedAnalogTrace.log'),
    (Join-Path $output 'HallJoyCrash.txt')
)
if (Test-Path -LiteralPath $trace) {
    Remove-Item -LiteralPath $trace -Force
}

if (-not ('HallJoyProductionSmokeWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class HallJoyProductionSmokeWindow {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);

    public static int PostClose(uint targetProcessId) {
        int sent = 0;
        EnumWindows((window, unused) => {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId == targetProcessId && PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero))
                ++sent;
            return true;
        }, IntPtr.Zero);
        return sent;
    }
}
'@
}

$processArguments = @()
if ($StartOverlay) {
    $processArguments += @('--overlay-server', '--port', [string]$OverlayPort)
}
$startParameters = @{
    FilePath = $ExePath
    PassThru = $true
    WindowStyle = 'Hidden'
    WorkingDirectory = $output
}
if ($processArguments.Count -ne 0) {
    $startParameters.ArgumentList = $processArguments
}
$process = Start-Process @startParameters
try {
    if ($StartOverlay) {
        & python (Join-Path $root 'tools\check_overlay_responsiveness.py') `
            --port $OverlayPort --deadline-ms 1000 --connect-deadline-ms 5000
        if ($LASTEXITCODE -ne 0) {
            throw "Production overlay responsiveness gate failed with exit code $LASTEXITCODE."
        }
        & python (Join-Path $root 'tools\check_overlay_http_framing.py') `
            --port $OverlayPort --connect-deadline-ms 5000
        if ($LASTEXITCODE -ne 0) {
            throw "Production overlay framing gate failed with exit code $LASTEXITCODE."
        }
        & python (Join-Path $root 'tools\check_overlay_concurrency_origin.py') `
            --port $OverlayPort --connect-deadline-ms 5000 --deadline-ms 1000
        if ($LASTEXITCODE -ne 0) {
            throw "Production overlay concurrency/origin gate failed with exit code $LASTEXITCODE."
        }
        & python (Join-Path $root 'tools\fuzz_overlay_http.py') `
            --port $OverlayPort --connect-deadline-ms 5000 `
            --iterations $OverlayFuzzIterations --workers 8
        if ($LASTEXITCODE -ne 0) {
            throw "Production overlay fuzz gate failed with exit code $LASTEXITCODE."
        }
    }
    Start-Sleep -Seconds $RunSeconds
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    $closeAccepted = $false
    do {
        $process.Refresh()
        if ($process.HasExited) { break }
        if ([HallJoyProductionSmokeWindow]::PostClose([uint32]$process.Id) -gt 0) {
            $closeAccepted = $true
        }
        if ($process.WaitForExit(250)) { break }
    } while ([DateTime]::UtcNow -lt $deadline)

    if (-not $closeAccepted) {
        throw 'HallJoy did not expose a window that accepted graceful close.'
    }
    if (-not $process.HasExited) {
        throw 'HallJoy did not exit within 15 seconds after graceful close.'
    }
}
finally {
    $process.Refresh()
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
}

$process.Refresh()
if ($process.ExitCode -ne 0) {
    throw "HallJoy exited with code $($process.ExitCode)."
}
$unexpectedLogs = @($forbiddenProductionLogs | Where-Object {
    Test-Path -LiteralPath $_ -PathType Leaf
})
if ($unexpectedLogs.Count -ne 0) {
    throw "Production created a continuous diagnostic or crash log: $($unexpectedLogs -join ', ')"
}
if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
    throw 'Production stability trace was not produced.'
}
if ((Get-Content -LiteralPath $trace -Raw) -match '\[level=ERROR\]') {
    throw 'Production stability trace contains ERROR.'
}

$processCleanupDeadline = [DateTime]::UtcNow.AddSeconds(2)
do {
    # The preflight requires physical-UAP isolation.  Here we only identify a
    # process survivor from this exact artifact; unrelated HallJoy processes
    # cannot have been present when the test began.
    $remaining = @(
        Get-CimInstance Win32_Process -Filter "Name='HallJoy.exe'" -ErrorAction SilentlyContinue |
            Where-Object {
                $_.ExecutablePath -and
                [IO.Path]::GetFullPath([string]$_.ExecutablePath) -eq $ExePath -and
                $preexistingArtifactPids -notcontains [uint32]$_.ProcessId
            }
    )
    if ($remaining.Count -eq 0) { break }
    Start-Sleep -Milliseconds 100
} while ([DateTime]::UtcNow -lt $processCleanupDeadline)
if ($remaining.Count -ne 0) {
    throw 'A process from the tested HallJoy artifact remained after production shutdown.'
}

if ($StartOverlay) {
    Write-Host 'HallJoy production overlay framing/startup/shutdown smoke: PASS' -ForegroundColor Green
} else {
    Write-Host 'HallJoy production startup/shutdown smoke: PASS' -ForegroundColor Green
}
Write-Host 'Continuous diagnostic logs: none; crash report: none' -ForegroundColor Green
$stateAfter = Get-HallJoyStateSnapshot -StateRoot $stateRoot
Write-StateSnapshot -Snapshot $stateAfter -Path (Join-Path $EvidenceRoot 'live-state-after.json')
Assert-SameStateSnapshot -Before $stateBefore -After $stateAfter
[ordered]@{
    schema = 1
    requested_executable = $sourceExePath
    requested_executable_sha256 = $sourceExeHash
    tested_executable = $profileExePath
    storage_mode = 'portable-isolated-copy'
    overlay = [bool]$StartOverlay
    overlay_fuzz_iterations = $OverlayFuzzIterations
    run_seconds = $RunSeconds
    live_user_state_unchanged = $true
} | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'summary.json') -Encoding UTF8
Write-Host "Evidence: $EvidenceRoot" -ForegroundColor Green
