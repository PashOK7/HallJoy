# Scoped process ownership for the full build's physical-UAP check.
# Dot-sourcing this file defines functions only; CheckOnly never loads it.
function Get-HallJoyUapProcesses {
    @(Get-CimInstance Win32_Process -Filter "Name='HallJoy.exe'" -ErrorAction Stop)
}

function Get-HallJoyUapTargetState([string]$Target) {
    & (Join-Path $PSScriptRoot 'close_project_halljoy.ps1') -TargetPath $Target -InspectOnly
}

function Close-HallJoyUapTarget([string]$Target) {
    $null = & (Join-Path $PSScriptRoot 'close_project_halljoy.ps1') -TargetPath $Target
}

function Restore-HallJoyUapTargets($States) {
    $failures = New-Object System.Collections.Generic.List[string]
    foreach ($state in @($States)) {
        if (-not $state.WasRunning) { continue }
        try {
            if (-not (Test-Path -LiteralPath $state.Target -PathType Leaf)) {
                throw "Original executable is missing: $($state.Target)"
            }
            $current = Get-HallJoyUapTargetState $state.Target
            if ($current.WasRunning) { continue }
            if ((Get-FileHash -LiteralPath $state.Target -Algorithm SHA256).Hash -ne $state.RestoreHash) {
                throw "Original executable changed during the physical-UAP check; refusing to launch it."
            }
            $restored = Start-Process -FilePath $state.Target -WorkingDirectory (Split-Path -Parent $state.Target) -WindowStyle $state.WindowStyle -PassThru
            if ($restored.WaitForExit(1000)) {
                throw "Restored HallJoy exited immediately: $($restored.ExitCode)"
            }
            Write-Host "Restored HallJoy after the physical-UAP check: $($state.Target)"
        } catch {
            $failures.Add("$($state.Target): $($_.Exception.Message)")
        }
    }
    if ($failures.Count) { throw ('HallJoy restoration failed: ' + ($failures -join '; ')) }
}

function Invoke-HallJoyUapIsolation {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][scriptblock]$Check)

    $checkout = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\') + '\'
    $session = (Get-Process -Id $PID).SessionId
    . (Join-Path $PSScriptRoot 'halljoy_process_identity.ps1')

    # Prove ownership of every enumerated process before closing any target.
    # Another checkout, another Windows session or an unknown identity blocks
    # the check; no broad name-based kill is permitted.
    $targets = @()
    foreach ($info in @(Get-HallJoyUapProcesses)) {
        if (-not (Get-Process -Id $info.ProcessId -ErrorAction SilentlyContinue)) { continue }
        try { $image = [HallJoyBuild.ProcessIdentity]::ImagePath($info.ProcessId) }
        catch {
            if (-not (Get-Process -Id $info.ProcessId -ErrorAction SilentlyContinue)) { continue }
            throw "Cannot verify HallJoy identity for PID $($info.ProcessId); close it manually before the physical-UAP check. $_"
        }
        if ($info.SessionId -ne $session -or -not $image.StartsWith($checkout, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Physical-UAP isolation is blocked by HallJoy outside this checkout or Windows session. Close it manually: PID=$($info.ProcessId) path=$image"
        }
        $targets += $image
    }
    $states = @($targets | Sort-Object -Unique | ForEach-Object { Get-HallJoyUapTargetState $_ })
    foreach ($state in $states) {
        if ($state.WasRunning) {
            # Resolve every restorable image before touching any process.
            $hash = (Get-FileHash -LiteralPath $state.Target -Algorithm SHA256 -ErrorAction Stop).Hash
            $state | Add-Member -NotePropertyName RestoreHash -NotePropertyValue $hash
        }
    }

    $failure = $null
    try {
        foreach ($state in $states) {
            Write-Host "Temporarily closing HallJoy for the physical-UAP check: $($state.Target)"
            Close-HallJoyUapTarget $state.Target
        }
        # The caller and leaf ABI checker both retain their independent global
        # process guards, catching a new owner racing with this shutdown.
        & $Check
    } catch {
        $failure = $_
    } finally {
        try { Restore-HallJoyUapTargets $states }
        catch {
            if ($failure) {
                throw "Physical-UAP check failed: $($failure.Exception.Message). $($_.Exception.Message)"
            }
            throw
        }
    }
    if ($failure) { throw $failure }
}
