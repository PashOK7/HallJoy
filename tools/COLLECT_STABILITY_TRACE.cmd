@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0collect_stability_trace.ps1"
if errorlevel 1 (
  echo.
  echo Trace collection failed.
) else (
  echo.
  echo Upload HallJoyStabilityTraceBundle.zip for analysis.
)
pause
