[CmdletBinding()]
param(
    [string]$ExePath,
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

$process = Start-Process -FilePath $ExePath -PassThru -WindowStyle Hidden
try {
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
if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
    throw "Production trace was not produced: $trace"
}

$traceText = Get-Content -LiteralPath $trace -Raw -Encoding UTF8
$required = @(
    '[component=app][event=startup.transaction.commit] origin=initial',
    '[component=realtime][event=stop.end]',
    '[component=vigem-output][event=stop.end]',
    '[component=backend][event=shutdown.end]',
    '[component=main][event=session.end] exit_code=0'
)
$missing = @($required | Where-Object { -not $traceText.Contains($_) })
if ($missing.Count -ne 0) {
    throw "Production trace is incomplete. Missing: $($missing -join ', ')"
}
if ($traceText -match '\[level=ERROR\]') {
    throw 'Production trace contains an ERROR event.'
}

$remaining = @(Get-Process -Name 'HallJoy' -ErrorAction SilentlyContinue)
if ($remaining.Count -ne 0) {
    throw 'A HallJoy process remained after production shutdown.'
}

Write-Host 'HallJoy production startup/shutdown smoke: PASS' -ForegroundColor Green
Write-Host "Trace: $trace"
