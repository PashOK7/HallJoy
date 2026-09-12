#pragma once
#include <windows.h>
namespace halljoy::block_keys {
// The hook's message pump must never share the UI's disk writes or rendering.
// Start/Stop are UI-owned; callback and initialize run exclusively on this thread.
class KeyboardHookThread {
    HANDLE thread_ = nullptr, ready_ = nullptr;
    DWORD id_ = 0, error_ = 0;
    HOOKPROC callback_ = nullptr;
    void (*initialize_)() = nullptr;
    static DWORD WINAPI Run(void* context) {
        auto& self = *static_cast<KeyboardHookThread*>(context);
        MSG message{};
        PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        const HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, self.callback_, GetModuleHandleW(nullptr), 0);
        self.error_ = hook ? ERROR_SUCCESS : GetLastError();
        if (hook && self.initialize_) self.initialize_();
        SetEvent(self.ready_);
        if (hook) {
            while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            UnhookWindowsHookEx(hook);
        }
        return 0;
    }
public:
    KeyboardHookThread() = default;
    KeyboardHookThread(const KeyboardHookThread&) = delete;
    KeyboardHookThread& operator=(const KeyboardHookThread&) = delete;
    ~KeyboardHookThread() { Stop(); }
    DWORD Start(HOOKPROC callback, void (*initialize)()) {
        if (thread_) return error_;
        ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ready_) return GetLastError();
        callback_ = callback; initialize_ = initialize;
        thread_ = CreateThread(nullptr, 0, Run, this, 0, &id_);
        if (!thread_) { const DWORD e = GetLastError(); CloseHandle(ready_); ready_ = nullptr; return e; }
        WaitForSingleObject(ready_, INFINITE);
        CloseHandle(ready_); ready_ = nullptr;
        return error_;
    }
    void Stop() {
        if (!thread_) return;
        PostThreadMessageW(id_, WM_QUIT, 0, 0);
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_); thread_ = nullptr; id_ = 0;
    }
};
}
