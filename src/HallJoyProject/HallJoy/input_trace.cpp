#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "input_trace.h"

#include "analog_key_codes.h"
#include "bindings.h"
#include "key_settings.h"
#include "settings.h"
#include "support_log.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
constexpr int kPads = BINDINGS_MAX_GAMEPADS;
constexpr ULONGLONG kPadIntervalMs = 16, kBindingsCheckMs = 1000, kBindingsRepeatMs = 10000;
constexpr unsigned kEventsPerSecond = 300;
constexpr const char* kButtonNames[] = {"a", "b", "x", "y", "lb", "rb", "back", "start", "guide",
                                        "ls", "rs", "up", "down", "left", "right"};

// Shared budget for event producers (several threads): newest second only.
std::atomic<ULONGLONG> g_budgetSecond{0};
std::atomic<unsigned> g_budgetUsed{0};

// Format capture: events per key code (UI thread only after enabling).
constexpr unsigned kCaptureEventsPerKey = 4;
std::atomic<bool> g_keyCapture{false};
std::array<std::uint8_t, 0x500> g_captureCount{};
bool TakeBudget() noexcept {
    const ULONGLONG second = GetTickCount64() / 1000;
    ULONGLONG seen = g_budgetSecond.load(std::memory_order_relaxed);
    if (seen != second && g_budgetSecond.compare_exchange_strong(seen, second)) g_budgetUsed.store(0);
    return g_budgetUsed.fetch_add(1, std::memory_order_relaxed) < kEventsPerSecond;
}

int Pads() noexcept { return std::clamp(Settings_GetVirtualGamepadCount(), 1, kPads); }
unsigned Milli(float v) noexcept {
    if (!(v > 0.0f)) return 0;
    return v >= 1.0f ? 1000u : static_cast<unsigned>(std::lround(v * 1000.0f));
}

// Appends "%x" plus curve flags: '!' inverted, '*' own per-key curve.
void AppendKey(char*& p, char* end, std::uint16_t hid, bool globalInvert) {
    if (p >= end) return;
    if (!hid) { p += std::max(0, snprintf(p, end - p, "-")); return; }
    bool unique = KeySettings_GetUseUnique(hid), inverted = globalInvert;
    if (unique) inverted = KeySettings_Get(hid).invert;
    p += std::max(0, snprintf(p, end - p, "%x%s%s", hid, inverted ? "!" : "", unique ? "*" : ""));
}

std::size_t BindingsText(char* text, std::size_t capacity) {
    char* p = text;
    char* const end = text + capacity;
    const bool invert = Settings_GetInputInvert();
    p += std::max(0, snprintf(p, end - p, "trace.bindings pads=%d global_invert=%d snappy=%d last_key=%d",
        Pads(), invert ? 1 : 0, Settings_GetSnappyJoystick() ? 1 : 0, Settings_GetLastKeyPriority() ? 1 : 0));
    static constexpr const char* axes[] = {"lx", "ly", "rx", "ry"};
    for (int pad = 0; pad < Pads() && p < end; ++pad) {
        p += std::max(0, snprintf(p, end - p, " | p=%d", pad));
        for (int a = 0; a < 4 && p < end; ++a) {
            const AxisBinding b = Bindings_GetAxisForPad(pad, static_cast<Axis>(a));
            p += std::max(0, snprintf(p, end - p, " %s=", axes[a]));
            AppendKey(p, end, b.minusHid, invert);
            if (p < end) *p++ = '/';
            AppendKey(p, end, b.plusHid, invert);
        }
        for (int t = 0; t < 2 && p < end; ++t) {
            p += std::max(0, snprintf(p, end - p, " %s=", t ? "rt" : "lt"));
            AppendKey(p, end, Bindings_GetTriggerForPad(pad, static_cast<Trigger>(t)), invert);
        }
        for (int button = 0; button < 15 && p < end; ++button) {
            bool first = true;
            for (int chunk = 0; chunk < Bindings_GetButtonMaskChunkCount(); ++chunk) {
                const std::uint64_t mask = Bindings_GetButtonMaskChunkForPad(pad, static_cast<GameButton>(button), chunk);
                for (unsigned bit = 0; bit < 64 && p < end; ++bit) {
                    if (!(mask >> bit & 1u)) continue;
                    p += std::max(0, snprintf(p, end - p, first ? " %s=" : ",", kButtonNames[button]));
                    first = false;
                    AppendKey(p, end, static_cast<std::uint16_t>(chunk * 64 + bit), invert);
                }
            }
        }
    }
    if (p >= end) { p = end - 1; }
    *p = 0;
    return static_cast<std::size_t>(p - text);
}

struct PadState {
    InputTracePad last{};
    std::array<std::uint16_t, 10> lastAnalog{};
    ULONGLONG lastEmit = 0;
    bool pending = false, emitted = false;
};
std::array<PadState, kPads> g_pads{};
ULONGLONG g_bindingsChecked = 0, g_bindingsEmitted = 0;
char g_lastBindings[1024]{};

bool Same(const InputTracePad& a, const InputTracePad& b) noexcept {
    return a.lx == b.lx && a.ly == b.ly && a.rx == b.rx && a.ry == b.ry && a.lt == b.lt && a.rt == b.rt &&
           a.buttons == b.buttons;
}
} // namespace

