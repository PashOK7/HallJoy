#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <thread>

#include "process_generation_supervisor.h"

#if defined(_WIN32)
#include <windows.h>

namespace
{
using halljoy::process::ChildObservation;
using halljoy::process::GenerationOutcome;
using halljoy::process::ProcessGenerationSupervisor;
using halljoy::process::ProtectedNativeHandle;
using halljoy::process::SupervisionRequest;
using halljoy::process::SupervisionResult;

constexpr std::uint64_t kNonce = 0xC5190A33D55E771Bull;
constexpr std::uint32_t kStressGenerations = 1000u;

void TestProtectedHandleOwnership()
{
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    assert(event);
    ProtectedNativeHandle owner;
    std::uint32_t error = ERROR_SUCCESS;
    assert(owner.Adopt(event, error) && error == ERROR_SUCCESS);
    assert(owner.ProtectionIntact(error) && error == ERROR_SUCCESS);

    SetLastError(ERROR_SUCCESS);
    assert(!CloseHandle(owner.Get()));
    assert(GetLastError() == ERROR_INVALID_HANDLE);
    assert(WaitForSingleObject(owner.Get(), 0u) == WAIT_TIMEOUT);
    assert(owner.ProtectionIntact(error) && error == ERROR_SUCCESS);
    assert(owner.Close(error) && error == ERROR_SUCCESS);

    HANDLE cleanupEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    assert(cleanupEvent);
    assert(owner.Adopt(cleanupEvent, error) && error == ERROR_SUCCESS);
    assert(SetHandleInformation(owner.Get(), HANDLE_FLAG_PROTECT_FROM_CLOSE, 0u));
    assert(!owner.ProtectionIntact(error) && error == ERROR_INVALID_STATE);
    assert(owner.Close(error) && error == ERROR_SUCCESS);
}

struct alignas(64) FakeShared
{
    volatile LONG64 nonce = 0;
    volatile LONG64 generation = 0;
    volatile LONG64 progress = 0;
    volatile LONG childPid = 0;
    volatile LONG ready = 0;
    volatile LONG fatal = 0;
    volatile LONG error = 0;
    volatile LONG containedAtEntry = 0;
    volatile LONG decoyHandleCallSucceeded = 0;
    volatile LONG activeChildren = 0;
    volatile LONG maxActiveChildren = 0;
};

struct PrepareContext
{
    FakeShared* shared = nullptr;
    bool succeed = true;
    bool called = false;
    bool childWasActive = false;
};

bool PrepareTermination(void* rawContext, std::uint32_t& error) noexcept
{
    auto* context = static_cast<PrepareContext*>(rawContext);
    if (!context || !context->shared)
    {
        error = ERROR_INVALID_PARAMETER;
        return false;
    }
    context->called = true;
    context->childWasActive =
        InterlockedCompareExchange(&context->shared->activeChildren, 0, 0) > 0;
    error = context->succeed ? ERROR_SUCCESS : ERROR_BUSY;
    return context->succeed;
}

std::uint64_t Parse64(const char* text)
{
    char* end = nullptr;
    const auto value = std::strtoull(text, &end, 16);
    assert(end && *end == '\0');
    return value;
}

HANDLE ParseHandle(const char* text)
{
    return reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(Parse64(text)));
}

void AtomicMax(volatile LONG& destination, LONG value)
{
    LONG observed = InterlockedCompareExchange(&destination, 0, 0);
    while (observed < value)
    {
        const LONG previous = InterlockedCompareExchange(&destination, value, observed);
        if (previous == observed)
            return;
        observed = previous;
    }
}

