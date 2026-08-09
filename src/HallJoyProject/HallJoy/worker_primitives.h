#pragma once

#include <cstdint>
#include <type_traits>

#include "worker_lifecycle.h"

namespace halljoy::lifecycle
{

struct WaitToken
{
    std::uintptr_t value = 0;
    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
    friend constexpr bool operator==(WaitToken, WaitToken) noexcept = default;
};

struct ThreadToken
{
    std::uintptr_t value = 0;
    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
    friend constexpr bool operator==(ThreadToken, ThreadToken) noexcept = default;
};

static_assert(std::is_trivially_copyable_v<WaitToken>);
static_assert(std::is_trivially_copyable_v<ThreadToken>);

using WorkerEntry = void (*)(void*) noexcept;

enum class PrimitiveStatus : std::uint8_t
{
    Succeeded,
    Failed,
};

struct SignalCreateResult
{
    PrimitiveStatus status = PrimitiveStatus::Failed;
    WaitToken token{};
    std::uint32_t native_error = 0;

    [[nodiscard]] constexpr bool Succeeded() const noexcept
    {
        return status == PrimitiveStatus::Succeeded && token.IsValid();
    }
};

struct PrimitiveResult
{
    PrimitiveStatus status = PrimitiveStatus::Failed;
    std::uint32_t native_error = 0;

    [[nodiscard]] constexpr bool Succeeded() const noexcept
    {
        return status == PrimitiveStatus::Succeeded;
    }
};

enum class WaitStatus : std::uint8_t
{
    Signaled,
    TimedOut,
    Failed,
};

struct WaitResult
{
    WaitStatus status = WaitStatus::Failed;
    std::uint32_t native_error = 0;

    [[nodiscard]] constexpr bool Signaled() const noexcept
    {
        return status == WaitStatus::Signaled;
    }
};

enum class ThreadStartStatus : std::uint8_t
{
    Started,
    Failed,
};

struct ThreadStartResult
{
    ThreadStartStatus status = ThreadStartStatus::Failed;
    ThreadToken token{};
    std::uint32_t native_error = 0;

    [[nodiscard]] constexpr bool Started() const noexcept
    {
        return status == ThreadStartStatus::Started && token.IsValid();
    }
};

enum class JoinStatus : std::uint8_t
{
    Joined,
    TimedOut,
    Failed,
};

struct JoinResult
{
    JoinStatus status = JoinStatus::Failed;
    std::uint32_t native_error = 0;

    [[nodiscard]] constexpr bool Joined() const noexcept
    {
        return status == JoinStatus::Joined;
    }
};

static_assert(std::is_trivially_copyable_v<SignalCreateResult>);
static_assert(std::is_trivially_copyable_v<PrimitiveResult>);
static_assert(std::is_trivially_copyable_v<WaitResult>);
static_assert(std::is_trivially_copyable_v<ThreadStartResult>);
static_assert(std::is_trivially_copyable_v<JoinResult>);

// Platform adapters implement this seam. The lifecycle controller owns call
// order and state; the adapter owns OS details. Tests can inject deterministic
// time, resource-creation failures, wait timeouts and failed joins without a
// real thread or Windows HANDLE.
class WorkerPrimitives
{
public:
    virtual ~WorkerPrimitives() = default;

    [[nodiscard]] virtual std::uint64_t MonotonicMilliseconds() noexcept = 0;

    [[nodiscard]] virtual SignalCreateResult CreateStopSignal() noexcept = 0;
    [[nodiscard]] virtual PrimitiveResult SignalStop(WaitToken token) noexcept = 0;
    [[nodiscard]] virtual WaitResult WaitForStop(
        WaitToken token, std::uint32_t timeout_ms) noexcept = 0;

    [[nodiscard]] virtual ThreadStartResult StartThread(
        WorkerEntry entry, void* context) noexcept = 0;
    [[nodiscard]] virtual JoinResult JoinThread(
        ThreadToken token, std::uint32_t timeout_ms) noexcept = 0;

    virtual void CloseThread(ThreadToken token) noexcept = 0;
    virtual void CloseWait(WaitToken token) noexcept = 0;
};

} // namespace halljoy::lifecycle
