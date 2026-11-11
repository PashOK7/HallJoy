# Keep physical-UAP isolation while briefly suspending one known local app.
function Get-HallJoyUapProcesses {
    . (Join-Path $PSScriptRoot 'halljoy_process_identity.ps1')
    foreach ($owner in @(Get-CimInstance Win32_Process -Filter "Name='HallJoy.exe'" -ErrorAction Stop)) {
        # Do not trust CIM's potentially empty ExecutablePath for elevated apps.
        $image = [HallJoyBuild.ProcessIdentity]::ImagePath($owner.ProcessId)
        [pscustomobject]@{
            ProcessId = $owner.ProcessId
            ParentProcessId = $owner.ParentProcessId
            SessionId = $owner.SessionId
            ExecutablePath = [IO.Path]::GetFullPath($image)
            CommandLine = $owner.CommandLine
            CreationDate = $owner.CreationDate
        }
    }
}

function Get-HallJoyUapSuspendTarget {
    param([object[]]$Owners, [string]$Root, [int]$SessionId)
    if ($Owners.Count -eq 0) { return $null }
    $allowed = @(
        [IO.Path]::GetFullPath((Join-Path $Root '.local/portable/HallJoy.exe'))
        [IO.Path]::GetFullPath((Join-Path $Root 'build/HallJoy.exe'))
        [IO.Path]::GetFullPath((Join-Path $Root 'build/bin/Release/x64/HallJoy.exe'))
    )
    $main = @()
    foreach ($owner in $Owners) {
        if ($owner.SessionId -ne $SessionId -or $owner.ExecutablePath -notin $allowed -or
            -not $owner.CommandLine -or $owner.CommandLine -notmatch '^\s*(?:"[^"]+"|\S+)\s*(.*)$') {
            throw "Cannot safely suspend HallJoy PID=$($owner.ProcessId). Close other/unrecognized instances manually before rebuilding."
        }
        $arguments = $Matches[1].Trim()
        if (-not $arguments) { $main += $owner }
        elseif ($arguments -notmatch '^--(diagnostic-watch|halljoy-analog-host|halljoy-vigem-output-host|halljoy-shark-worker|halljoy-mini60-worker)([="\s]|$)') {
            throw "Unrecognized HallJoy arguments for PID=$($owner.ProcessId); no processes were closed."
        }
    }
    if ($main.Count -ne 1) { throw 'Expected one local HallJoy application and its workers; no processes were closed.' }
    foreach ($owner in $Owners) {
        if ($owner.ExecutablePath -ne $main[0].ExecutablePath -or
            ($owner.ProcessId -ne $main[0].ProcessId -and $owner.ParentProcessId -ne $main[0].ProcessId)) {
            throw 'HallJoy processes are not one verified application family; no processes were closed.'
        }
    }
    return $main[0].ExecutablePath
}

function Invoke-HallJoyIsolatedUapCheck {
    param([Parameter(Mandatory = $true)][scriptblock]$Check)
    $root = Split-Path -Parent $PSScriptRoot
    $owners = @(Get-HallJoyUapProcesses)
    $target = Get-HallJoyUapSuspendTarget -Owners $owners -Root $root -SessionId (Get-Process -Id $PID).SessionId
    $closer = Join-Path $PSScriptRoot 'close_project_halljoy.ps1'
    $state = $null
    $shutdownAttempted = $false
    if ($target) {
        $state = & $closer -TargetPath $target -InspectOnly
        if (-not $state.WasRunning -or $state.Count -ne $owners.Count) { throw 'HallJoy changed during inspection; retry the build.' }
        $originalHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        # Revalidate just before shutdown, including creation times to detect PID reuse.
        $before = $owners | Sort-Object ProcessId | ConvertTo-Json -Compress
        $fresh = @(Get-HallJoyUapProcesses)
        $null = Get-HallJoyUapSuspendTarget -Owners $fresh -Root $root -SessionId (Get-Process -Id $PID).SessionId
        $after = $fresh | Sort-Object ProcessId | ConvertTo-Json -Compress
        if ($before -ne $after) { throw 'HallJoy processes changed before shutdown; retry the build.' }
    }
    try {
        if ($target) {
            Write-Host "Temporarily closing HallJoy for the physical-UAP check: $target" -ForegroundColor Cyan
            $shutdownAttempted = $true
            $null = & $closer -TargetPath $target
        }
        if (@(Get-HallJoyUapProcesses).Count -ne 0) {
            throw 'Physical-UAP isolation failed: a HallJoy process is still active. The check was not started.'
        }
        & $Check
    } finally {
        if ($shutdownAttempted -and $state.WasRunning) {
            $now = & $closer -TargetPath $target -InspectOnly
            if (-not $now.WasRunning) {
                if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $originalHash) {
                    throw 'The previous HallJoy executable changed during the check; refusing to launch different bytes.'
                }
                $restored = Start-Process -FilePath $target -WorkingDirectory (Split-Path -Parent $target) -WindowStyle $state.WindowStyle -PassThru
                if ($restored.WaitForExit(1000)) { throw 'The previous HallJoy exited during restoration.' }
                Write-Host "Restored the previously running HallJoy: $target" -ForegroundColor Green
            }
        }
    }
}
