#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>
#include <dxgi.h>
#include <dwmapi.h>

#include "display_pacer.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace halljoy::display_pacer {

double NowSeconds() {
    static const double frequency = [] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return double(f.QuadPart); }();
    LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
    return double(now.QuadPart) / frequency;
}

namespace {
// The DXGI output that drives `monitor`, or nullptr.
IDXGIOutput* OutputFor(IDXGIFactory1* factory, HMONITOR monitor) {
    if (!factory || !monitor) return nullptr;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT a = 0; factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a) {
        IDXGIOutput* output = nullptr;
        for (UINT o = 0; adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o) {
            DXGI_OUTPUT_DESC desc{};
            if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == monitor) { adapter->Release(); return output; }
            output->Release();
        }
        adapter->Release();
    }
    return nullptr;
}
}

bool Pacer::Start(HWND target, UINT message, HMONITOR monitor) {
    if (shared_) { shared_->monitor.store(monitor); return true; }
    auto shared = std::make_shared<Shared>();
    shared->target = target;
    shared->message = message;
    shared->monitor.store(monitor);
    auto* owned = new std::shared_ptr<Shared>(shared);
    const auto thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Run, owned, 0, nullptr));
    if (!thread) { delete owned; return false; }
    CloseHandle(thread);
    shared_ = std::move(shared);
    return true;
}

void Pacer::Stop() {
    if (!shared_) return;
    shared_->stop.store(true);
    shared_.reset();
}

unsigned __stdcall Pacer::Run(void* parameter) {
    const std::shared_ptr<Shared> shared = std::move(*static_cast<std::shared_ptr<Shared>*>(parameter));
    delete static_cast<std::shared_ptr<Shared>*>(parameter);
    // A busy UI thread must not delay the frame signal itself.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    IDXGIFactory1* factory = nullptr;
    CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
    IDXGIOutput* output = nullptr;
    HMONITOR current = nullptr;
    int instantReturns = 0;
    while (!shared->stop.load()) {
        const HMONITOR wanted = shared->monitor.load();
        if (wanted != current) {
            if (output) output->Release();
            output = OutputFor(factory, wanted);
            current = wanted;
            instantReturns = 0;
        }
        const double before = NowSeconds();
        bool waited = output && SUCCEEDED(output->WaitForVBlank());
        // Some drivers return at once (display asleep, remote sessions): never spin.
        if (waited && NowSeconds() - before < 0.0003) {
            if (++instantReturns > 3) waited = false;
        } else if (waited) {
            instantReturns = 0;
        }
        if (!waited && FAILED(DwmFlush())) Sleep(8);
        if (shared->stop.load()) break;
        shared->frameTime.store(NowSeconds());
        if (!shared->posted.exchange(true) && !PostMessageW(shared->target, shared->message, 0, 0))
            shared->posted.store(false);
    }
    if (output) output->Release();
    if (factory) factory->Release();
    return 0;
}

} // namespace halljoy::display_pacer
