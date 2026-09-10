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
    $ExePath = Join-Path $root 'build\packages\gravastar-v75-diagnostic\HallJoy.exe'
}
$ExePath = [IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "GravaStar V75 diagnostic executable was not found: $ExePath"
}

$existing = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -ieq 'HallJoy.exe'
})
if ($existing.Count -ne 0) {
    throw 'Refusing to overlap an existing HallJoy process.'
}

if (-not ('HallJoyGravaStarSmokeWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class HallJoyGravaStarSmokeWindow {
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
                PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero)) ++sent;
            return true;
        }, IntPtr.Zero);
        return sent;
    }
}
'@
}

$smokeRoot = Join-Path $env:TEMP `
    ('halljoy-gravastar-v75-smoke-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $smokeRoot | Out-Null
$smokeExe = Join-Path $smokeRoot 'HallJoy.exe'
Copy-Item -LiteralPath $ExePath -Destination $smokeExe
New-Item -ItemType File -Path (Join-Path $smokeRoot 'HallJoy.portable') | Out-Null
$hashBefore = (Get-FileHash -LiteralPath $smokeExe -Algorithm SHA256).Hash

$process = Start-Process -FilePath $smokeExe -WorkingDirectory $smokeRoot `
    -WindowStyle Hidden -PassThru
try {
    Start-Sleep -Seconds $RunSeconds
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    $closeAccepted = $false
    do {
        $process.Refresh()
        if ($process.HasExited) { break }
        if ([HallJoyGravaStarSmokeWindow]::PostClose(
                [uint32]$process.Id) -gt 0) {
            $closeAccepted = $true
        }
        if ($process.WaitForExit(250)) { break }
    } while ([DateTime]::UtcNow -lt $deadline)

    if (-not $closeAccepted) {
        throw 'HallJoy did not expose a window that accepted graceful close.'
    }
    if (-not $process.HasExited) {
        throw 'HallJoy did not exit within 20 seconds after graceful close.'
    }
    $process.Refresh()
    if ($process.ExitCode -ne 0) {
        throw "HallJoy exited with code $($process.ExitCode)."
    }

    $hashAfter = (Get-FileHash -LiteralPath $smokeExe -Algorithm SHA256).Hash
    if ($hashAfter -ne $hashBefore) {
        throw 'HallJoy changed its own executable during the smoke run.'
    }

    $trace = Join-Path $smokeRoot 'HallJoy.log'
    if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
        throw 'Diagnostic smoke did not create HallJoy.log.'
    }
    $traceBytes = [IO.File]::ReadAllBytes($trace)
    if ([Array]::IndexOf($traceBytes, [byte]0) -ge 0) {
        throw 'Diagnostic log contains a preallocated NUL tail.'
    }
    $traceText = [Text.Encoding]::UTF8.GetString($traceBytes)
    foreach ($marker in @(
        '[component=main][event=session.start]',
        '[component=aula-win60he][event=diagnostic.enabled]',
        '[component=aula-win60he][event=worker.start]',
        '[component=aula-win60he][event=worker.exit]',
        '[component=aula-win60he][event=diagnostic.verdict]',
        '[component=app][event=shutdown.complete]',
        '[component=main][event=final_shutdown.end]',
        '[component=main][event=session.end]'
    )) {
        if (-not $traceText.Contains($marker)) {
            throw "Diagnostic log is missing lifecycle marker: $marker"
        }
    }
    $verdictLine = @($traceText -split "`r?`n" | Where-Object {
        $_.Contains('[component=aula-win60he][event=diagnostic.verdict]')
    }) | Select-Object -Last 1
    foreach ($verdictToken in @('conclusive=1', 'result=', 'action=',
        'progress=', 'failure=', 'discovery_attempts=', 'matrices=')) {
        if ([string]::IsNullOrWhiteSpace($verdictLine) -or
            -not $verdictLine.Contains($verdictToken)) {
            throw "Diagnostic verdict is incomplete: $verdictToken"
        }
    }
    foreach ($privateToken in @(
        $env:USERPROFILE,
        $smokeRoot,
        '\\?\',
        '\\.\',
        '[mad68pr.rawinput]',
        '[backend.spark] candidate'
    )) {
        if (-not [string]::IsNullOrWhiteSpace($privateToken) -and
            $traceText.IndexOf($privateToken,
                [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            throw "Diagnostic log leaked a forbidden path/inventory token: $privateToken"
        }
    }

    Start-Sleep -Milliseconds 500
    $survivors = @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -ieq 'HallJoy.exe'
    })
    if ($survivors.Count -ne 0) {
        throw 'A HallJoy parent or child process remained after shutdown.'
    }

    Write-Host 'GravaStar V75 diagnostic startup/privacy/shutdown smoke: PASS' `
        -ForegroundColor Green
    Write-Host "Exit code 0; WM_CLOSE accepted; log bytes=$($traceBytes.Length); SHA256=$hashBefore" `
        -ForegroundColor Green
}
finally {
    $process.Refresh()
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
    $resolved = [IO.Path]::GetFullPath($smokeRoot)
    $tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
    if ($resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force `
            -ErrorAction SilentlyContinue
    }
}
