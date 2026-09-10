#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

// Structural events only. Never pass key codes, input values, paths or serials.
bool SupportLog_Start(const wchar_t* directoryOverride = nullptr) noexcept;
bool SupportLog_Stop() noexcept;
void SupportLog_Event(const char* category, std::uint64_t value, std::uint64_t error = 0) noexcept;
void SupportLog_OverlaySummary(const wchar_t* aggregate) noexcept;
void SupportLog_InventoryChanged() noexcept;
void SupportLog_ReportMissingSource() noexcept;
void SupportLog_ReportFailure(const char* category, std::uint64_t error) noexcept;
void SupportLog_SetWindow(HWND window) noexcept;
DWORD SupportLog_LastError() noexcept;
std::wstring SupportLog_Directory();
