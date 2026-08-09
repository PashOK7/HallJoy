[CmdletBinding()]
param(
    [string]$ExePath,
    [ValidateRange(10, 2000)]
    [int]$WheelEventsPerPage = 240,
    [ValidateRange(0, 20)]
    [int]$EventDelayMs = 2,
    [string]$EvidenceRoot
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
if ([IO.Path]::GetFileName($ExePath) -ne 'HallJoy.exe') {
    throw "UI scroll stress requires the final executable name HallJoy.exe: $ExePath"
}
if (@(Get-Process -Name HallJoy -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'Refusing to start while HallJoy is already running.'
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $root "build\evidence\ui-scroll-stress\$stamp"
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null

if (-not ('HallJoyUiScrollStress' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public static class HallJoyUiScrollStress {
    public delegate bool EnumProc(IntPtr window, IntPtr parameter);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct NMHDR { public IntPtr hwndFrom; public UIntPtr idFrom; public int code; }

    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc callback, IntPtr parameter);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumProc callback, IntPtr parameter);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] static extern IntPtr GetParent(IntPtr window);
    [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr window);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder value, int length);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr window, out RECT rect);
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", SetLastError = true)] static extern IntPtr SendMessageTimeout(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool UpdateWindow(IntPtr window);
    [DllImport("user32.dll")] static extern uint GetGuiResources(IntPtr process, uint flags);
    [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr window);

    const uint WM_NULL = 0x0000;
    const uint WM_CLOSE = 0x0010;
    const uint WM_NOTIFY = 0x004E;
    const uint WM_MOUSEWHEEL = 0x020A;
    const uint WM_LBUTTONDOWN = 0x0201;
    const uint WM_LBUTTONUP = 0x0202;
    const uint WM_KEYDOWN = 0x0100;
    const uint MK_LBUTTON = 0x0001;
    const uint VK_ESCAPE = 0x001B;
    const uint TCM_FIRST = 0x1300;
    const uint PC_MSG_QUERY_SCROLL_STATE = 0x8000 + 390;
    const uint TCM_GETITEMCOUNT = TCM_FIRST + 4;
    const uint TCM_SETCURSEL = TCM_FIRST + 12;
    const int TCN_SELCHANGE = -551;
    const uint SMTO_ABORTIFHUNG = 0x0002;
    const int SW_HIDE = 0;
    const int SW_SHOW = 5;

    static string ClassName(IntPtr window) {
        var text = new StringBuilder(128);
        GetClassName(window, text, text.Capacity);
        return text.ToString();
    }

    public static IntPtr FindMain(uint processId) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, unused) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner == processId && IsWindowVisible(window) && ClassName(window) == "WootingVigemGui") {
                found = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static IntPtr FindChildByClass(IntPtr parent, string className, bool visibleOnly) {
        IntPtr found = IntPtr.Zero;
        EnumChildWindows(parent, (window, unused) => {
            if ((!visibleOnly || IsWindowVisible(window)) && ClassName(window) == className) {
                found = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static IntPtr ActivateSurface(IntPtr parent, string targetClass, string[] surfaceClasses) {
        var classes = new HashSet<string>(surfaceClasses, StringComparer.Ordinal);
        IntPtr target = IntPtr.Zero;
        EnumChildWindows(parent, (window, unused) => {
            string name = ClassName(window);
            if (classes.Contains(name)) {
                bool selected = name == targetClass;
                ShowWindow(window, selected ? SW_SHOW : SW_HIDE);
                if (selected) target = window;
            }
            return true;
        }, IntPtr.Zero);
        return target;
    }

    public static int TabCount(IntPtr tab) {
        return SendMessage(tab, TCM_GETITEMCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }

    public static bool SelectTab(IntPtr tab, int index) {
        IntPtr parent = GetParent(tab);
        if (parent == IntPtr.Zero) return false;
        SendMessage(tab, TCM_SETCURSEL, new IntPtr(index), IntPtr.Zero);
        var header = new NMHDR {
            hwndFrom = tab,
            idFrom = new UIntPtr(unchecked((uint)GetDlgCtrlID(tab))),
            code = TCN_SELCHANGE
        };
        IntPtr memory = Marshal.AllocHGlobal(Marshal.SizeOf(header));
        try {
            Marshal.StructureToPtr(header, memory, false);
            SendMessage(parent, WM_NOTIFY, new IntPtr(GetDlgCtrlID(tab)), memory);
            return true;
        } finally {
            Marshal.FreeHGlobal(memory);
        }
    }

    public static bool Responsive(IntPtr window, uint timeoutMs) {
        IntPtr result;
        return SendMessageTimeout(window, WM_NULL, IntPtr.Zero, IntPtr.Zero,
            SMTO_ABORTIFHUNG, timeoutMs, out result) != IntPtr.Zero;
    }

    public static int Scale(IntPtr window, int pixels) {
        uint dpi = GetDpiForWindow(window);
        if (dpi == 0) dpi = 96;
        return (int)Math.Round(pixels * dpi / 96.0);
    }

    public static bool Click(IntPtr window, int x, int y, uint timeoutMs) {
        IntPtr point = new IntPtr((y << 16) | (x & 0xffff));
        IntPtr ignored;
        bool down = SendMessageTimeout(window, WM_LBUTTONDOWN, new IntPtr(MK_LBUTTON),
            point, SMTO_ABORTIFHUNG, timeoutMs, out ignored) != IntPtr.Zero;
        bool up = SendMessageTimeout(window, WM_LBUTTONUP, IntPtr.Zero,
            point, SMTO_ABORTIFHUNG, timeoutMs, out ignored) != IntPtr.Zero;
        return down && up;
    }

    public static int CountVisibleChildrenByClass(IntPtr parent, string className) {
        int count = 0;
        EnumChildWindows(parent, (window, unused) => {
            if (IsWindowVisible(window) && ClassName(window) == className) ++count;
            return true;
        }, IntPtr.Zero);
        return count;
    }

    public static int CountVisibleTopWindowsByClass(uint processId, string className) {
        int count = 0;
        EnumWindows((window, unused) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner == processId && IsWindowVisible(window) && ClassName(window) == className) ++count;
            return true;
        }, IntPtr.Zero);
        return count;
    }

    public static IntPtr FindVisibleTopWindowByClass(uint processId, string className) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, unused) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner == processId && IsWindowVisible(window) && ClassName(window) == className) {
                found = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static bool CloseVisibleCombo(IntPtr parent, uint timeoutMs) {
        IntPtr combo = FindChildByClass(parent, "PremiumCombo_Control", true);
        if (combo == IntPtr.Zero) return false;
        IntPtr ignored;
        return SendMessageTimeout(combo, WM_KEYDOWN, new IntPtr(VK_ESCAPE), IntPtr.Zero,
            SMTO_ABORTIFHUNG, timeoutMs, out ignored) != IntPtr.Zero;
    }

    public static int QueryComboScrollState(IntPtr combo) {
        return SendMessage(combo, PC_MSG_QUERY_SCROLL_STATE, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }

    public static bool Wheel(IntPtr window, int delta, uint timeoutMs) {
        RECT rect;
        if (!GetWindowRect(window, out rect)) return false;
        int x = (rect.Left + rect.Right) / 2;
        int y = (rect.Top + rect.Bottom) / 2;
        uint packedDelta = unchecked((uint)((ushort)(short)delta)) << 16;
        IntPtr point = new IntPtr((y << 16) | (x & 0xffff));
        IntPtr ignored;
        bool delivered = SendMessageTimeout(window, WM_MOUSEWHEEL, new IntPtr(unchecked((int)packedDelta)),
            point, SMTO_ABORTIFHUNG, timeoutMs, out ignored) != IntPtr.Zero;
        if (delivered) UpdateWindow(window);
        return delivered;
    }

    public static uint GdiHandles(Process process) { return GetGuiResources(process.Handle, 0); }
    public static uint UserHandles(Process process) { return GetGuiResources(process.Handle, 1); }

    public static int CloseAll(uint processId) {
        int sent = 0;
        EnumWindows((window, unused) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner == processId && PostMessage(window, WM_CLOSE, IntPtr.Zero, IntPtr.Zero)) ++sent;
            return true;
        }, IntPtr.Zero);
        return sent;
    }
}
'@
}

