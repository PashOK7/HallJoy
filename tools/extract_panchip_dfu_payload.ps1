[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$InputExe,

    [Parameter(Mandatory = $true)]
    [string]$OutputFirmware
)

$ErrorActionPreference = 'Stop'

$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $InputExe))
if ($bytes.Length -lt 12) {
    throw 'Input is shorter than the Panchip DFU trailer.'
}

$magic = [uint32]0x55AA3377
$payloadLength = [BitConverter]::ToUInt32($bytes, $bytes.Length - 12) -bxor $magic
$metadataLength = [BitConverter]::ToUInt32($bytes, $bytes.Length - 8) -bxor $magic
$payloadStart = [int64]$bytes.Length - 12 - [int64]$metadataLength - [int64]$payloadLength

if ($payloadStart -lt 128 -or $payloadLength -lt 1 -or
    ($payloadStart + $payloadLength) -gt ($bytes.Length - 12)) {
    throw 'Trailer lengths do not describe a valid Panchip DFU payload.'
}

$firmware = New-Object byte[] $payloadLength
for ($offset = 0; $offset -lt $payloadLength; $offset++) {
    $firmware[$offset] = $bytes[$payloadStart + $offset] -bxor (($offset + 0xAA) -band 0xFF)
}

[System.IO.File]::WriteAllBytes($OutputFirmware, $firmware)
$hash = (Get-FileHash -LiteralPath $OutputFirmware -Algorithm SHA256).Hash

[pscustomobject]@{
    Input = (Resolve-Path -LiteralPath $InputExe).Path
    PayloadStart = ('0x{0:X}' -f $payloadStart)
    PayloadLength = ('0x{0:X}' -f $payloadLength)
    Output = (Resolve-Path -LiteralPath $OutputFirmware).Path
    SHA256 = $hash
}
