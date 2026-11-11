@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build_windows_installer.ps1" %*
set "result=%errorlevel%"
if not "%result%"=="0" echo Build failed. See the error above.
if "%~1"=="" pause
exit /b %result%
