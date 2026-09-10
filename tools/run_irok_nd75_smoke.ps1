[CmdletBinding()]
param(
    [string]$ExePath = '',
    [ValidateRange(3, 30)]
    [int]$RunSeconds = 5
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $root `
        'build\packages\irok-nd75-test-v1.4\HallJoy-IROK-ND75-Test.exe'
}
$ExePath = [IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "IROK ND75 diagnostic executable was not found: $ExePath"
}

if (-not ('HallJoyIrokNd75SmokeWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class HallJoyIrokNd75SmokeWindow {
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    public static int PostClose(uint targetProcessId) {
        int sent = 0;
        EnumWindows((window, unused) => {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId == targetProcessId &&
                PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero))
                ++sent;
            return true;
        }, IntPtr.Zero);
        return sent;
    }
}
'@
}

$smokeRoot = Join-Path $env:TEMP `
    ('halljoy-nd75-smoke-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $smokeRoot | Out-Null
$smokeExe = Join-Path $smokeRoot 'HallJoy-IROK-ND75-Test.exe'
Copy-Item -LiteralPath $ExePath -Destination $smokeExe
New-Item -ItemType File -Path (Join-Path $smokeRoot 'HallJoy.portable') | Out-Null
$hashBefore = (Get-FileHash -LiteralPath $smokeExe -Algorithm SHA256).Hash

$process = Start-Process -FilePath $smokeExe -WorkingDirectory $smokeRoot `
    -WindowStyle Hidden -PassThru
try {
    Start-Sleep -Seconds $RunSeconds
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    $closeAccepted = $false
    do {
        $process.Refresh()
        if ($process.HasExited) { break }
        if ([HallJoyIrokNd75SmokeWindow]::PostClose(
                [uint32]$process.Id) -gt 0) {
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

    $process.Refresh()
    if ($process.ExitCode -ne 0) {
        throw "HallJoy exited with code $($process.ExitCode)."
    }
    $hashAfter = (Get-FileHash -LiteralPath $smokeExe -Algorithm SHA256).Hash
    if ($hashAfter -ne $hashBefore) {
        throw 'HallJoy changed its own executable during the smoke run.'
    }

    $trace = Join-Path $smokeRoot 'HallJoyStabilityTrace.log'
    if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
        throw 'Diagnostic smoke did not create HallJoyStabilityTrace.log.'
    }
    $traceText = Get-Content -LiteralPath $trace -Raw
    foreach ($marker in @(
        '[component=irok-nd75][event=start.ok]',
        'hotplug_ready=1',
        '[component=irok-nd75][event=worker.exit]',
        'fault_kind=0'
    )) {
        if (-not $traceText.Contains($marker)) {
            throw "Diagnostic smoke trace is missing marker: $marker"
        }
    }

    Start-Sleep -Milliseconds 500
    $remainingChildren = @(Get-CimInstance Win32_Process | Where-Object {
        $_.ParentProcessId -eq $process.Id
    })
    if ($remainingChildren.Count -ne 0) {
        throw 'A child process remained after HallJoy shutdown.'
    }

    Write-Host 'IROK ND75 no-device startup/shutdown smoke: PASS' `
        -ForegroundColor Green
    Write-Host "Exit code: $($process.ExitCode); WM_CLOSE accepted; hash stable." `
        -ForegroundColor Green
}
finally {
    $process.Refresh()
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
    $resolved = [IO.Path]::GetFullPath($smokeRoot)
    $temp = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
    if ($resolved.StartsWith($temp, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
    }
}
