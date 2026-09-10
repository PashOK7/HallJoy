[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

& (Join-Path $PSScriptRoot 'build_aula_diagnostic.ps1') `
    -PackageName 'gravastar-v75-diagnostic'
