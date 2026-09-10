#include <cassert>
#include <cstdint>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <cwchar>
#include <new>
#include <string>
#include <vector>

#include "../HallJoy/vigem_output_channel.h"

namespace
{
using namespace halljoy::vigem_output;

constexpr std::uint32_t kOwnerPidSentinel = 0u;
constexpr std::uint64_t kNonce = 0xBADC0FFEE0DDF00Dull;
constexpr std::uint64_t kOutputGeneration = 37u;
constexpr std::uint64_t kPublicationCount = 100000u;

std::uint64_t ParseU64(const char* text, int base)
{
    if (!text || !*text)
        return 0u;
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, base);
    return end != text && *end == '\0' ? static_cast<std::uint64_t>(value) : 0u;
}

XusbReportV1 MakeReport(std::uint64_t sequence, std::uint32_t pad)
{
    XusbReportV1 report{};
    report.buttons = static_cast<std::uint16_t>(sequence);
    report.leftTrigger = static_cast<std::uint8_t>(sequence >> 8u);
    report.rightTrigger = static_cast<std::uint8_t>(pad);
    report.thumbLX = static_cast<std::int16_t>(sequence);
    report.thumbLY = static_cast<std::int16_t>(~sequence);
    report.thumbRX = static_cast<std::int16_t>(sequence ^ (pad * 0x1111u));
    report.thumbRY = static_cast<std::int16_t>((sequence >> 1u) ^ (pad * 0x2222u));
    return report;
}

bool ReportMatches(const SnapshotPayloadV1& snapshot)
{
    if (snapshot.padCount != kMaxPads || snapshot.validMask != 0x0fu)
        return false;
    for (std::uint32_t pad = 0; pad < kMaxPads; ++pad)
    {
        const XusbReportV1 expected = MakeReport(snapshot.publicationSequence, pad);
        const XusbReportV1& actual = snapshot.reports[pad];
        if (actual.buttons != expected.buttons ||
            actual.leftTrigger != expected.leftTrigger ||
            actual.rightTrigger != expected.rightTrigger ||
            actual.thumbLX != expected.thumbLX ||
            actual.thumbLY != expected.thumbLY ||
            actual.thumbRX != expected.thumbRX ||
            actual.thumbRY != expected.thumbRY)
        {
            return false;
        }
    }
    return true;
}

int RunChild(int argc, char** argv)
{
    if (argc != 8)
        return 31;
    HANDLE mapping = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(ParseU64(argv[2], 16)));
    HANDLE wakeEvent = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(ParseU64(argv[3], 16)));
    HANDLE stopEvent = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(ParseU64(argv[4], 16)));
    const std::uint32_t ownerPid = static_cast<std::uint32_t>(ParseU64(argv[5], 10));
    const std::uint64_t nonce = ParseU64(argv[6], 16);
    const std::uint64_t generation = ParseU64(argv[7], 10);
    DWORD flags = 0;
    if (!mapping || !wakeEvent || !stopEvent || ownerPid == kOwnerPidSentinel ||
        nonce == 0u || generation == 0u ||
        !GetHandleInformation(mapping, &flags) ||
        !GetHandleInformation(wakeEvent, &flags) ||
        !GetHandleInformation(stopEvent, &flags))
    {
        return 32;
    }

    auto* shared = static_cast<SharedStateV1*>(MapViewOfFile(
        mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedStateV1)));
    if (!shared)
        return 33;
    if (!ValidateSharedState(*shared, ownerPid, nonce))
    {
        UnmapViewOfFile(shared);
        return 41;
    }

    InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&shared->childPid),
        static_cast<LONG>(GetCurrentProcessId()));
    InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&shared->childReportedState),
        static_cast<LONG>(ChildState::Ready));

    std::uint64_t observed = 0u;
    HANDLE waits[2]{ stopEvent, wakeEvent };
    for (;;)
    {
        const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, 250u);
        if (wait == WAIT_OBJECT_0)
            break;
        if (wait != WAIT_OBJECT_0 + 1u && wait != WAIT_TIMEOUT)
        {
            UnmapViewOfFile(shared);
            return 42;
        }

        SnapshotPayloadV1 snapshot{};
        if (TryConsumeNewestSnapshot(*shared, generation, observed, &snapshot) ==
            ConsumeResult::Updated)
        {
            if (snapshot.publicationSequence <= observed || !ReportMatches(snapshot))
            {
                UnmapViewOfFile(shared);
                return 43;
            }
            observed = snapshot.publicationSequence;
            InterlockedExchange64(
                reinterpret_cast<volatile LONG64*>(&shared->appliedPublicationSequence),
                static_cast<LONG64>(observed));
        }
    }

    InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&shared->childReportedState),
        static_cast<LONG>(ChildState::Stopped));
    UnmapViewOfFile(shared);
    return 0;
}

