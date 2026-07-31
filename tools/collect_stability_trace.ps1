[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$directory = $PSScriptRoot
$running = Get-Process -Name 'HallJoyMAD68ProRNative' -ErrorAction SilentlyContinue
if ($running) {
    throw 'HallJoy is still running. Close it normally before collecting the trace.'
}

$current = Join-Path $directory 'HallJoyStabilityTrace.log'
$previous = Join-Path $directory 'HallJoyStabilityTrace.previous.log'
$bundle = Join-Path $directory 'HallJoyStabilityTraceBundle.zip'
$report = Join-Path $directory 'HallJoyStabilityTraceBundle_README.txt'

$logs = @($current, $previous) | Where-Object { Test-Path -LiteralPath $_ }
if ($logs.Count -eq 0) {
    throw 'HallJoyStabilityTrace.log was not found. Run and close HallJoy before collecting the trace.'
}

@"
HallJoy temporary stabilization trace bundle
Created: $(Get-Date -Format o)

Files:
$($logs | ForEach-Object { '- ' + (Split-Path -Leaf $_) } | Out-String)
Send HallJoyStabilityTraceBundle.zip for machine analysis.
The trace contains lifecycle/state events only. It does not contain key names,
key values, typed text, file paths, usernames or per-poll records.
"@ | Set-Content -LiteralPath $report -Encoding UTF8

if (Test-Path -LiteralPath $bundle) {
    Remove-Item -LiteralPath $bundle -Force
}
$items = @($logs) + @($report)
Compress-Archive -LiteralPath $items -DestinationPath $bundle -CompressionLevel Optimal
Remove-Item -LiteralPath $report -Force

Write-Host "Created: $bundle" -ForegroundColor Green
