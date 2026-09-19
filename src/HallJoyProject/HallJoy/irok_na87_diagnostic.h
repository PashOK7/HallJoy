#pragma once
#include <windows.h>
#if defined(HALLJOY_IROK_NA87_DIAGNOSTIC)
bool Na87Diagnostic_TryRunCommand(int& result) noexcept;
void Na87Diagnostic_Start() noexcept;
void Na87Diagnostic_Stop() noexcept;
void Na87Diagnostic_UpdateWindow(HWND window) noexcept;
void Na87Diagnostic_ObserveRawInput(HRAWINPUT input) noexcept;
#else
inline bool Na87Diagnostic_TryRunCommand(int&) noexcept { return false; }
inline void Na87Diagnostic_Start() noexcept {}
inline void Na87Diagnostic_Stop() noexcept {}
inline void Na87Diagnostic_UpdateWindow(HWND) noexcept {}
inline void Na87Diagnostic_ObserveRawInput(HRAWINPUT) noexcept {}
#endif