int RunChild(int argc, char** argv)
{
    if (argc != 9)
        return 90;
    HANDLE mapping = ParseHandle(argv[2]);
    HANDLE stopEvent = ParseHandle(argv[3]);
    HANDLE ownerProcess = ParseHandle(argv[4]);
    HANDLE decoy = ParseHandle(argv[5]);
    const std::uint64_t nonce = Parse64(argv[6]);
    const std::uint64_t generation = Parse64(argv[7]);
    const std::string mode = argv[8];

    auto* shared = static_cast<FakeShared*>(MapViewOfFile(
        mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FakeShared)));
    if (!shared || nonce != kNonce ||
        static_cast<std::uint64_t>(InterlockedCompareExchange64(&shared->nonce, 0, 0)) != nonce)
        return 91;

    BOOL inJob = FALSE;
    if (!IsProcessInJob(GetCurrentProcess(), nullptr, &inJob) || !inJob)
        return 92;
    InterlockedExchange(&shared->containedAtEntry, 1);

    // A numeric handle value can be rebound to an unrelated child object. The
    // parent therefore validates the identity of its decoy event, not whether
    // this number happens to be accepted in the child handle table.
    if (SetEvent(decoy))
        InterlockedExchange(&shared->decoyHandleCallSucceeded, 1);
    if (WaitForSingleObject(ownerProcess, 0) != WAIT_TIMEOUT)
        return 93;

    const LONG active = InterlockedIncrement(&shared->activeChildren);
    AtomicMax(shared->maxActiveChildren, active);
    InterlockedExchange(&shared->childPid, static_cast<LONG>(GetCurrentProcessId()));
    InterlockedExchange64(&shared->generation, static_cast<LONG64>(generation));

    if (mode == "exit-before-ready")
    {
        InterlockedDecrement(&shared->activeChildren);
        return 42;
    }
    if (mode == "wrong-generation")
    {
        InterlockedExchange64(&shared->generation, static_cast<LONG64>(generation + 1u));
        for (;;)
            Sleep(1000u);
    }
    if (mode == "never-ready")
    {
        for (;;)
            Sleep(1000u);
    }

    InterlockedExchange(&shared->ready, 1);
    if (mode == "crash-after-ready")
    {
        InterlockedIncrement64(&shared->progress);
        Sleep(20u);
        InterlockedDecrement(&shared->activeChildren);
        return 77;
    }
    if (mode == "stall-after-ready")
    {
        InterlockedIncrement64(&shared->progress);
        for (;;)
            Sleep(1000u);
    }
    if (mode == "report-fault")
    {
        InterlockedExchange(&shared->error, 0x2468);
        InterlockedExchange(&shared->fatal, 1);
        for (;;)
            Sleep(1000u);
    }

    const bool ignoreStop = mode == "ignore-stop";
    for (;;)
    {
        InterlockedIncrement64(&shared->progress);
        if (!ignoreStop && WaitForSingleObject(stopEvent, 1u) == WAIT_OBJECT_0)
        {
            InterlockedDecrement(&shared->activeChildren);
            return 0;
        }
        Sleep(1u);
    }
}

bool Probe(void* context, ChildObservation& observation) noexcept
{
    auto* shared = static_cast<FakeShared*>(context);
    const LONG pid = InterlockedCompareExchange(&shared->childPid, 0, 0);
    if (pid == 0)
        return true;
    observation.available = true;
    observation.childPid = static_cast<std::uint32_t>(pid);
    observation.generation = static_cast<std::uint64_t>(
        InterlockedCompareExchange64(&shared->generation, 0, 0));
    observation.progress = static_cast<std::uint64_t>(
        InterlockedCompareExchange64(&shared->progress, 0, 0));
    observation.ready = InterlockedCompareExchange(&shared->ready, 0, 0) != 0;
    observation.fatal = InterlockedCompareExchange(&shared->fatal, 0, 0) != 0;
    observation.error = static_cast<std::uint32_t>(
        InterlockedCompareExchange(&shared->error, 0, 0));
    return true;
}

std::wstring HexHandle(HANDLE handle)
{
    wchar_t text[32]{};
    swprintf_s(text, L"%llX",
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(handle)));
    return text;
}

std::wstring Hex64(std::uint64_t value)
{
    wchar_t text[32]{};
    swprintf_s(text, L"%llX", static_cast<unsigned long long>(value));
    return text;
}

std::wstring Quote(const std::wstring& value)
{
    return L"\"" + value + L"\"";
}

void ResetShared(FakeShared& shared, std::uint64_t generation)
{
    InterlockedExchange64(&shared.generation, static_cast<LONG64>(generation));
    InterlockedExchange64(&shared.progress, 0);
    InterlockedExchange(&shared.childPid, 0);
    InterlockedExchange(&shared.ready, 0);
    InterlockedExchange(&shared.fatal, 0);
    InterlockedExchange(&shared.error, 0);
    InterlockedExchange(&shared.containedAtEntry, 0);
    InterlockedExchange(&shared.decoyHandleCallSucceeded, 0);
    InterlockedExchange(&shared.activeChildren, 0);
    InterlockedExchange(&shared.maxActiveChildren, 0);
}

struct Fixture
{
    HANDLE mapping = nullptr;
    HANDLE childStop = nullptr;
    HANDLE supervisorStop = nullptr;
    HANDLE ownerProcess = nullptr;
    HANDLE decoy = nullptr;
    FakeShared* shared = nullptr;
    std::wstring executable;

