#include "block_keys_hotkey.h"
#include "keyboard_hook_thread.h"
#include <cassert>
#include <thread>
#include <future>
static HWND workerWindow;
static HANDLE workerDone;
static DWORD workerId;
static LRESULT CALLBACK PassHook(int code, WPARAM w, LPARAM l) { return CallNextHookEx(nullptr,code,w,l); }
static LRESULT CALLBACK WorkerProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_APP) { SetEvent(workerDone); return 0; }
    return DefWindowProcW(hwnd,msg,w,l);
}
static void InitializeWorker() {
    workerId = GetCurrentThreadId();
    WNDCLASSW cls{}; cls.lpfnWndProc = WorkerProc;
    cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"HallJoyHookPumpTest";
    RegisterClassW(&cls);
    workerWindow = CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);
}
int main() {
    using namespace halljoy::block_keys;
    workerDone = CreateEventW(nullptr,TRUE,FALSE,nullptr);
    KeyboardHookThread worker;
    assert(worker.Start(PassHook,InitializeWorker) == ERROR_SUCCESS);
    assert(workerWindow && workerId != GetCurrentThreadId());
    assert(PostMessageW(workerWindow,WM_APP,0,0));
    // Calling/UI thread does NOT pump messages while waiting, like a blocked
    // settings save. The real hook thread must continue dispatching independently.
    assert(WaitForSingleObject(workerDone,1000) == WAIT_OBJECT_0);
    worker.Stop(); worker.Stop();
    assert(!IsWindow(workerWindow));
    CloseHandle(workerDone);
    HWND window = CreateWindowExW(0, L"STATIC", L"HallJoy hotkey test", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(window);
    HotkeyRegistration shortcut;
    unsigned chosen = 0;
    for (unsigned key = VK_F13; key <= VK_F24; ++key) {
        unsigned candidate = ((MOD_CONTROL | MOD_ALT | MOD_SHIFT) << 8) | key;
        if (!shortcut.Apply(window, candidate)) { chosen = candidate; break; }
    }
    assert(chosen);
    assert(shortcut.Chord() == chosen);
    assert(!shortcut.Apply(window, chosen));
    assert(shortcut.Apply(window, 16) == ERROR_INVALID_PARAMETER);
    std::promise<unsigned> ready;
    std::promise<void> release;
    auto released = release.get_future();
    std::thread competitor([&] {
        unsigned occupied = 0;
        for (unsigned key = VK_F13; key <= VK_F24; ++key) {
            unsigned candidate = ((MOD_CONTROL | MOD_ALT | MOD_SHIFT) << 8) | key;
            if (candidate != chosen && RegisterHotKey(nullptr, 1, (candidate >> 8) | MOD_NOREPEAT, key)) {
                occupied = candidate; break;
            }
        }
        ready.set_value(occupied);
        released.wait();
        if (occupied) UnregisterHotKey(nullptr, 1);
    });
    unsigned occupied = ready.get_future().get();
    assert(occupied);
    assert(shortcut.Apply(window, occupied) != ERROR_SUCCESS);
    assert(shortcut.Chord() == chosen);
    assert(shortcut.Matches(0x4b11, MAKELPARAM(chosen >> 8, chosen & 255)));
    assert(!shortcut.Matches(0x4b10, MAKELPARAM(chosen >> 8, chosen & 255)));
    release.set_value(); competitor.join();
    assert(!shortcut.Apply(window, occupied));
    assert(shortcut.Chord() == occupied);
    assert(!shortcut.Apply(window, 0));
    assert(!shortcut.Chord());
    assert(RegisterHotKey(window, 1, occupied >> 8, occupied & 255));
    UnregisterHotKey(window, 1);
    shortcut.Stop(); DestroyWindow(window);
}
