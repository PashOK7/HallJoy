#pragma once

#include <cstdint>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace halljoy::process
{

// Owns one process-local Win32 HANDLE whose numeric slot must never be closed
// by another subsystem. HANDLE_FLAG_PROTECT_FROM_CLOSE makes that ownership a
// kernel-enforced invariant; the sole owner explicitly removes protection only
// after the associated object has reached its terminal state.
class ProtectedNativeHandle final
{
public:
    ProtectedNativeHandle() noexcept = default;
    ~ProtectedNativeHandle() noexcept
    {
        std::uint32_t ignored = 0u;
        (void)Close(ignored);
    }

    ProtectedNativeHandle(const ProtectedNativeHandle&) = delete;
    ProtectedNativeHandle& operator=(const ProtectedNativeHandle&) = delete;
    ProtectedNativeHandle(ProtectedNativeHandle&&) = delete;
    ProtectedNativeHandle& operator=(ProtectedNativeHandle&&) = delete;

#if defined(_WIN32)
    [[nodiscard]] bool Adopt(HANDLE value, std::uint32_t& error) noexcept
    {
        error = ERROR_SUCCESS;
        if (value_ || !value || value == INVALID_HANDLE_VALUE)
        {
            error = ERROR_INVALID_PARAMETER;
            return false;
        }
        // Ownership transfers before protection is attempted. If the kernel
        // rejects the flag change, the caller must still retain this handle
        // until the associated object is terminal; losing it here would make
        // truthful reap impossible on the very failure path being contained.
        value_ = value;
        if (!SetHandleInformation(value, HANDLE_FLAG_PROTECT_FROM_CLOSE,
                HANDLE_FLAG_PROTECT_FROM_CLOSE))
        {
            error = GetLastError();
            return false;
        }
        return true;
    }

    [[nodiscard]] bool Close(std::uint32_t& error) noexcept
    {
        error = ERROR_SUCCESS;
        if (!value_)
            return true;
        if (!SetHandleInformation(value_, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0u))
        {
            error = GetLastError();
            return false;
        }
        if (!CloseHandle(value_))
        {
            error = GetLastError();
            return false;
        }
        value_ = nullptr;
        return true;
    }

    [[nodiscard]] bool ProtectionIntact(std::uint32_t& error) const noexcept
    {
        error = ERROR_SUCCESS;
        if (!value_)
        {
            error = ERROR_INVALID_HANDLE;
            return false;
        }
        DWORD flags = 0u;
        if (!GetHandleInformation(value_, &flags))
        {
            error = GetLastError();
            return false;
        }
        if ((flags & HANDLE_FLAG_PROTECT_FROM_CLOSE) == 0u)
        {
            error = ERROR_INVALID_STATE;
            return false;
        }
        return true;
    }

    [[nodiscard]] HANDLE Get() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return value_ != nullptr; }

private:
    HANDLE value_ = nullptr;
#else
    [[nodiscard]] bool Adopt(void*, std::uint32_t& error) noexcept
    {
        error = 0u;
        return false;
    }
    [[nodiscard]] bool Close(std::uint32_t& error) noexcept
    {
        error = 0u;
        return true;
    }
    [[nodiscard]] bool ProtectionIntact(std::uint32_t& error) const noexcept
    {
        error = 0u;
        return false;
    }
    [[nodiscard]] void* Get() const noexcept { return nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return false; }
#endif
};

} // namespace halljoy::process
