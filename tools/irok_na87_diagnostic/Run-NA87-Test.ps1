[CmdletBinding()]
param([switch]$InventoryOnly)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$output = Join-Path $PSScriptRoot ('NA87-results-' + $stamp)
if (Test-Path -LiteralPath $output) { throw 'Result directory already exists.' }
[IO.Directory]::CreateDirectory($output) | Out-Null
$exe = Join-Path $PSScriptRoot 'HallJoy-NA87-Diagnostic.exe'
$diagnosticExit = -1
try {
    & (Join-Path $PSScriptRoot 'collect_irok_na87_inventory.ps1') -OutputDirectory $output
    $mode = if ($InventoryOnly) { '--inventory' } else { '--output' }
    & $exe $mode $output
    $diagnosticExit = $LASTEXITCODE
} finally {
    # Keep evidence even after a failed device operation or an interrupted test.
    $summary = [Collections.Generic.List[string]]::new()
    $summary.Add('NA87 diagnostic evidence; this is not a hardware support PASS.')
    $summary.Add('Diagnostic process exit: ' + $diagnosticExit)
    $summary.Add('Depths above 40 are outside the ND75 reference range; raw bytes remain in JSONL.')
    $summary.Add('Last reported nonzero depth is not proof that a key is physically held.')
    foreach ($file in @(Get-ChildItem -LiteralPath $output -Filter '*.jsonl' -File)) {
        foreach ($line in [IO.File]::ReadLines($file.FullName)) {
            if ($line -match '"kind":"(identity|capability|phase_summary|learn_result|probe_result|selected_profile|worker_timeout_or_cancel|session_complete|no_verified_na87_identity|no_matching_vendor_interface|reconnect_identity_failed|snapshot_result|control_snapshot|fallback_complete|cleanup_result|reconnect_result|snapshot_budget_end)"') {
                $summary.Add($file.Name + ': ' + $line)
            }
        }
    }
    $summaryPath = Join-Path $output 'SUMMARY.txt'
    $bytes = [Text.Encoding]::UTF8.GetBytes(($summary -join "`r`n"))
    $stream = [IO.File]::Open($summaryPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
    $hashes = @('Executable: ' + (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash)
    foreach ($file in @(Get-ChildItem -LiteralPath $output -File)) {
        $hashes += (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash + '  ' + $file.Name
    }
    $hashPath = Join-Path $output 'SHA256SUMS.txt'
    $bytes = [Text.Encoding]::UTF8.GetBytes(($hashes -join "`r`n"))
    $stream = [IO.File]::Open($hashPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
    $zip = $output + '.zip'
    if (Test-Path -LiteralPath $zip) { throw 'Result archive already exists.' }
    Compress-Archive -LiteralPath $output -DestinationPath $zip -CompressionLevel Optimal
    Write-Host ''
    Write-Host 'Send this single results archive to the HallJoy owner:'
    Write-Host $zip
    Write-Host 'If interrupted, reconnect the keyboard before normal use.'
}
