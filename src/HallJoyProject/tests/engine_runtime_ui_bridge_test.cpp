// Engine/UI bridge: a timed-out request must never wedge later requests, and a
// RestoreInput that finishes after its waiter gave up is undone.
#include "../HallJoy/engine_runtime_ui_bridge.cpp"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

namespace bridge = halljoy::engine_runtime::ui_bridge;

namespace {
constexpr UINT kMessage = WM_APP + 77;
std::atomic<int> g_restores{0}, g_releases{0};
std::atomic<DWORD> g_handlerDelayMs{0};

bool Handler(bridge::Operation operation, std::uint32_t& error) noexcept
{
    if (const DWORD delay = g_handlerDelayMs.load()) Sleep(delay);
    if (operation == bridge::Operation::RestoreInput) ++g_restores;
    if (operation == bridge::Operation::ReleaseInput) ++g_releases;
    error = ERROR_SUCCESS;
    return true;
}

LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    if (message == kMessage) { (void)bridge::Dispatch(static_cast<std::uintptr_t>(w)); return 0; }
    return DefWindowProcW(hwnd, message, w, l);
}

void Pump(DWORD ms)
{
    const ULONGLONG end = GetTickCount64() + ms;
    do {
        MSG m{};
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&m);
        Sleep(1);
    } while (GetTickCount64() < end);
}

// Runs Execute on a worker ("engine") thread while this ("UI") thread pumps.
struct Call {
    std::atomic<bool> done{false};
    bool result = false;
    std::uint32_t error = 0;
    std::thread worker;
    explicit Call(bridge::Operation operation)
        : worker([this, operation] { result = bridge::Execute(operation, error); done = true; }) {}
    void Finish(DWORD pumpMs)
    {
        const ULONGLONG end = GetTickCount64() + pumpMs;
        while (!done && GetTickCount64() < end) Pump(5);
        worker.join();
    }
};
}

int main()
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = Proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"HallJoyBridgeTest";
    assert(RegisterClassW(&wc));
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    assert(window);
    assert(bridge::Start(window, kMessage, Handler));
    bridge::SetTimeoutForTesting(150);

    // 1. Normal request.
    {
        Call call(bridge::Operation::RestoreInput);
        call.Finish(2000);
        assert(call.result && call.error == ERROR_SUCCESS && g_restores == 1);
    }

    // 2. UI thread busy past the timeout: the request is dropped, never runs
    // late, and the next request works (before the fix: ERROR_BUSY forever).
    {
        Call late(bridge::Operation::RestoreInput);
        while (!late.done) Sleep(5); // UI thread does not pump meanwhile
        late.worker.join();
        assert(!late.result && late.error == ERROR_TIMEOUT);
        Pump(50); // the stale message is dispatched now and must be ignored
        assert(g_restores == 1);
        Call next(bridge::Operation::RestoreInput);
        next.Finish(2000);
        assert(next.result && g_restores == 2);
    }

    // 3. Handler still running at the timeout: the bridge stays busy until it
    // ends, then the late restore is undone by a release.
    {
        g_handlerDelayMs = 400;
        Call slow(bridge::Operation::RestoreInput);
        std::thread probe([] {
            Sleep(250); // after the 150 ms timeout, before the handler ends
            std::uint32_t error = 0;
            const bool ok = bridge::Execute(bridge::Operation::ReleaseInput, error);
            assert(!ok && error == ERROR_BUSY);
        });
        slow.Finish(3000); // pumps: runs the 400 ms handler on this thread
        probe.join();
        assert(!slow.result && slow.error == ERROR_TIMEOUT);
        assert(g_restores == 3 && g_releases == 1); // late restore undone
        g_handlerDelayMs = 0;
        Call next(bridge::Operation::RestoreInput);
        next.Finish(2000);
        assert(next.result && g_restores == 4);
    }

    // 4. Shutdown cancellation still wakes a waiting engine at once.
    {
        bridge::SetTimeoutForTesting(5000);
        Call waiting(bridge::Operation::ReleaseInput);
        Sleep(50);
        bridge::CancelPending();
        while (!waiting.done) Sleep(5);
        waiting.worker.join();
        assert(!waiting.result);
        Pump(20);
        assert(bridge::Stop());
    }
    DestroyWindow(window);
    std::puts("ENGINE_RUNTIME_UI_BRIDGE_TEST=PASS stale_request late_restore_undone busy_while_running cancel");
    return 0;
}
