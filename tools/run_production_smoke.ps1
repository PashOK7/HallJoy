[CmdletBinding()]
param(
    [string]$ExePath,
    [switch]$StartOverlay,
    [ValidateRange(1, 65535)]
    [int]$OverlayPort = 18765,
    [ValidateRange(1, 100000)]
    [int]$OverlayFuzzIterations = 2000,
    [ValidateRange(3, 60)]
    [int]$RunSeconds = 10
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $root 'build\output\HallJoy.exe'
}
$ExePath = [IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "Production executable was not found: $ExePath"
}

$output = Split-Path -Parent $ExePath
$trace = Join-Path $output 'HallJoyStabilityTrace.log'
$forbiddenProductionLogs = @(
    $trace,
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

$processCleanupDeadline = [DateTime]::UtcNow.AddSeconds(2)
do {
    $remaining = @(Get-Process -Name 'HallJoy' -ErrorAction SilentlyContinue)
    if ($remaining.Count -eq 0) { break }
    Start-Sleep -Milliseconds 100
} while ([DateTime]::UtcNow -lt $processCleanupDeadline)
if ($remaining.Count -ne 0) {
    throw 'A HallJoy process remained after production shutdown.'
}

if ($StartOverlay) {
    Write-Host 'HallJoy production overlay framing/startup/shutdown smoke: PASS' -ForegroundColor Green
} else {
    Write-Host 'HallJoy production startup/shutdown smoke: PASS' -ForegroundColor Green
}
Write-Host 'Continuous diagnostic logs: none; crash report: none' -ForegroundColor Green
