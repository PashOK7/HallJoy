# Exercise the actual BUILD.cmd -> Windows PowerShell entry without compiling.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'build_environment.ps1')
$python = Resolve-HallJoyPython
$root = Split-Path -Parent $PSScriptRoot
$sandbox = Join-Path $root ('build/tests/full-build-environment-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path (Join-Path $sandbox 'tools') -Force
foreach ($relative in @('BUILD.cmd', 'tools/build.ps1', 'tools/build_environment.ps1')) {
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination (Join-Path $sandbox $relative)
}
# This deliberately nonexecutable alias must be skipped before probing Python.
$aliasDirectory = Join-Path $sandbox 'Microsoft/WindowsApps'
$null = New-Item -ItemType Directory -Path $aliasDirectory -Force
[IO.File]::WriteAllText((Join-Path $aliasDirectory 'python.exe'), 'not a Python interpreter')
$originalPath = $env:PATH
$checks = New-Object System.Collections.Generic.List[string]
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAILED: $Message" }
    $checks.Add($Message)
    Write-Output "PASS: $Message"
}
try {
    $env:PATH = $aliasDirectory + ';' + $originalPath
    $out = Join-Path $sandbox 'default.log'
    $err = Join-Path $sandbox 'default.err'
    $arguments = '/d /c ""' + (Join-Path $sandbox 'BUILD.cmd') + '" -CheckOnly"'
    $process = Start-Process -FilePath $env:ComSpec -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    # Retain the process handle so Windows PowerShell reports ExitCode after wait.
    $processHandle = $process.Handle
    if (-not $process.WaitForExit(30000)) {
        Stop-Process -InputObject $process -Force
        throw "BUILD.cmd -CheckOnly timed out. Evidence: $sandbox"
    }
    Assert ($process.ExitCode -eq 0) 'BUILD.cmd forwards CheckOnly and returns without pausing'
    $output = Get-Content -LiteralPath $out -Raw
    Assert ($output.Contains('FULL_BUILD_ENVIRONMENT=PASS')) 'Full builder initializes its environment before checking source/dependencies'
    Assert (-not $output.Contains('Using Python: ' + $aliasDirectory)) 'WindowsApps alias is not selected'
    Assert (-not (Test-Path -LiteralPath (Join-Path $sandbox 'build'))) 'Read-only check creates no build output'
    Assert (-not (Test-Path -LiteralPath (Join-Path $sandbox '.cache'))) 'Read-only check downloads no dependencies'
    # Reproduce the public PowerShell prompt -> CMD -> PowerShell entry too,
    # rather than testing only a direct Start-Process cmd.exe invocation.
    $caller = Join-Path $sandbox 'invoke-from-powershell.ps1'
    [IO.File]::WriteAllText($caller, '& (Join-Path $PSScriptRoot ''BUILD.cmd'') -CheckOnly' + [Environment]::NewLine + 'exit $LASTEXITCODE')
    $callerOutput = Join-Path $sandbox 'powershell-caller.log'
    $callerProcess = Start-Process -FilePath powershell.exe -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File "' + $caller + '"') -WindowStyle Hidden -PassThru -RedirectStandardOutput $callerOutput -RedirectStandardError (Join-Path $sandbox 'powershell-caller.err')
    $callerHandle = $callerProcess.Handle
    if (-not $callerProcess.WaitForExit(30000)) {
        Stop-Process -InputObject $callerProcess -Force
        throw "PowerShell -> BUILD.cmd -CheckOnly timed out. Evidence: $sandbox"
    }
    Assert ($callerProcess.ExitCode -eq 0) 'PowerShell prompt forwards CheckOnly through CMD and exits successfully'
    $callerText = Get-Content -LiteralPath $callerOutput -Raw
    Assert (([regex]::Matches($callerText, 'FULL_BUILD_ENVIRONMENT=PASS')).Count -eq 1) 'PowerShell invocation reports exactly one environment-check completion'
    Assert (-not (Test-Path -LiteralPath (Join-Path $sandbox 'build')) -and -not (Test-Path -LiteralPath (Join-Path $sandbox '.cache'))) 'PowerShell invocation starts no build or dependency download'
    $entry = Join-Path $sandbox 'tools/build.ps1'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $entry -CheckOnly -PythonPath $python
    Assert ($LASTEXITCODE -eq 0) 'Full builder accepts an explicit existing Python'
    $invalidArguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $entry + '" -CheckOnly -PythonPath "' + (Join-Path $sandbox 'missing/python.exe') + '"'
    $invalid = Start-Process -FilePath powershell.exe -ArgumentList $invalidArguments -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput (Join-Path $sandbox 'invalid.log') -RedirectStandardError (Join-Path $sandbox 'invalid.err')
    Assert ($invalid.ExitCode -ne 0) 'Full builder rejects an invalid explicit Python before building'
} finally {
    $env:PATH = $originalPath
}
@{ checks = @($checks.ToArray()) } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $sandbox 'results.json') -Encoding UTF8
Write-Output "FULL_BUILD_ENVIRONMENT_TESTS=PASS checks=$($checks.Count) evidence=$sandbox"
