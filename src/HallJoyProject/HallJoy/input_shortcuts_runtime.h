#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <array>
#include <mutex>
#include "input_shortcuts.h"
#include "settings.h"
#include "bindings.h"

// Process-wide owner of the shortcut Engine. Digital events arrive on the
// keyboard-hook thread, analog samples on the realtime thread; both share one
// small lock. The realtime side never waits for it (try_lock), the hook side
// holds it only for the in-memory update.
namespace halljoy::shortcuts {

inline std::mutex mutex;
inline Engine engine;

// Installed by the application: whether an action may run now, and what to do
// when it fires. Both are called outside the engine lock.
inline std::atomic<bool (*)(Action)> applicable{nullptr};
inline std::atomic<void (*)(Action)> dispatch{nullptr};

// Window receiving kCaptureMessage (lParam = packed shortcut or
// kCaptureCancelled) when an explicit capture completes.
constexpr UINT kCaptureMessage = WM_APP + 391;
inline std::atomic<HWND> captureWindow{nullptr};

// Game profile shortcuts, published by the profile service on the UI thread:
// [0] next profile, [1] return to automatic, [2..] profile slots.
inline std::array<std::atomic<unsigned>, 2 + kProfileSlots> profileBindings{};
inline void SetProfileBindings(unsigned next, unsigned automatic, const unsigned (&slots)[kProfileSlots]) noexcept {
    profileBindings[0].store(next);
    profileBindings[1].store(automatic);
    for (unsigned i = 0; i < kProfileSlots; ++i) profileBindings[2 + i].store(slots[i]);
}

inline Bindings CurrentBindings() noexcept {
    Bindings b{};
    for (unsigned i = 0; i < profileBindings.size(); ++i) b[4 + i] = profileBindings[i].load();
    b[0] = Settings_GetBlockKeysHotkey();
    if (Settings_GetPauseSeparate()) {
        b[2] = Settings_GetPauseShortcut(1);
        b[3] = Settings_GetPauseShortcut(2);
    } else {
        b[1] = Settings_GetPauseShortcut(0);
    }
    return b;
}

inline bool AnyAssigned() noexcept {
    for (unsigned s : CurrentBindings()) if (s) return true;
    return false;
}

namespace detail {
struct Fired { Action actions[4]{}; unsigned count = 0; };
inline bool Applicable(Action a) noexcept { auto f = applicable.load(); return f && f(a); }
inline bool IsBound(unsigned hid) noexcept { return hid < 256 && Bindings_IsHidBound(static_cast<std::uint16_t>(hid)); }
inline void Finish(const Fired& fired, unsigned captureResult) noexcept {
    if (auto f = dispatch.load()) for (unsigned i = 0; i < fired.count; ++i) f(fired.actions[i]);
    if (captureResult) if (HWND w = captureWindow.load()) PostMessageW(w, kCaptureMessage, 0, static_cast<LPARAM>(captureResult));
}
}

// Keyboard hook: returns true when the digital event must be swallowed.
inline bool Digital(unsigned hid, bool down) noexcept {
    const Bindings bindings = CurrentBindings();
    detail::Fired fired;
    bool swallow = false;
    unsigned captureResult = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        swallow = engine.Digital(hid, down, bindings, detail::Applicable, detail::IsBound,
            [&](Action a) { if (fired.count < 4) fired.actions[fired.count++] = a; });
        captureResult = engine.TakeCapture();
    }
    detail::Finish(fired, captureResult);
    return swallow;
}

// Realtime path: samples only the keys the current bindings (or an active
// capture) need. Never blocks behind the hook thread.
template <class ReadMilli>
inline void Analog(ReadMilli&& readMilli) noexcept {
    const Bindings bindings = CurrentBindings();
    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    detail::Fired fired;
    engine.ForEachAnalogKey(bindings, [&](unsigned hid) {
        engine.Analog(hid, static_cast<unsigned>(readMilli(hid)), bindings, detail::Applicable, detail::IsBound,
            [&](Action a) { if (fired.count < 4) fired.actions[fired.count++] = a; });
    });
    const unsigned captureResult = engine.TakeCapture();
    lock.unlock();
    detail::Finish(fired, captureResult);
}

// True when analog depth is needed even if the UI is hidden.
inline bool NeedsAnalog() noexcept {
    if (AnyAssigned()) return true;
    std::lock_guard<std::mutex> lock(mutex);
    return engine.Capturing();
}

inline void SeedDigital(unsigned hid) noexcept { std::lock_guard<std::mutex> lock(mutex); engine.SeedDigital(hid); }
inline void ResetAnalog() noexcept { std::lock_guard<std::mutex> lock(mutex); engine.ResetAnalog(); }

inline void BeginCapture(HWND owner) noexcept {
    std::lock_guard<std::mutex> lock(mutex);
    captureWindow.store(owner);
    engine.BeginCapture();
}
inline void CancelCapture() noexcept {
    std::lock_guard<std::mutex> lock(mutex);
    engine.CancelCapture();
    (void)engine.TakeCapture();
    captureWindow.store(nullptr);
}
inline bool Capturing() noexcept { std::lock_guard<std::mutex> lock(mutex); return engine.Capturing(); }

} // namespace halljoy::shortcuts
