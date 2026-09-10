#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace halljoy::instance_guard
{

enum class AcquireResult : unsigned char
{
    Acquired,
    Conflicted,
    Failed,
};

// One process which can own HallJoy's per-user settings root and device
// sessions. The guard deliberately does not replace any device protocol lease.
class Guard final
{
public:
    Guard() noexcept = default;
    ~Guard() noexcept;
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

    [[nodiscard]] AcquireResult AcquireForCurrentUser() noexcept;
    void Release() noexcept;
    [[nodiscard]] DWORD LastError() const noexcept { return lastError_; }

private:
    [[nodiscard]] AcquireResult AcquireNamed(const wchar_t* name) noexcept;

    HANDLE mutex_ = nullptr;
    DWORD lastError_ = ERROR_SUCCESS;
};

} // namespace halljoy::instance_guard
