#pragma once
#include <windows.h>
#if defined(HALLJOY_AULA_MINI60_DIAGNOSTIC) || defined(HALLJOY_AULA_MINI60_NATIVE)
bool Mini60Diagnostic_TryRunCommand(int& result) noexcept;
void Mini60Diagnostic_Start() noexcept;
void Mini60Diagnostic_Stop() noexcept;
void Mini60Diagnostic_UpdateWindow(HWND window) noexcept;
void Mini60Diagnostic_ObserveRawInput(HRAWINPUT input) noexcept;
void Mini60Diagnostic_DeviceChanged(HANDLE device) noexcept;
#else
inline bool Mini60Diagnostic_TryRunCommand(int&) noexcept { return false; }
inline void Mini60Diagnostic_Start() noexcept {}
inline void Mini60Diagnostic_Stop() noexcept {}
inline void Mini60Diagnostic_UpdateWindow(HWND) noexcept {}
inline void Mini60Diagnostic_ObserveRawInput(HRAWINPUT) noexcept {}
inline void Mini60Diagnostic_DeviceChanged(HANDLE) noexcept {}
#endif
