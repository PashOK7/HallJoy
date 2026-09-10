@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build_fixed_plugin.ps1" %*
exit /b %errorlevel%
