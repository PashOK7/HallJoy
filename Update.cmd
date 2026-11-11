@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\update_portable.ps1" %*
set "result=%errorlevel%"
pause
exit /b %result%
