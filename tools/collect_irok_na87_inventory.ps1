[CmdletBinding()]
param([string]$OutputDirectory = $PSScriptRoot)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Windows PnP metadata only. No HID report I/O, keyboard events or vendor commands.
# 0416:7372 is a family identifier, not sufficient evidence of the NA87 model.
$propertiesToRead = @(
    'DEVPKEY_Device_HardwareIds',
    'DEVPKEY_Device_CompatibleIds',
    'DEVPKEY_Device_BusReportedDeviceDesc',
    'DEVPKEY_Device_Manufacturer',
    'DEVPKEY_Device_DriverVersion',
    'DEVPKEY_Device_DriverProvider'
)
$items = @()
$enumerationStatus = 'ok'
$enumerationErrorType = $null
try {
    $devices = @(Get-PnpDevice -PresentOnly -ErrorAction Stop | Where-Object {
        $_.InstanceId -match '^(USB|HID)\\VID_0416&PID_7372(?:&|\\)'
    })
    foreach ($device in $devices) {
        $properties = [ordered]@{}
        $unavailable = @()
        foreach ($key in $propertiesToRead) {
            try {
                $property = Get-PnpDeviceProperty -InstanceId $device.InstanceId `
                    -KeyName $key -ErrorAction Stop
                $properties[$key] = $property.Data
            } catch {
                $unavailable += $key
            }
        }
        # Retain VID/PID/revision/interface/collection identity, omit the unique
        # instance suffix (which can contain serial numbers or port topology).
        $parts = $device.InstanceId -split '\\', 3
        $items += [ordered]@{
            Enumerator = $parts[0]
            HardwareNode = $parts[1]
            Class = $device.Class
            FriendlyName = $device.FriendlyName
            Status = $device.Status
            Properties = $properties
            UnavailableProperties = @($unavailable)
        }
    }
} catch {
    $enumerationStatus = 'error'
    $enumerationErrorType = $_.Exception.GetType().FullName
}
if ($enumerationStatus -eq 'ok' -and $items.Count -eq 0) {
    $enumerationStatus = 'not_found'
}
$report = [ordered]@{
    SchemaVersion = 1
    CollectedAtUtc = [DateTime]::UtcNow.ToString('o')
    Scope = 'Present Windows PnP nodes for USB/HID VID_0416 PID_7372 only'
    EnumerationStatus = $enumerationStatus
    EnumerationErrorType = $enumerationErrorType
    MatchedNodeCount = $items.Count
    Nodes = @($items)
    FirmwareVersion = 'Not collected: obtain from the official driver UI'
    Limitations = @(
        'Multiple PnP nodes can belong to one keyboard.',
        'VID/PID alone does not identify NA87 or prove analog support.',
        'DriverVersion is a Windows driver version, not keyboard firmware.',
        'No HID report lengths, live depths or keyboard events are collected.'
    )
}
$directory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not [IO.Directory]::Exists($directory)) {
    throw 'Output directory does not exist. Extract the package to a writable folder.'
}
$name = 'NA87-inventory-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + `
    '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8) + '.json'
$path = Join-Path $directory $name
$bytes = [Text.Encoding]::UTF8.GetBytes(($report | ConvertTo-Json -Depth 8))
$stream = [IO.File]::Open($path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write,
    [IO.FileShare]::None)
try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
Write-Host ('Saved: ' + $path)
Write-Host ('Result: ' + $enumerationStatus + '; matching PnP nodes: ' + $items.Count)
Write-Host 'Return the JSON plus the exact model and firmware shown by the official driver.'
if ($enumerationStatus -eq 'error') { exit 1 }
