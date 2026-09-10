#include "engine_runtime_owner.h"
#include <windows.h>
#include <atomic>
#include <iostream>
#include <stdexcept>

using namespace halljoy::engine_runtime;
using halljoy::runtime_command::State;
struct Context { DWORD uiThread; std::atomic<unsigned> events{0}; std::atomic<bool> fail{false}; };
static bool Step(void*, std::uint32_t& error) noexcept { error = 0; return true; }
static bool Release(void* context, std::uint32_t& error) noexcept {
    if (static_cast<Context*>(context)->fail) { error = ERROR_BUSY; return false; }
    return Step(context, error);
}
static void Changed(void* context) noexcept {
    auto& c = *static_cast<Context*>(context);
    ++c.events;
    PostThreadMessageW(c.uiThread, WM_APP + 362, 0, 0);
}
static void Check(bool ok) { if (!ok) throw std::runtime_error("owner notification regression"); }
static void Await(State expected, Context& context, unsigned before) {
    const auto deadline = GetTickCount64() + 3000;
    bool notified = false;
    do {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, WM_APP + 362, WM_APP + 362, PM_REMOVE)) notified = true;
        if (notified && context.events > before && EngineRuntimeOwner_Snapshot().state == expected) return;
        Sleep(1);
    } while (GetTickCount64() < deadline);
    Check(false);
}
int main() {
    Context context{GetCurrentThreadId()}; // Outlives Stop even on assertion failure.
    try {
    MSG msg{};
    PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE); // Establish receiver queue.
    OperationsV1 ops{&context, Step, Step, Step, Step, Step, Step, Release,
        Step, Step, Step, Step, Step, Step, Step, Changed};
    Check(EngineRuntimeOwner_Start(ops));
    Check(EngineRuntimeOwner_RequestResume() == SubmitStatus::Queued);
    Await(State::Active, context, 0);
    auto before = context.events.load();
    Check(EngineRuntimeOwner_RequestResume() == SubmitStatus::NoChange);
    Sleep(30); Check(context.events == before); // No notifications while idle.
    Check(EngineRuntimeOwner_RequestPause() == SubmitStatus::Queued);
    Await(State::Paused, context, before);
    before = context.events.load();
    Check(EngineRuntimeOwner_RequestResume() == SubmitStatus::Queued);
    Await(State::Active, context, before);
    context.fail = true; before = context.events.load();
    Check(EngineRuntimeOwner_RequestPause() == SubmitStatus::Queued);
    Await(State::PauseFaulted, context, before);
    Check(EngineRuntimeOwner_Stop().RestartSafe());
    std::cout << "OWNER_STATE_NOTIFICATIONS=PASS active paused resumed faulted idle_quiet\n";
    return 0;
} catch (const std::exception& error) {
    (void)EngineRuntimeOwner_Stop();
    std::cerr << error.what() << '\n'; return 1;
}
}