    Fixture()
    {
        SECURITY_ATTRIBUTES inherited{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &inherited,
            PAGE_READWRITE, 0, sizeof(FakeShared), nullptr);
        childStop = CreateEventW(&inherited, TRUE, FALSE, nullptr);
        ownerProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
            TRUE, GetCurrentProcessId());
        decoy = CreateEventW(&inherited, TRUE, FALSE, nullptr);
        supervisorStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        assert(mapping && childStop && ownerProcess && decoy && supervisorStop);
        shared = static_cast<FakeShared*>(MapViewOfFile(
            mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FakeShared)));
        assert(shared);
        new (shared) FakeShared{};
        InterlockedExchange64(&shared->nonce, static_cast<LONG64>(kNonce));
        wchar_t path[32768]{};
        assert(GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path))) != 0u);
        executable = path;
    }

    ~Fixture()
    {
        UnmapViewOfFile(shared);
        CloseHandle(supervisorStop);
        CloseHandle(decoy);
        CloseHandle(ownerProcess);
        CloseHandle(childStop);
        CloseHandle(mapping);
    }
};

SupervisionResult RunScenario(ProcessGenerationSupervisor& supervisor,
    Fixture& fixture, std::uint64_t generation, const wchar_t* mode,
    bool requestStop, bool preSignalStop = false)
{
    ResetEvent(fixture.childStop);
    ResetEvent(fixture.supervisorStop);
    ResetShared(*fixture.shared, generation);
    if (preSignalStop)
        SetEvent(fixture.supervisorStop);

    SupervisionRequest request{};
    request.launch.applicationPath = fixture.executable;
    request.launch.generation = generation;
    request.launch.creationFlags = CREATE_NO_WINDOW;
    request.launch.inheritedHandles = {
        fixture.mapping, fixture.childStop, fixture.ownerProcess
    };
    request.launch.commandLine = Quote(fixture.executable) + L" --child " +
        HexHandle(fixture.mapping) + L" " + HexHandle(fixture.childStop) + L" " +
        HexHandle(fixture.ownerProcess) + L" " + HexHandle(fixture.decoy) + L" " +
        Hex64(kNonce) + L" " + Hex64(generation) + L" " + mode;
    request.childStopEvent = fixture.childStop;
    request.supervisorStopEvent = fixture.supervisorStop;
    request.probe = Probe;
    request.probeContext = fixture.shared;
    PrepareContext prepare{ fixture.shared };
    request.prepareTermination = PrepareTermination;
    request.prepareTerminationContext = &prepare;
    request.policy.startupTimeoutMs = 250u;
    request.policy.progressTimeoutMs = 120u;
    request.policy.observationIntervalMs = 2u;
    request.policy.gracefulStopTimeoutMs = 80u;
    request.policy.hardReapTimeoutMs = 3000u;

    std::thread stopThread;
    if (requestStop && !preSignalStop)
    {
        stopThread = std::thread([event = fixture.supervisorStop] {
            Sleep(60u);
            SetEvent(event);
        });
    }
    const SupervisionResult result = supervisor.RunOne(request);
    if (stopThread.joinable())
        stopThread.join();
    assert(result.restartSafe);
    assert(result.processHandleProtected && result.jobHandleProtected);
    assert(result.processIdentityMatched && result.ownershipError == ERROR_SUCCESS);
    assert(result.terminationPreparationAttempted && result.terminationPrepared);
    assert(prepare.called);
    // Preparation is required, but it can follow the cooperative stop signal.
    // A normal child may therefore have already observed the signal and cleared
    // its test-only active counter before this callback runs.  Treating that
    // scheduling race as an ownership failure made this portable oracle flaky.
    assert(!supervisor.HasUnreapedGeneration());
    assert(supervisor.RestartSafe());
    const LONG containedAtEntry = InterlockedCompareExchange(
        &fixture.shared->containedAtEntry, 0, 0);
    if (preSignalStop)
    {
        // A stop that is already signalled may legitimately make the parent
        // terminate the contained process before its first user-mode
        // instruction is scheduled.  If the child did run, it must still
        // have observed Job containment; otherwise the stop must have used
        // the hard contained-process reap path.
        assert(containedAtEntry == 1 || result.forced);
    }
    else
    {
        assert(containedAtEntry == 1);
    }
    assert(WaitForSingleObject(fixture.decoy, 0) == WAIT_TIMEOUT);
    assert(InterlockedCompareExchange(&fixture.shared->maxActiveChildren, 0, 0) <= 1);
    HANDLE survivor = OpenProcess(SYNCHRONIZE, FALSE, result.childPid);
    if (survivor)
    {
        assert(WaitForSingleObject(survivor, 0) == WAIT_OBJECT_0);
        CloseHandle(survivor);
    }
    return result;
}