bool InputTrace_IsBound(std::uint16_t hid) noexcept {
    if (!hid) return false;
    for (int pad = 0; pad < Pads(); ++pad)
        if (Bindings_IsHidBoundForPad(pad, hid)) return true;
    return false;
}

void InputTrace_Pad(int pad, const InputTracePad& out, const float* raw, const float* filtered,
                    std::size_t count, InputTraceNative native, const void* nativeContext) noexcept {
    if (pad < 0 || pad >= kPads || !raw || !filtered) return;
    try {
        const ULONGLONG now = GetTickCount64();
        if (pad == 0 && now - g_bindingsChecked >= kBindingsCheckMs) {
            g_bindingsChecked = now;
            char text[1024]{};
            BindingsText(text, sizeof(text));
            if (strcmp(text, g_lastBindings) != 0 || now - g_bindingsEmitted >= kBindingsRepeatMs) {
                strcpy_s(g_lastBindings, text);
                g_bindingsEmitted = now;
                SupportLog_Trace(text);
            }
        }
        // Analog targets: axis directions and triggers.
        std::array<std::uint16_t, 10> keys{};
        for (int a = 0; a < 4; ++a) {
            const AxisBinding b = Bindings_GetAxisForPad(pad, static_cast<Axis>(a));
            keys[a * 2] = b.minusHid; keys[a * 2 + 1] = b.plusHid;
        }
        keys[8] = Bindings_GetTriggerForPad(pad, Trigger::LT);
        keys[9] = Bindings_GetTriggerForPad(pad, Trigger::RT);
        std::array<std::uint16_t, 10> analog{};
        for (std::size_t i = 0; i < keys.size(); ++i)
            if (keys[i] && keys[i] < count) analog[i] = static_cast<std::uint16_t>(Milli(raw[keys[i]]));
        auto& s = g_pads[static_cast<std::size_t>(pad)];
        if (!s.emitted || !Same(out, s.last) || analog != s.lastAnalog) s.pending = true;
        s.last = out; s.lastAnalog = analog;
        if (!s.pending || now - s.lastEmit < kPadIntervalMs) return;
        s.pending = false; s.emitted = true; s.lastEmit = now;

        char line[1000]{};
        char* p = line;
        char* const end = line + sizeof(line) - 1;
        p += std::max(0, snprintf(p, end - p, "trace.pad p=%d lx=%d ly=%d rx=%d ry=%d lt=%u rt=%u buttons=%04x keys",
            pad, out.lx, out.ly, out.rx, out.ry, out.lt, out.rt, out.buttons));
        for (const auto hid : keys)
            if (hid && hid < count && p < end) {
                p += std::max(0, snprintf(p, end - p, " %x=%u/%u", hid, Milli(raw[hid]), Milli(filtered[hid])));
                const int own = native ? native(nativeContext, hid) : -1;
                if (own >= 0 && p < end) p += std::max(0, snprintf(p, end - p, "/n%d", own));
            }
        // Button keys only while they carry a value.
        for (int button = 0; button < 15 && p < end; ++button)
            for (int chunk = 0; chunk < Bindings_GetButtonMaskChunkCount(); ++chunk) {
                const std::uint64_t mask = Bindings_GetButtonMaskChunkForPad(pad, static_cast<GameButton>(button), chunk);
                for (unsigned bit = 0; bit < 64 && p < end; ++bit) {
                    const std::size_t hid = static_cast<std::size_t>(chunk) * 64u + bit;
                    if ((mask >> bit & 1u) && hid < count && (raw[hid] > 0.0f || filtered[hid] > 0.0f))
                        p += std::max(0, snprintf(p, end - p, " %s:%zx=%u/%u", kButtonNames[button], hid,
                            Milli(raw[hid]), Milli(filtered[hid])));
                }
            }
        *std::min(p, end) = 0;
        SupportLog_Trace(line);
    } catch (...) {
    }
}

void InputTrace_SetKeyCapture(bool on) noexcept { g_keyCapture.store(on); }

void InputTrace_OsKey(std::uintptr_t device, std::uint16_t hid, bool down) noexcept {
    const auto mixed = static_cast<std::uint64_t>(device) * 0x9E3779B97F4A7C15ull;
    if (g_keyCapture.load() && hid && hid < g_captureCount.size() && g_captureCount[hid] < kCaptureEventsPerKey) {
        ++g_captureCount[hid];
        char line[96]{};
        snprintf(line, sizeof(line), "capture.key hid=%x down=%d keyboard=%04x", hid, down ? 1 : 0,
                 static_cast<unsigned>(mixed >> 48));
        SupportLog_Capture(line);
    }
    if (!InputTrace_IsBound(hid) || !TakeBudget()) return;
    char line[96]{};
    snprintf(line, sizeof(line), "trace.os_key hid=%x down=%d keyboard=%04x", hid, down ? 1 : 0,
             static_cast<unsigned>(mixed >> 48));
    SupportLog_Trace(line);
}

void InputTrace_Source(const char* provider, unsigned row, unsigned column, unsigned travel,
                       unsigned milli, std::uint16_t hid) noexcept {
    if (!InputTrace_IsBound(hid) || !TakeBudget()) return;
    char line[128]{};
    snprintf(line, sizeof(line), "trace.src %s row=%u col=%u travel=%u milli=%u hid=%x",
             provider ? provider : "?", row, column, travel, milli, hid);
    SupportLog_Trace(line);
}
