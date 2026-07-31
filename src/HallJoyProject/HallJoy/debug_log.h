#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "worker_lifecycle.h"

// Writes diagnostic log near the executable. Normal lines are queued to a
// background writer; fatal handlers use separate synchronous crash reports.
void DebugLog_Init();
halljoy::lifecycle::StopResult DebugLog_Shutdown();
void DebugLog_Write(const wchar_t* fmt, ...);
// Explicit asynchronous alias retained for latency/high-rate call sites.
void DebugLog_WriteBuffered(const wchar_t* fmt, ...);
const wchar_t* DebugLog_Path();

// Diagnostic builds keep a privacy-safe crash breadcrumb separately from the
// normal asynchronous logger. Release builds compile these calls to no-ops.
void DebugLog_InstallCrashHandler();
void DebugLog_SetCheckpoint(const wchar_t* fmt, ...);
bool DebugLog_IsDiagnosticBuild();

// Diagnostic-only out-of-process exit capture. The watchdog records the final
// process exit code even for abort/fast-fail/TerminateProcess cases that bypass
// SetUnhandledExceptionFilter.
bool DebugLog_TryRunExitWatchdogCommand();
void DebugLog_StartExitWatchdog();

// Logs privacy-safe basenames and image metadata for loaded analog modules.
void DebugLog_LogAnalogModules();
