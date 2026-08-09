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

// Installs crash-only reporting. Production does not start the asynchronous
// logger or create files during normal operation; HallJoyCrash.txt is written
// synchronously only for an unhandled process exception. Diagnostic builds add
// their richer first-chance report and checkpoints.
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

#if defined(NDEBUG) && !defined(HALLJOY_DIAGNOSTIC) && \
    !defined(HALLJOY_ANALOG_SIMULATOR) && !defined(HALLJOY_DEBUG_LOG_IMPLEMENTATION)
// Ordinary production logging is absent at the call site: arguments are not
// evaluated and no hot-path function call survives compilation. Shutdown,
// crash reporting and the native emergency watchdog remain real functions.
#define DebugLog_Init() ((void)0)
#define DebugLog_Write(...) do { if constexpr (false) DebugLog_Write(__VA_ARGS__); } while (false)
#define DebugLog_WriteBuffered(...) do { if constexpr (false) DebugLog_WriteBuffered(__VA_ARGS__); } while (false)
#define DebugLog_SetCheckpoint(...) do { if constexpr (false) DebugLog_SetCheckpoint(__VA_ARGS__); } while (false)
#define DebugLog_IsDiagnosticBuild() false
#define DebugLog_LogAnalogModules() ((void)0)
#endif
