#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine_runtime_ui_bridge.h"

#include <mutex>

namespace halljoy::engine_runtime::ui_bridge
{
namespace
{
constexpr DWORD kUiAcknowledgementTimeoutMs = 5000u;

enum class RequestState : std::uint8_t { Idle, Pending, Executing, Completed, Cancelled };

std::mutex g_mutex;
HWND g_window = nullptr;
UINT g_message = 0;
Handler g_handler = nullptr;
HANDLE g_completionEvent = nullptr;
std::uintptr_t g_token = 0;
Operation g_operation = Operation::ReleaseInput;
std::uint32_t g_error = ERROR_SUCCESS;
bool g_result = false;
RequestState g_state = RequestState::Idle;

bool IsStartedLocked() noexcept
{
    return g_window && g_message != 0 && g_handler && g_completionEvent;
}
} // namespace

bool Start(void* windowHandle, unsigned message, Handler handler) noexcept
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (IsStartedLocked())
        return g_window == static_cast<HWND>(windowHandle) && g_message == message && g_handler == handler;
    if (!windowHandle || message == 0 || !handler || g_completionEvent)
        return false;
    g_completionEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_completionEvent)
        return false;
    g_window = static_cast<HWND>(windowHandle);
    g_message = message;
    g_handler = handler;
    g_token = 0;
    g_state = RequestState::Idle;
    return true;
}

bool Execute(Operation operation, std::uint32_t& nativeError) noexcept
{
    std::uintptr_t token = 0;
    HANDLE completion = nullptr;
    {
        const std::lock_guard<std::mutex> lock(g_mutex);
        if (!IsStartedLocked() || g_state != RequestState::Idle)
        {
            nativeError = ERROR_BUSY;
            return false;
        }
        ++g_token;
        if (g_token == 0) ++g_token;
        token = g_token;
        g_operation = operation;
        g_error = ERROR_SUCCESS;
        g_result = false;
        g_state = RequestState::Pending;
        ResetEvent(g_completionEvent);
        completion = g_completionEvent;
        if (!PostMessageW(g_window, g_message, static_cast<WPARAM>(token), 0))
        {
            nativeError = GetLastError();
            g_state = RequestState::Idle;
            return false;
        }
    }

    const DWORD wait = WaitForSingleObject(completion, kUiAcknowledgementTimeoutMs);
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (wait != WAIT_OBJECT_0 || g_token != token || g_state == RequestState::Cancelled)
    {
        if (g_state == RequestState::Pending)
            g_state = RequestState::Cancelled;
        nativeError = wait == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT;
        return false;
    }
    if (g_state != RequestState::Completed)
    {
        nativeError = ERROR_INVALID_STATE;
        return false;
    }
    nativeError = g_error;
    const bool result = g_result;
    g_state = RequestState::Idle;
    return result;
}

bool Dispatch(std::uintptr_t token) noexcept
{
    Handler handler = nullptr;
    Operation operation{};
    {
        const std::lock_guard<std::mutex> lock(g_mutex);
        if (!IsStartedLocked() || token == 0 || token != g_token || g_state != RequestState::Pending)
            return false;
        g_state = RequestState::Executing;
        handler = g_handler;
        operation = g_operation;
    }

    std::uint32_t error = ERROR_SUCCESS;
    const bool result = handler(operation, error);

    const std::lock_guard<std::mutex> lock(g_mutex);
    if (token != g_token || g_state != RequestState::Executing)
        return false;
    g_result = result;
    g_error = error;
    g_state = RequestState::Completed;
    SetEvent(g_completionEvent);
    return true;
}

void CancelPending() noexcept
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state == RequestState::Pending)
    {
        g_state = RequestState::Cancelled;
        g_error = ERROR_CANCELLED;
        SetEvent(g_completionEvent);
    }
}

bool Stop() noexcept
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_completionEvent)
        return true;
    if (g_state == RequestState::Pending || g_state == RequestState::Executing)
        return false;
    CloseHandle(g_completionEvent);
    g_completionEvent = nullptr;
    g_window = nullptr;
    g_message = 0;
    g_handler = nullptr;
    g_state = RequestState::Idle;
    return true;
}

} // namespace halljoy::engine_runtime::ui_bridge
