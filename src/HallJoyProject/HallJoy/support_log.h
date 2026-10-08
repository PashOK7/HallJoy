#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <string>

// Directory overrides isolate writer tests; production uses the app data root
// and mirrors continuous logs beside the executable.
// Structural events only. Never pass key codes, input values, paths or serials.
bool SupportLog_Start(const wchar_t* directoryOverride = nullptr, const wchar_t* mirrorOverride = nullptr) noexcept;
bool SupportLog_Stop() noexcept;
enum class SupportLogDetailKind : unsigned char { None, Data, Win32, Protocol };
struct SupportLogDetail { std::uint64_t value=0; SupportLogDetailKind kind=SupportLogDetailKind::None; };
constexpr SupportLogDetail SupportLog_Data(std::uint64_t v) { return {v,SupportLogDetailKind::Data}; }
constexpr SupportLogDetail SupportLog_Win32(std::uint64_t v) { return {v,SupportLogDetailKind::Win32}; }
constexpr SupportLogDetail SupportLog_Protocol(std::uint64_t v) { return {v,SupportLogDetailKind::Protocol}; }
// An untyped third integer is deliberately a compile error. Metadata is not an error.
void SupportLog_Event(const char* category, std::uint64_t value, SupportLogDetail detail = {}) noexcept;
void SupportLog_OverlaySummary(const wchar_t* aggregate) noexcept;
void SupportLog_InventoryChanged() noexcept;
void SupportLog_ReportMissingSource() noexcept;
void SupportLog_ReportFailure(const char* category, std::uint64_t error) noexcept;
void SupportLog_SetWindow(HWND window) noexcept;
DWORD SupportLog_LastError() noexcept;
std::uint64_t SupportLog_RequestSnapshot() noexcept;
std::uint64_t SupportLog_CompletedSnapshot() noexcept;
// Fills an aggregate "key=value ..." summary of the input configuration (counts
// only); written as `input.config` with every snapshot. Null disables it.
using SupportLogInputConfigProvider = void (*)(char* text, std::size_t capacity) noexcept;
void SupportLog_SetInputConfigProvider(SupportLogInputConfigProvider provider) noexcept;
// Input-chain trace (input_trace.h): a record starting with "trace." about keys
// bound to the gamepad and the resulting gamepad state. Kept in its own bounded
// window of the newest 4000 lines; the header says bound_key_trace=1.
void SupportLog_Trace(const char* record) noexcept;
// Format capture for decoding an unknown device protocol (owner rule
// 2026-10-06: the log must carry the data). A record starts with "capture."
// and is stored at once, without the timed queue, in its own store of the
// first 8000 records; every written report contains the whole store. Bounded
// by content only. False when the store is full or the record is invalid.
bool SupportLog_Capture(const char* record) noexcept;
// Device evidence for an unconfirmed keyboard. A record starts with "evidence.".
// keep=true: stored whole (finite probe results, identity, key records).
// keep=false: stream records; the report holds the first 2000 and the newest
// 8000 with an explicit omitted count between them. Written in every full
// report (Open log). Bounded by content, never by time.
void SupportLog_Evidence(const char* record, bool keep) noexcept;
std::wstring SupportLog_Directory();

// Private RedSquare research: reviewed code-window bytes or aggregate stream
// counters only. Never raw input, per-key values, serials, paths or arbitrary RAM.
// Background producer only. False means queue/full or invalid record; abort capture.
bool SupportLog_RedSquareResearch(const char* record) noexcept;
