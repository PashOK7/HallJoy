@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build_windows_installer.ps1" %*
set "EXITCODE=%ERRORLEVEL%"

if not "%EXITCODE%"=="0" (
  echo.
  echo Build failed with exit code %EXITCODE%.
  if "%~1"=="" pause
  exit /b %EXITCODE%
)

echo.
echo Installer build command completed.
if "%~1"=="" pause
exit /b 0
