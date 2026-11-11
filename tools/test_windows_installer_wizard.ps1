[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$InstallerPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$installer = (Get-Item -LiteralPath $InstallerPath).FullName
$testPrefix = [IO.Path]::GetFullPath((Join-Path $root 'build/tests')).TrimEnd('\') + '\'
if (-not $installer.StartsWith($testPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use an isolated TEST_ROOT installer from build/tests.'
}
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class InstallerWizardProbe {
    delegate bool Visitor(IntPtr window, IntPtr unused);
    [DllImport("user32.dll")] static extern bool EnumWindows(Visitor visit, IntPtr unused);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, Visitor visit, IntPtr unused);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr dialog, int id);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr w, StringBuilder text);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window, uint message, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr window);
    public static IntPtr Dialog(int pid) {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr window, IntPtr unused) {
            uint owner; GetWindowThreadProcessId(window, out owner);
            var name = new StringBuilder(256); GetClassName(window, name, name.Capacity);
            if (owner == pid && name.ToString() == "#32770") { result=window; return false; }
            return true;
        }, IntPtr.Zero);
        return result;
    }
    static string Text(IntPtr window) {
        var text = new StringBuilder(32768);
        SendMessage(window, 0x000D, (IntPtr)text.Capacity, text);
        return text.ToString();
    }
    public static string Page(IntPtr dialog) {
        var result = new StringBuilder(Text(dialog));
        EnumChildWindows(dialog, delegate(IntPtr window, IntPtr unused) {
            result.AppendLine(Text(window)); return true;
        }, IntPtr.Zero);
        return result.ToString();
    }
    public static void Next(IntPtr dialog) {
        var button = GetDlgItem(dialog, 1);
        if (button == IntPtr.Zero || !IsWindowEnabled(button)) throw new Exception("Next is not available");
        PostMessage(button, 0x00F5, IntPtr.Zero, IntPtr.Zero);
    }
}
'@
$process = Start-Process -FilePath $installer -WindowStyle Hidden -PassThru
$pages = New-Object System.Collections.Generic.List[string]
try {
    foreach ($expected in @('Welcome to','License Agreement','Data and permissions','Choose Install Location','Choose Components')) {
        $deadline = [DateTime]::UtcNow.AddSeconds(10)
        $found = $false
        do {
            if ($process.HasExited) { throw "Wizard exited before $expected" }
            $dialog = [InstallerWizardProbe]::Dialog($process.Id)
            $text = if ($dialog -ne [IntPtr]::Zero) { [InstallerWizardProbe]::Page($dialog) } else { '' }
            $ready = $text.Contains($expected)
            if ($expected -eq 'Data and permissions') { $ready = $ready -and $text.Contains('Select Next to continue') }
            if ($ready) { $found = $true; break }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $deadline)
        if (-not $found) { throw "Page not found: $expected. Controls: $text" }
        $pages.Add($expected)
        Write-Output "WIZARD_PAGE=PASS $expected"
        if ($expected -ne 'Choose Components') { [InstallerWizardProbe]::Next($dialog) }
    }
    $pages | ConvertTo-Json | Set-Content -LiteralPath (Join-Path (Split-Path -Parent $installer) 'wizard-pages.json')
    Write-Output 'INSTALLER_WIZARD=PASS five pages; install button was not clicked'
} finally {
    # Only this test's exact process; no installation was started by the probe.
    if (-not $process.HasExited) { Stop-Process -InputObject $process -Force; $null = $process.WaitForExit(5000) }
}
