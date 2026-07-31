#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

namespace halljoy::worker
{

enum class WorkerExceptionKind : std::uint8_t
{
    None,
    StandardException,
    UnknownException,
};

// Fixed-size and allocation-free so it can be produced while handling
// std::bad_alloc or another exception at a thread/C ABI boundary.
struct WorkerExceptionRecord
{
    static constexpr std::size_t kMessageCapacity = 192;

    WorkerExceptionKind kind = WorkerExceptionKind::None;
    char message[kMessageCapacity]{};

    [[nodiscard]] constexpr bool HasFault() const noexcept
    {
        return kind != WorkerExceptionKind::None;
    }
};

static_assert(std::is_trivially_copyable_v<WorkerExceptionRecord>);

inline void CopyWorkerExceptionMessage(
    WorkerExceptionRecord& record, const char* message) noexcept
{
    if (!message)
        return;

    std::size_t index = 0;
    while (index + 1 < WorkerExceptionRecord::kMessageCapacity && message[index] != '\0')
    {
        record.message[index] = message[index];
        ++index;
    }
    record.message[index] = '\0';
}

// The callbacks must be noexcept. They run after stack unwinding and therefore
// may only publish fixed diagnostics, close producer/running gates, reset safe
// externally visible values, and signal completion. They must not allocate.
template <typename Body, typename OnFault, typename OnCompletion>
[[nodiscard]] std::uint32_t RunWorkerEntryBarrier(
    Body&& body,
    OnFault&& on_fault,
    OnCompletion&& on_completion,
    std::uint32_t fault_exit_code) noexcept
{
    static_assert(std::is_invocable_v<Body&>, "worker body must be invocable without arguments");
    static_assert(std::is_convertible_v<std::invoke_result_t<Body&>, std::uint32_t>,
        "worker body result must be convertible to uint32_t");
    static_assert(std::is_nothrow_invocable_v<OnFault&, const WorkerExceptionRecord&>,
        "fault callback must be noexcept");
    static_assert(std::is_nothrow_invocable_v<OnCompletion&, const WorkerExceptionRecord&>,
        "completion callback must be noexcept");

    WorkerExceptionRecord record{};
    std::uint32_t exit_code = 0;

    try
    {
        exit_code = static_cast<std::uint32_t>(std::invoke(body));
    }
    catch (const std::exception& ex)
    {
        record.kind = WorkerExceptionKind::StandardException;
        CopyWorkerExceptionMessage(record, ex.what());
        std::invoke(on_fault, record);
        exit_code = fault_exit_code;
    }
    catch (...)
    {
        record.kind = WorkerExceptionKind::UnknownException;
        CopyWorkerExceptionMessage(record, "non-std exception");
        std::invoke(on_fault, record);
        exit_code = fault_exit_code;
    }

    std::invoke(on_completion, record);
    return exit_code;
}

} // namespace halljoy::worker