$surfaceClasses = @(
    'RemapPanelClass',
    'KeyboardSubConfigPage',
    'KeyboardSubTesterPage',
    'KeyboardSubGlobalSettingsPage',
    'KeyboardSubInputOverlayPage',
    'KeyboardSubMouseSettingsPage'
)
$tracePath = Join-Path (Split-Path -Parent $ExePath) 'HallJoyStabilityTrace.log'
$traceStartLength = if (Test-Path -LiteralPath $tracePath) { (Get-Item -LiteralPath $tracePath).Length } else { 0L }
$process = $null
$results = @()
$comboResults = @()
$failure = $null
$gdiStart = 0
$userStart = 0
$gdiMax = 0
$userMax = 0

try {
    $process = Start-Process -FilePath $ExePath -PassThru
    $deadline = (Get-Date).AddSeconds(15)
    $main = [IntPtr]::Zero
    do {
        Start-Sleep -Milliseconds 50
        $main = [HallJoyUiScrollStress]::FindMain([uint32]$process.Id)
    } while ($main -eq [IntPtr]::Zero -and (Get-Date) -lt $deadline -and -not $process.HasExited)
    if ($main -eq [IntPtr]::Zero) { throw 'Main HallJoy window was not found.' }

    $tab = [HallJoyUiScrollStress]::FindChildByClass($main, 'SysTabControl32', $true)
    if ($tab -eq [IntPtr]::Zero) { throw 'Keyboard tab control was not found.' }
    $tabCount = [HallJoyUiScrollStress]::TabCount($tab)
    if ($tabCount -ne $surfaceClasses.Count) {
        throw "Expected $($surfaceClasses.Count) keyboard pages, found $tabCount."
    }

    # Allocate every retained surface before the leak baseline. The initial GDI
    # increase is expected cache creation; only steady-state growth is a defect.
    for ($index = 0; $index -lt $tabCount; ++$index) {
        [HallJoyUiScrollStress]::SelectTab($tab, $index) | Out-Null
        $surface = [HallJoyUiScrollStress]::ActivateSurface($main, $surfaceClasses[$index], $surfaceClasses)
        if ($surface -eq [IntPtr]::Zero) { throw "Could not activate $($surfaceClasses[$index]) during warm-up." }
        for ($event = 0; $event -lt 24; ++$event) {
            $delta = if ($event -lt 12) { -120 } else { 120 }
            if (-not [HallJoyUiScrollStress]::Wheel($surface, $delta, 1000)) {
                throw "$($surfaceClasses[$index]) stopped responding during warm-up."
            }
        }
        Start-Sleep -Milliseconds 80
    }

    $gdiStart = [HallJoyUiScrollStress]::GdiHandles($process)
    $userStart = [HallJoyUiScrollStress]::UserHandles($process)
    $gdiMax = $gdiStart
    $userMax = $userStart

    for ($index = 0; $index -lt $tabCount; ++$index) {
        [HallJoyUiScrollStress]::SelectTab($tab, $index) | Out-Null
        $className = $surfaceClasses[$index]
        $surface = [HallJoyUiScrollStress]::ActivateSurface($main, $className, $surfaceClasses)
        if ($surface -eq [IntPtr]::Zero) { throw "Could not activate $className." }
        Start-Sleep -Milliseconds 80
        if (-not [HallJoyUiScrollStress]::Responsive($surface, 1000)) {
            throw "$className was unresponsive before wheel stress."
        }

        $watch = [Diagnostics.Stopwatch]::StartNew()
        for ($event = 0; $event -lt $WheelEventsPerPage; ++$event) {
            $phase = [Math]::Floor($event / 24) % 2
            $delta = if ($phase -eq 0) { -120 } else { 120 }
            if (-not [HallJoyUiScrollStress]::Wheel($surface, $delta, 1000)) {
                throw "$className stopped responding at wheel event $event."
            }
            if ($EventDelayMs -gt 0) { Start-Sleep -Milliseconds $EventDelayMs }
            if (($event % 24) -eq 23) {
                $gdiMax = [Math]::Max($gdiMax, [HallJoyUiScrollStress]::GdiHandles($process))
                $userMax = [Math]::Max($userMax, [HallJoyUiScrollStress]::UserHandles($process))
            }
        }
        $watch.Stop()
        if (-not [HallJoyUiScrollStress]::Responsive($surface, 1000)) {
            throw "$className was unresponsive after wheel stress."
        }
        $results += [pscustomobject]@{
            tab_index = $index
            surface_class = $className
            wheel_events = $WheelEventsPerPage
            elapsed_ms = $watch.ElapsedMilliseconds
            wheel_update_cycles_per_second = if ($watch.ElapsedMilliseconds -gt 0) {
                [Math]::Round($WheelEventsPerPage * 1000.0 / $watch.ElapsedMilliseconds, 1)
            } else { $null }
            responsive = $true
        }
        Write-Host "Tab $index $className`: PASS ($WheelEventsPerPage wheel events, $($watch.ElapsedMilliseconds) ms)"
    }

    $comboCases = @(
        [pscustomobject]@{ tab = 1; name = 'Configuration preset'; x = 350; y = 266 },
        [pscustomobject]@{ tab = 3; name = 'Global profile'; x = 100; y = 53 },
        [pscustomobject]@{ tab = 3; name = 'Keyboard layout'; x = 100; y = 121 },
        [pscustomobject]@{ tab = 4; name = 'Input Overlay fill direction'; x = 180; y = 89 },
        [pscustomobject]@{ tab = 4; name = 'Input Overlay depth display'; x = 390; y = 89 },
        [pscustomobject]@{ tab = 4; name = 'Input Overlay label font'; x = 100; y = 433 }
    )
    foreach ($case in $comboCases) {
        [HallJoyUiScrollStress]::SelectTab($tab, $case.tab) | Out-Null
        $surface = [HallJoyUiScrollStress]::ActivateSurface(
            $main, $surfaceClasses[$case.tab], $surfaceClasses)
        for ($event = 0; $event -lt 20; ++$event) {
            [HallJoyUiScrollStress]::Wheel($surface, 120, 1000) | Out-Null
        }
        $x = [HallJoyUiScrollStress]::Scale($surface, $case.x)
        $y = [HallJoyUiScrollStress]::Scale($surface, $case.y)
        if (-not [HallJoyUiScrollStress]::Click($surface, $x, $y, 1000)) {
            throw "$($case.name) did not accept its retained-face click."
        }
        Start-Sleep -Milliseconds 220
        $popupOpen = [HallJoyUiScrollStress]::CountVisibleTopWindowsByClass(
            [uint32]$process.Id, 'PremiumCombo_Popup')
        $controllerOpen = [HallJoyUiScrollStress]::CountVisibleChildrenByClass(
            $main, 'PremiumCombo_Control')
        if ($popupOpen -ne 1 -or $controllerOpen -ne 1) {
            throw "$($case.name) popup lifecycle did not open exactly one popup/controller (popup=$popupOpen controller=$controllerOpen)."
        }
        $popup = [HallJoyUiScrollStress]::FindVisibleTopWindowByClass(
            [uint32]$process.Id, 'PremiumCombo_Popup')
        if ($popup -eq [IntPtr]::Zero) {
            throw "$($case.name) popup was not available for wheel routing."
        }
        $controller = [HallJoyUiScrollStress]::FindChildByClass(
            $main, 'PremiumCombo_Control', $true)
        $beforeState = [HallJoyUiScrollStress]::QueryComboScrollState($controller)
        $beforeTop = $beforeState -band 0xff
        $maxTop = ($beforeState -shr 8) -band 0xff
        $beforeSelection = ($beforeState -shr 16) - 1
        $wheelDelta = if ($beforeTop -lt $maxTop) { -120 } else { 120 }
        if (-not [HallJoyUiScrollStress]::Wheel($popup, $wheelDelta, 1000)) {
            throw "$($case.name) popup did not accept routed wheel input."
        }
        $afterState = [HallJoyUiScrollStress]::QueryComboScrollState($controller)
        $afterTop = $afterState -band 0xff
        $afterSelection = ($afterState -shr 16) - 1
        $expectsOverflow = $case.name -eq 'Input Overlay label font'
        if ($expectsOverflow -and ($maxTop -le 0 -or $afterTop -eq $beforeTop)) {
            throw "$($case.name) did not scroll its overflowing list viewport (top=$beforeTop->$afterTop max=$maxTop)."
        }
        if (-not $expectsOverflow -and ($maxTop -ne 0 -or $afterTop -ne $beforeTop)) {
            throw "$($case.name) scrolled even though all options fit (top=$beforeTop->$afterTop max=$maxTop)."
        }
        if ($afterSelection -ne $beforeSelection) {
            throw "$($case.name) wheel changed selection ($beforeSelection->$afterSelection)."
        }
        if (-not [HallJoyUiScrollStress]::Responsive($popup, 1000) -or
            [HallJoyUiScrollStress]::CountVisibleTopWindowsByClass(
                [uint32]$process.Id, 'PremiumCombo_Popup') -ne 1) {
            throw "$($case.name) popup closed or became unresponsive during wheel routing."
        }
        if (-not [HallJoyUiScrollStress]::CloseVisibleCombo($main, 1000)) {
            throw "$($case.name) visible popup controller did not accept Escape."
        }
        Start-Sleep -Milliseconds 320
        $popupClosed = [HallJoyUiScrollStress]::CountVisibleTopWindowsByClass(
            [uint32]$process.Id, 'PremiumCombo_Popup')
        $controllerClosed = [HallJoyUiScrollStress]::CountVisibleChildrenByClass(
            $main, 'PremiumCombo_Control')
        if ($popupClosed -ne 0 -or $controllerClosed -ne 0) {
            throw "$($case.name) leaked a visible popup/controller after close (popup=$popupClosed controller=$controllerClosed)."
        }
        $comboResults += [pscustomobject]@{
            name = $case.name
            popup_open_count = $popupOpen
            controller_open_count = $controllerOpen
            wheel_route_cycles = 1
            scroll_top_before = $beforeTop
            scroll_top_after = $afterTop
            max_scroll_top = $maxTop
            selection_unchanged = $true
            popup_closed_count = $popupClosed
            controller_closed_count = $controllerClosed
            status = 'passed'
        }
        Write-Host "$($case.name): PASS (one popup/controller while open, zero after close)"
    }

    $gdiEnd = [HallJoyUiScrollStress]::GdiHandles($process)
    $userEnd = [HallJoyUiScrollStress]::UserHandles($process)
    if (($gdiEnd - $gdiStart) -gt 12 -or ($userEnd - $userStart) -gt 12) {
        throw "GUI handle growth exceeded the bounded allowance: GDI $gdiStart->$gdiEnd, USER $userStart->$userEnd."
    }

    [HallJoyUiScrollStress]::CloseAll([uint32]$process.Id) | Out-Null
    if (-not $process.WaitForExit(15000)) { throw 'HallJoy did not exit after WM_CLOSE.' }
    if ($process.ExitCode -ne 0) { throw "HallJoy exit code was $($process.ExitCode)." }

    $summary = [ordered]@{
        status = 'passed'
        executable = $ExePath
        executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $ExePath).Hash
        pages = $results
        combo_popup_lifecycle = $comboResults
        gdi_handles = [ordered]@{ start = $gdiStart; max = $gdiMax; end = $gdiEnd }
        user_handles = [ordered]@{ start = $userStart; max = $userMax; end = $userEnd }
        exit_code = $process.ExitCode
    }
    $summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'summary.json') -Encoding UTF8

    if (Test-Path -LiteralPath $tracePath) {
        Copy-Item -LiteralPath $tracePath -Destination (Join-Path $EvidenceRoot 'HallJoyStabilityTrace.log') -Force
        $traceBytes = [IO.File]::ReadAllBytes($tracePath)
        if ($traceBytes.Length -gt $traceStartLength) {
            $newBytes = New-Object byte[] ($traceBytes.Length - $traceStartLength)
            [Array]::Copy($traceBytes, $traceStartLength, $newBytes, 0, $newBytes.Length)
            [IO.File]::WriteAllBytes((Join-Path $EvidenceRoot 'trace-appended.log'), $newBytes)
        }
    }
    Write-Host "HallJoy unified UI scroll stress: PASS ($tabCount/$tabCount pages)" -ForegroundColor Green
    Write-Host "GUI handles: GDI $gdiStart/$gdiMax/$gdiEnd, USER $userStart/$userMax/$userEnd (start/max/end)"
    Write-Host "Evidence: $EvidenceRoot"
} catch {
    $failure = $_
    throw
} finally {
    if ($null -ne $process -and -not $process.HasExited) {
        [HallJoyUiScrollStress]::CloseAll([uint32]$process.Id) | Out-Null
        $process.WaitForExit(5000) | Out-Null
    }
    if ($null -ne $failure) {
        [ordered]@{ status = 'failed'; error = $failure.Exception.Message; pages = $results } |
            ConvertTo-Json -Depth 6 |
            Set-Content -LiteralPath (Join-Path $EvidenceRoot 'summary.json') -Encoding UTF8
    }
}
