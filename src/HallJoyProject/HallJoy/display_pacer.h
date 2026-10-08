#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <memory>

// Frame pacing for UI animations at the real refresh rate of the monitor the
// animation is on (mixed-refresh multi-monitor setups included). A worker
// thread waits for that monitor's vertical blank (DXGI WaitForVBlank) and
// posts one frame message to the target window; at most one message is queued
// at a time. SetTimer cannot do this: in practice it ticks at ~64-100 Hz.
namespace halljoy::display_pacer {

// Seconds from a high-resolution clock (QueryPerformanceCounter).
double NowSeconds();

class Pacer {
public:
    Pacer() = default;
    ~Pacer() { Stop(); }
    Pacer(const Pacer&) = delete;
    Pacer& operator=(const Pacer&) = delete;

    // Starts posting `message` to `target` once per refresh (no-op if running).
    bool Start(HWND target, UINT message, HMONITOR monitor);
    // Never blocks: the worker exits by itself after its current wait. A frame
    // message already in flight may still arrive once after Stop.
    void Stop();
    bool Running() const { return shared_ != nullptr; }
    // The monitor whose refresh drives the frames; follow the animated object.
    void SetMonitor(HMONITOR monitor) { if (shared_) shared_->monitor.store(monitor); }
    // Call when handling the frame message: allows the next one to be posted.
    void FrameConsumed() { if (shared_) shared_->posted.store(false); }
    // Time (NowSeconds) of the refresh that produced the latest frame message;
    // evenly spaced, unlike the time the message happens to be handled.
    double FrameTime() const { return shared_ ? shared_->frameTime.load() : 0.0; }

private:
    struct Shared {
        HWND target = nullptr;
        UINT message = 0;
        std::atomic<bool> stop{ false };
        std::atomic<bool> posted{ false };
        std::atomic<HMONITOR> monitor{ nullptr };
        std::atomic<double> frameTime{ 0.0 };
    };
    static unsigned __stdcall Run(void* parameter);
    std::shared_ptr<Shared> shared_;
};

} // namespace halljoy::display_pacer
