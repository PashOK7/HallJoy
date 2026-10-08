#pragma once
#include <cstddef>
#include <cstdint>

// Input-chain trace for the ordinary support log (SupportLog_Trace), so a
// tester's HallJoy.log shows where a wrong gamepad state comes from:
//   trace.src      provider event (matrix position, travel) of a bound key
//   trace.os_key   Windows key down/up of a bound key (physical reference)
//   trace.pad      gamepad output with raw/filtered(/native) values of its keys
//   trace.bindings binding table with curve flags (on change, every 10 s)
// Only keys bound to the gamepad are recorded, never other keys or text.
// Producers are rate limited; the log keeps the newest 4000 trace lines.

struct InputTracePad {
    short lx = 0, ly = 0, rx = 0, ry = 0;
    std::uint8_t lt = 0, rt = 0;
    std::uint16_t buttons = 0;
};

// Realtime thread: one call per pad per built report. `raw`/`filtered` are
// indexed by HallJoy key code (0..1 floats), `count` entries.
// `native` (optional) returns a native backend's own value for a key in
// permille, or -1 when no native backend owns it (the raw value then came from
// UAP/Wooting arbitration).
using InputTraceNative = int (*)(const void* context, std::uint16_t hid);
void InputTrace_Pad(int pad, const InputTracePad& out, const float* raw, const float* filtered,
                    std::size_t count, InputTraceNative native = nullptr,
                    const void* nativeContext = nullptr) noexcept;
// UI thread, Raw Input keyboard events (any keyboard; `device` is hashed).
void InputTrace_OsKey(std::uintptr_t device, std::uint16_t hid, bool down) noexcept;
// Provider threads: one native event of a key; ignored unless the key is bound.
void InputTrace_Source(const char* provider, unsigned row, unsigned column, unsigned travel,
                       unsigned milli, std::uint16_t hid) noexcept;
bool InputTrace_IsBound(std::uint16_t hid) noexcept;
// Format capture (SupportLog_Capture): while a native backend records an
// unknown protocol, every key's first two presses (down and up) are recorded as
// `capture.key`, from any keyboard, so the raw data can be matched to physical
// keys. Bounded per key, not by time; repeated presses of a key add nothing.
void InputTrace_SetKeyCapture(bool on) noexcept;
