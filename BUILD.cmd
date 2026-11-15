@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1" %*
set "EXITCODE=%ERRORLEVEL%"

if not "%EXITCODE%"=="0" (
  echo.
  echo Build failed with exit code %EXITCODE%.
  if "%~1"=="" pause
  exit /b %EXITCODE%
)

if not "%~1"=="" exit /b 0

echo.
echo Build complete.
echo Output: build\bin\Release\x64\HallJoy.exe
pause
exit /b 0
