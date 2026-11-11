# Shared discovery for Windows build entry points. No downloads or persistent PATH changes.
# Use this host's modules when a PS7 -> CMD -> PS5 chain inherits PS7 modules.
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1') -Force -ErrorAction Stop
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Security/Microsoft.PowerShell.Security.psd1') -Force -ErrorAction Stop
function Resolve-HallJoyPython {
    [CmdletBinding()]
    param([string]$PythonPath)
    $explicit = -not [string]::IsNullOrWhiteSpace($PythonPath)
    $candidates = @()
    if ($explicit) {
        $candidates += $PythonPath
    } else {
        $candidates += @(Get-Command python.exe -All -ErrorAction SilentlyContinue | ForEach-Object Source)
        $launcher = Get-Command py.exe -ErrorAction SilentlyContinue
        if ($launcher) {
            try {
                $found = & $launcher.Source -3 -I -c 'import sys; print(sys.executable)' 2>$null
                if ($LASTEXITCODE -eq 0) { $candidates += @($found) }
            } catch { }
        }
        # uv-managed Python does not have to be on PATH. Prefer the CI Python minor.
        $managedRoots = @($env:UV_PYTHON_INSTALL_DIR)
        if ($env:APPDATA) { $managedRoots += Join-Path $env:APPDATA 'uv/python' }
        foreach ($managedRoot in ($managedRoots | Where-Object { $_ })) {
            if (Test-Path -LiteralPath $managedRoot -PathType Container) {
                $managed = @(Get-ChildItem -LiteralPath $managedRoot -Directory -Filter 'cpython-*-windows-x86_64-none' |
                    Sort-Object @{Expression={ if ($_.Name -like 'cpython-3.12.*') { 0 } else { 1 } }}, Name)
                $candidates += @($managed | ForEach-Object { Join-Path $_.FullName 'python.exe' })
            }
        }
    }
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (-not $candidate -or $candidate -like '*\Microsoft\WindowsApps\*' -or
            -not (Test-Path -LiteralPath $candidate -PathType Leaf)) { continue }
        $candidate = (Get-Item -LiteralPath $candidate).FullName
        if ([IO.Path]::GetFileName($candidate) -ine 'python.exe') { continue }
        try {
            $probe = & $candidate -I -B -c 'import sys; sys.exit(0 if sys.version_info >= (3,10) and sys.maxsize > 2**32 else 1)' 2>&1
            if ($LASTEXITCODE -eq 0) { return $candidate }
        } catch { }
    }
    if ($explicit) { throw "PythonPath is not a working x64 Python 3.10+ python.exe: $PythonPath" }
    throw 'Python 3.10+ x64 was not found. Windows Store aliases are not Python. Install Python or pass -PythonPath <python.exe>; installed uv-managed Python is also detected.'
}

function Initialize-HallJoyBuildEnvironment {
    [CmdletBinding()]
    param([string]$PythonPath, [switch]$RequireClang)
    $python = Resolve-HallJoyPython -PythonPath $PythonPath
    $directories = @((Split-Path -Parent $python))
    $clang = $null
    if ($RequireClang) {
        $candidates = @(Get-Command clang.exe -All -ErrorAction SilentlyContinue | ForEach-Object Source)
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $candidates += @(& $vswhere -all -products '*' -find 'VC/Tools/Llvm/x64/bin/clang.exe')
        }
        $candidates += Join-Path $env:ProgramFiles 'LLVM/bin/clang.exe'
        foreach ($candidate in ($candidates | Select-Object -Unique)) {
            if (-not $candidate -or -not (Test-Path -LiteralPath $candidate -PathType Leaf)) { continue }
            $cxx = Join-Path (Split-Path -Parent $candidate) 'clang++.exe'
            if (-not (Test-Path -LiteralPath $cxx -PathType Leaf)) { continue }
            try {
                $target = & $candidate -dumpmachine 2>&1
                if ($LASTEXITCODE -ne 0 -or ($target -join '') -notmatch '^x86_64-.*windows') { continue }
                $probe = & $cxx --version 2>&1
                if ($LASTEXITCODE -eq 0) { $clang = (Get-Item -LiteralPath $candidate).FullName; break }
            } catch { }
        }
        if (-not $clang) { throw 'Working x64 clang.exe and clang++.exe are required. Add C++ Clang Compiler for Windows in Visual Studio Installer, or install LLVM on PATH.' }
        $directories += Split-Path -Parent $clang
    }
    # Child PowerShell/Python processes inherit these tools; system/user PATH is unchanged.
    $env:PATH = ($directories -join ';') + ';' + $env:PATH
    Write-Host "Using Python: $python"
    if ($clang) { Write-Host "Using Clang: $clang" }
    return [pscustomobject]@{ PythonPath=$python; ClangPath=$clang }
}