bool StartChild(
    const wchar_t* executable,
    HANDLE mapping,
    HANDLE wakeEvent,
    HANDLE stopEvent,
    std::uint32_t ownerPid,
    std::uint64_t nonce,
    std::uint64_t generation,
    PROCESS_INFORMATION& process)
{
    wchar_t command[2048]{};
    const int length = swprintf_s(command, L"\"%s\" --child %llX %llX %llX %lu %llX %llu",
        executable,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(mapping)),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(wakeEvent)),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(stopEvent)),
        static_cast<unsigned long>(ownerPid),
        static_cast<unsigned long long>(nonce),
        static_cast<unsigned long long>(generation));
    if (length <= 0 || static_cast<std::size_t>(length) >= std::size(command))
        return false;

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    ZeroMemory(&process, sizeof(process));
    return CreateProcessW(executable, command, nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE;
}

DWORD WaitChild(PROCESS_INFORMATION& process, DWORD timeoutMs)
{
    const DWORD waitResult = WaitForSingleObject(process.hProcess, timeoutMs);
    if (waitResult != WAIT_OBJECT_0)
    {
        // This is a test-owned process: never let a failed/timeout assertion
        // leave the child alive after the parent has closed its IPC objects.
        TerminateProcess(process.hProcess, STILL_ACTIVE);
        WaitForSingleObject(process.hProcess, 5000u);
    }
    DWORD code = STILL_ACTIVE;
    if (waitResult == WAIT_OBJECT_0)
        GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    process = {};
    return code;
}

int RunParent()
{
    SECURITY_ATTRIBUTES inherited{};
    inherited.nLength = sizeof(inherited);
    inherited.bInheritHandle = TRUE;
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &inherited,
        PAGE_READWRITE, 0, sizeof(SharedStateV1), nullptr);
    HANDLE wakeEvent = CreateEventW(&inherited, FALSE, FALSE, nullptr);
    HANDLE stopEvent = CreateEventW(&inherited, TRUE, FALSE, nullptr);
    assert(mapping && wakeEvent && stopEvent);
    auto* shared = static_cast<SharedStateV1*>(MapViewOfFile(
        mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedStateV1)));
    assert(shared);
    new (shared) SharedStateV1{};
    const std::uint32_t ownerPid = GetCurrentProcessId();
    InitializeSharedState(*shared, ownerPid, kNonce);

    wchar_t executable[32768]{};
    assert(GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable))) != 0u);

    // A child with a mismatched launch nonce must fail before touching data.
    PROCESS_INFORMATION rejected{};
    assert(StartChild(executable, mapping, wakeEvent, stopEvent, ownerPid,
        kNonce ^ 1u, kOutputGeneration, rejected));
    assert(WaitChild(rejected, 5000u) == 41u);

    PROCESS_INFORMATION child{};
    assert(StartChild(executable, mapping, wakeEvent, stopEvent, ownerPid,
        kNonce, kOutputGeneration, child));
    const ULONGLONG readyDeadline = GetTickCount64() + 5000u;
    LONG readyState = 0;
    do
    {
        readyState = InterlockedCompareExchange(
            reinterpret_cast<volatile LONG*>(&shared->childReportedState), 0, 0);
        if (readyState == static_cast<LONG>(ChildState::Ready))
            break;
        Sleep(1);
    } while (GetTickCount64() < readyDeadline);
    assert(readyState == static_cast<LONG>(ChildState::Ready));
    assert(BeginOutputGeneration(*shared, kOutputGeneration, kMaxPads));
    assert(PublishProducerLease(*shared, GetTickCount64()));

    std::uint64_t successful = 0u;
    while (successful < kPublicationCount)
    {
        const std::uint64_t expectedSequence = successful + 1u;
        XusbReportV1 reports[kMaxPads]{};
        for (std::uint32_t pad = 0; pad < kMaxPads; ++pad)
            reports[pad] = MakeReport(expectedSequence, pad);
        std::uint64_t published = 0u;
        const PublishResult result = TryPublishSnapshot(
            *shared, reports, kMaxPads, 0x0fu, expectedSequence, &published);
        if (result == PublishResult::Contended)
        {
            SwitchToThread();
            continue;
        }
        assert(result == PublishResult::Published);
        assert(published == expectedSequence);
        ++successful;
        SetEvent(wakeEvent);
    }

    const ULONGLONG appliedDeadline = GetTickCount64() + 10000u;
    std::uint64_t appliedSequence = 0u;
    do
    {
        appliedSequence = static_cast<std::uint64_t>(InterlockedCompareExchange64(
            reinterpret_cast<volatile LONG64*>(&shared->appliedPublicationSequence), 0, 0));
        if (appliedSequence >= kPublicationCount)
            break;
        SetEvent(wakeEvent);
        Sleep(1);
    } while (GetTickCount64() < appliedDeadline);
    assert(appliedSequence == kPublicationCount);
    assert(DisableOutputGeneration(*shared, kOutputGeneration));
    assert(PublicationQuiescent(*shared));
    SetEvent(stopEvent);
    assert(WaitChild(child, 5000u) == 0u);

    UnmapViewOfFile(shared);
    CloseHandle(stopEvent);
    CloseHandle(wakeEvent);
    CloseHandle(mapping);
    return 0;
}
}

int main(int argc, char** argv)
{
    if (argc > 1)
        return RunChild(argc, argv);
    return RunParent();
}

#else
int main()
{
    // The exact cross-process contract is Windows-only. Portable thread-level
    // behavior remains covered by vigem_output_channel_test.cpp.
    return 0;
}
#endif
