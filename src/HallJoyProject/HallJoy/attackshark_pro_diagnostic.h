#pragma once
#include <windows.h>
#if defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC) || defined(HALLJOY_ATTACKSHARK_NATIVE)
bool SharkDiagnostic_TryRunCommand(int& result) noexcept;
void SharkDiagnostic_Start() noexcept;
void SharkDiagnostic_Stop() noexcept;
void SharkDiagnostic_ObserveRawInput(HRAWINPUT input) noexcept;
void SharkDiagnostic_DeviceChanged(HANDLE device) noexcept;
void SharkDiagnostic_UpdateWindow(HWND window) noexcept;
#else
inline bool SharkDiagnostic_TryRunCommand(int&) noexcept{return false;}
inline void SharkDiagnostic_Start() noexcept{}
inline void SharkDiagnostic_Stop() noexcept{}
inline void SharkDiagnostic_ObserveRawInput(HRAWINPUT) noexcept{}
inline void SharkDiagnostic_DeviceChanged(HANDLE) noexcept{}
inline void SharkDiagnostic_UpdateWindow(HWND) noexcept{}
#endif