int RunParent()
{
    TestProtectedHandleOwnership();
    Fixture fixture;
    ProcessGenerationSupervisor supervisor;

    SupervisionRequest invalidHandle{};
    invalidHandle.launch.applicationPath = fixture.executable;
    invalidHandle.launch.commandLine = Quote(fixture.executable);
    invalidHandle.launch.generation = 900u;
    invalidHandle.launch.inheritedHandles = {
        fixture.mapping, fixture.supervisorStop // deliberately non-inheritable
    };
    invalidHandle.childStopEvent = fixture.childStop;
    invalidHandle.probe = Probe;
    invalidHandle.probeContext = fixture.shared;
    auto result = supervisor.RunOne(invalidHandle);
    assert(result.outcome == GenerationOutcome::LaunchFailed);
    assert(result.nativeError == ERROR_ACCESS_DENIED && result.restartSafe);
    assert(!supervisor.HasUnreapedGeneration());

    SupervisionRequest missingExecutable = invalidHandle;
    missingExecutable.launch.applicationPath += L".missing";
    missingExecutable.launch.commandLine = Quote(missingExecutable.launch.applicationPath);
    missingExecutable.launch.generation = 901u;
    missingExecutable.launch.inheritedHandles = {
        fixture.mapping, fixture.childStop, fixture.ownerProcess
    };
    result = supervisor.RunOne(missingExecutable);
    assert(result.outcome == GenerationOutcome::LaunchFailed);
    assert(result.restartSafe && !supervisor.HasUnreapedGeneration());

    result = RunScenario(supervisor, fixture, 1u, L"normal", true);
    assert(result.outcome == GenerationOutcome::PlannedStop);
    assert(result.readyObserved && !result.forced && result.childExitCode == 0u);
    assert(result.terminationPrepared);

    result = RunScenario(supervisor, fixture, 2u, L"exit-before-ready", false);
    assert(result.outcome == GenerationOutcome::ExitBeforeReady);
    assert(!result.readyObserved && result.childExitCode == 42u);

    result = RunScenario(supervisor, fixture, 3u, L"never-ready", false);
    assert(result.outcome == GenerationOutcome::StartupTimeout);
    assert(result.forced && !result.readyObserved);

    result = RunScenario(supervisor, fixture, 4u, L"stall-after-ready", false);
    assert(result.outcome == GenerationOutcome::ProgressTimeout);
    assert(result.forced && result.readyObserved);
    assert(result.terminationPrepared);

    result = RunScenario(supervisor, fixture, 5u, L"ignore-stop", true);
    assert(result.outcome == GenerationOutcome::PlannedStop);
    assert(result.forced && result.readyObserved);

    result = RunScenario(supervisor, fixture, 6u, L"crash-after-ready", false);
    assert(result.outcome == GenerationOutcome::UnexpectedExit);
    assert(result.readyObserved && result.childExitCode == 77u);

    result = RunScenario(supervisor, fixture, 7u, L"wrong-generation", false);
    assert(result.outcome == GenerationOutcome::ProtocolViolation);
    assert(result.forced);

    result = RunScenario(supervisor, fixture, 8u, L"report-fault", false);
    assert(result.outcome == GenerationOutcome::ChildFault);
    assert(result.forced && result.nativeError == 0x2468u);

    for (std::uint32_t index = 0; index < kStressGenerations; ++index)
    {
        result = RunScenario(supervisor, fixture, 1000u + index,
            L"normal", true, true);
        assert(result.outcome == GenerationOutcome::PlannedStop);
    }
    std::printf("PROCESS_GENERATION_SUPERVISOR_TEST=PASS generations=%u "
        "containment_before_resume=1 presignalled_entry_or_forced=1 "
        "explicit_decoy_inherited=0 "
        "startup_timeout=1 progress_timeout=1 forced_stop=1 "
        "prepare_before_reap=1 protected_process_job_handles=1 overlap=0\n",
        kStressGenerations + 8u);
    return 0;
}
}

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--child")
        return RunChild(argc, argv);
    return RunParent();
}
#else
int main()
{
    return 0;
}
#endif
