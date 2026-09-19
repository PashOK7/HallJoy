$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$script = Join-Path (Split-Path -Parent $PSScriptRoot) 'collect_irok_na87_inventory.ps1'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('HallJoyNA87InventoryTest-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null

# Synthetic PnP fixtures only: no device is enumerated or opened by this test.
function Get-PnpDevice {
    [CmdletBinding()]
    param([switch]$PresentOnly)
    if (-not $PresentOnly) { throw 'Expected present-only inventory.' }
    foreach ($id in @(
        'USB\VID_0416&PID_7372\PRIVATE_SERIAL_SENTINEL',
        'HID\VID_0416&PID_7372&MI_01&COL02\PRIVATE_SERIAL_SENTINEL',
        'HID\VID_0416&PID_73720\UNRELATED',
        'HID\VID_1234&PID_7372\UNRELATED'
    )) {
        [pscustomobject]@{InstanceId=$id; Class='HIDClass'; FriendlyName='Fixture'; Status='OK'}
    }
}
function Get-PnpDeviceProperty {
    [CmdletBinding()]
    param([string]$InstanceId, [string]$KeyName)
    if ($InstanceId -like '*UNRELATED') { throw 'Unrelated device property requested.' }
    if ($KeyName -eq 'DEVPKEY_Device_DriverProvider') { throw 'Optional property unavailable.' }
    [pscustomobject]@{Data=@('FixtureValue')}
}
& $script -OutputDirectory $testRoot
$files = @(Get-ChildItem -LiteralPath $testRoot -Filter '*.json')
if ($files.Count -ne 1) { throw 'Expected one report.' }
$text = [IO.File]::ReadAllText($files[0].FullName)
$report = $text | ConvertFrom-Json
if ($report.EnumerationStatus -ne 'ok' -or $report.MatchedNodeCount -ne 2 -or
    $report.Nodes.Count -ne 2) { throw 'Incorrect matching node count.' }
if ($text.Contains('PRIVATE_SERIAL_SENTINEL') -or $text.Contains('UNRELATED')) {
    throw 'Unexpected unique instance suffix or unrelated device in report.'
}
if ($report.Nodes[1].HardwareNode -ne 'VID_0416&PID_7372&MI_01&COL02') {
    throw 'HID interface/collection metadata was not preserved.'
}
if ($report.Nodes[0].UnavailableProperties -notcontains 'DEVPKEY_Device_DriverProvider') {
    throw 'Missing optional property was not recorded.'
}
Write-Host 'NA87_INVENTORY_FIXTURES=PASS exact_filter=1 collections=1 missing_property=1 instance_suffix_omitted=1'
Write-Host ('Evidence: ' + $testRoot)
