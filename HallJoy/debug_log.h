#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Writes diagnostic log near executable as "log.txt".
// Intended for troubleshooting end-user environments.
void DebugLog_Init();
void DebugLog_Shutdown();
void DebugLog_Write(const wchar_t* fmt, ...);
const wchar_t* DebugLog_Path();
