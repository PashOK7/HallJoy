[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$TargetPath,
    [switch]$InspectOnly,
    [ValidateRange(0, 30000)][int]$GraceMs = 5000
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$target = [IO.Path]::GetFullPath($TargetPath)
$checkoutPrefix = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\') + '\'
if (-not $target.StartsWith($checkoutPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Replacement target must be inside this checkout.'
}
$session = (Get-Process -Id $PID).SessionId
function Get-TargetProcesses {
    @(Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($target)) -ErrorAction SilentlyContinue | Where-Object {
        $_.Id -ne $PID -and $_.SessionId -eq $session -and $_.Path -and
        $_.Path.Equals($target, [StringComparison]::OrdinalIgnoreCase)
    })
}
$running = @(Get-TargetProcesses)
$interactive = @($running | Where-Object {
    if ($_.MainWindowHandle -ne [IntPtr]::Zero) { return $true }
    $info = Get-CimInstance Win32_Process -Filter "ProcessId = $($_.Id)" -ErrorAction SilentlyContinue
    # Normal app invocation has no internal child-role arguments, even in tray.
    $info -and $info.CommandLine -and $info.CommandLine -notmatch '--'
})
$state = [pscustomobject]@{ WasRunning = ($interactive.Count -gt 0); Count = $running.Count; Target = $target }
if ($InspectOnly -or $running.Count -eq 0) { return $state }
foreach ($process in $interactive) {
    if (-not $process.HasExited) { $null = $process.CloseMainWindow() }
}
$deadline = [DateTime]::UtcNow.AddMilliseconds($GraceMs)
foreach ($process in $running) {
    $remaining = [Math]::Max(0, [int]($deadline - [DateTime]::UtcNow).TotalMilliseconds)
    if (-not $process.HasExited -and $remaining -gt 0) { $null = $process.WaitForExit($remaining) }
}
foreach ($process in @(Get-TargetProcesses)) {
    if ($process.HasExited) { continue }
    $verifiedStart = $process.StartTime
    $fresh = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
    if ($fresh -and $fresh.Path -eq $target -and $fresh.StartTime -eq $verifiedStart) {
        Stop-Process -InputObject $fresh -Force -ErrorAction Stop
        if (-not $fresh.WaitForExit(5000)) { throw "Target process $($fresh.Id) did not exit." }
    }
}
if (@(Get-TargetProcesses).Count -ne 0) { throw 'Target restarted during replacement shutdown.' }
return $state
