#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "worker_exception_barrier.h"

using halljoy::worker::RunWorkerEntryBarrier;
using halljoy::worker::WorkerExceptionKind;
using halljoy::worker::WorkerExceptionRecord;

namespace
{
struct Probe
{
    int fault_calls = 0;
    int completion_calls = 0;
    WorkerExceptionRecord fault{};
    WorkerExceptionRecord completion{};
};

void TestSuccess()
{
    Probe probe{};
    const std::uint32_t result = RunWorkerEntryBarrier(
        [] { return 17u; },
        [&probe](const WorkerExceptionRecord& record) noexcept {
            ++probe.fault_calls;
            probe.fault = record;
        },
        [&probe](const WorkerExceptionRecord& record) noexcept {
            ++probe.completion_calls;
            probe.completion = record;
        },
        99u);

    assert(result == 17u);
    assert(probe.fault_calls == 0);
    assert(probe.completion_calls == 1);
    assert(!probe.completion.HasFault());
}

void TestStandardException()
{
    Probe probe{};
    const std::uint32_t result = RunWorkerEntryBarrier(
        []() -> std::uint32_t { throw std::runtime_error("worker failed"); },
        [&probe](const WorkerExceptionRecord& record) noexcept {
            ++probe.fault_calls;
            probe.fault = record;
        },
        [&probe](const WorkerExceptionRecord& record) noexcept {
            ++probe.completion_calls;
            probe.completion = record;
        },
        0xE0510001u);

    assert(result == 0xE0510001u);
    assert(probe.fault_calls == 1);
    assert(probe.completion_calls == 1);
    assert(probe.fault.kind == WorkerExceptionKind::StandardException);
    assert(probe.completion.kind == WorkerExceptionKind::StandardException);
    assert(std::strcmp(probe.fault.message, "worker failed") == 0);
}

void TestUnknownException()
{
    Probe probe{};
    const std::uint32_t result = RunWorkerEntryBarrier(
        []() -> std::uint32_t { throw 42; },
        [&probe](const WorkerExceptionRecord& record) noexcept {
            ++probe.fault_calls;
            probe.fault = record;
        },
        [&probe](const WorkerExceptionRecord& record) noexcept {
            ++probe.completion_calls;
            probe.completion = record;
        },
        0xE0510002u);

    assert(result == 0xE0510002u);
    assert(probe.fault_calls == 1);
    assert(probe.completion_calls == 1);
    assert(probe.fault.kind == WorkerExceptionKind::UnknownException);
    assert(std::strcmp(probe.fault.message, "non-std exception") == 0);
}

void TestMessageTruncation()
{
    char long_message[512]{};
    for (std::size_t i = 0; i + 1 < sizeof(long_message); ++i)
        long_message[i] = 'x';

    Probe probe{};
    const std::uint32_t result = RunWorkerEntryBarrier(
        [&long_message]() -> std::uint32_t { throw std::runtime_error(long_message); },
        [&probe](const WorkerExceptionRecord& record) noexcept { probe.fault = record; },
        [&probe](const WorkerExceptionRecord& record) noexcept { probe.completion = record; },
        1u);

    assert(result == 1u);
    assert(probe.fault.message[WorkerExceptionRecord::kMessageCapacity - 1] == '\0');
    assert(std::strlen(probe.fault.message) == WorkerExceptionRecord::kMessageCapacity - 1);
}
} // namespace

int main()
{
    TestSuccess();
    TestStandardException();
    TestUnknownException();
    TestMessageTruncation();
    return 0;
}
