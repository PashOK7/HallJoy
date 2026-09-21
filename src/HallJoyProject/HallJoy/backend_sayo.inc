#include "worker_join_policy.h"
#include "worker_exception_barrier.h"
#include "monotonic_time.h"
#include "sayo_o3c_protocol.h"
#include "native_layout_state.h"
#include "sayo_layout_state.h"
#include "generated/layout_pipeline/identities.h"

#include <chrono>
#include <mutex>
#include <stdexcept>

// ---- native HID path for SayoDevice O3C-compatible protocol ----
static constexpr USHORT kSayoVendorId = 0x8089;
static constexpr USHORT kSayoKnownProductId = 0x0009;
static constexpr size_t kSayoMaxDevices = 8;
static constexpr ULONGLONG kSayoReconnectIntervalMs = 2000;
static constexpr ULONGLONG kSayoNoPacketRestartMs = 1800;
static constexpr ULONGLONG kSayoPollIntervalMs = 8;
static constexpr ULONGLONG kSayoDepthFreshMs = 160;
static constexpr uint16_t kSayoRawFullScale = 4000;

struct SayoReader
{
    std::wstring path;
    HANDLE handle = nullptr;
    HANDLE thread = nullptr;
    uint16_t inputLen = 0;
    uint16_t outputLen = 0;
    uint16_t usagePage = 0;
    uint16_t usage = 0;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    bool writeCapable = false;
};

static std::array<SayoReader, kSayoMaxDevices> g_sayoReaders{};
static HANDLE                     g_sayoStopEvent = nullptr;
static std::atomic<bool>          g_sayoConnected{ false };
static std::atomic<uint16_t>      g_sayoConnectedVid{ 0 };
static std::atomic<uint16_t>      g_sayoConnectedPid{ 0 };
static std::atomic<ULONGLONG>     g_sayoLastPacketMs{ 0 };
static ULONGLONG                  g_sayoLastReconnectTryMs = 0;
namespace so3c = halljoy::sayo::o3c;
static std::array<std::atomic<uint16_t>,3> g_sayoPhysicalMilli{};
static std::array<std::atomic<uint16_t>,3> g_sayoIndexToHid{};
static auto& g_sayoLayoutToken=halljoy::sayo::layout::token;
static std::atomic<bool> g_sayoConfigRead{false};
static std::atomic<bool> g_sayoO3cCandidate{false};
static uint16_t SayoMappedHid(size_t index) noexcept
{
    return halljoy::native_layout::UsesRemapping(g_sayoLayoutToken.load(std::memory_order_acquire))
        ? static_cast<uint16_t>(halljoy::keycode::kO3cFirst+index) : so3c::Factory[index];
}
static bool SayoMatchesHid(size_t index,uint16_t hid) noexcept
{
    // Keep known logical aliases for existing profiles; new captures prefer
    // physical channels, which remain distinct even with duplicate labels.
    return so3c::Matches(index,hid,halljoy::sayo::layout::Active(),g_sayoIndexToHid[index].load());
}
static std::atomic<ULONGLONG>     g_sayoLastDepthMs{ 0 };
static std::atomic<uint64_t>      g_sayoLastDepthUs{ 0 };
static std::atomic<uint32_t>      g_sayoAvgDepthIntervalUs{ 0 };
static std::atomic<uint32_t>      g_sayoMaxDepthIntervalUs{ 0 };
static std::atomic<int>           g_sayoReaderCount{ 0 };
static std::atomic<uint64_t>      g_sayoPollAttempts{ 0 };
static std::atomic<uint64_t>      g_sayoPollSuccess{ 0 };
static std::atomic<uint64_t>      g_sayoPollFail{ 0 };
static std::atomic<uint64_t>      g_sayoDepthPackets{ 0 };
static std::atomic<uint32_t>      g_sayoMaxInputReportBytes{ 0 };
static std::atomic<uint32_t>      g_sayoMaxOutputReportBytes{ 0 };
static std::atomic<uint32_t>      g_sayoWriteCapableReaders{ 0 };
static constexpr uint32_t         kSayoObservedWordsPerKey = (kSayoRawFullScale + 64u) / 64u;
static std::array<std::array<std::atomic<uint64_t>, kSayoObservedWordsPerKey>, 16> g_sayoObservedDepthValues{};
static std::array<std::atomic<uint16_t>, 16> g_sayoObservedDepthCounts{};
static std::timed_mutex             g_sayoLifecycleMutex;
static halljoy::lifecycle::WorkerLifecycle g_sayoLifecycle;
static std::atomic<bool>            g_sayoRestartBlocked{ false };
static std::array<std::atomic<bool>, kSayoMaxDevices> g_sayoReaderExpected{};
static std::array<std::atomic<bool>, kSayoMaxDevices> g_sayoReaderExited{};
static std::array<std::atomic<halljoy::worker::WorkerExceptionKind>, kSayoMaxDevices>
    g_sayoReaderFaultKind{};
static std::array<halljoy::worker::WorkerExceptionRecord, kSayoMaxDevices>
    g_sayoReaderFaultRecord{};
static std::array<std::atomic<DWORD>, kSayoMaxDevices> g_sayoReaderSehCode{};
static std::atomic<bool> g_sayoReaderStartupPublishing{ false };

static void SayoRecordObservedDepth(uint8_t index, uint16_t raw)
{
    if (index >= g_sayoObservedDepthValues.size())
        return;
    const uint32_t value = std::min<uint32_t>(raw, kSayoRawFullScale);
    const uint32_t word = value / 64u;
    const uint64_t mask = 1ull << (value % 64u);
    const uint64_t old = g_sayoObservedDepthValues[index][word].fetch_or(mask, std::memory_order_relaxed);
    if ((old & mask) == 0)
        g_sayoObservedDepthCounts[index].fetch_add(1, std::memory_order_relaxed);
}

static void Sayo_ResetKeyState()
{
    halljoy::native_layout::Clear(g_sayoLayoutToken.exchange(0));
    g_sayoConfigRead.store(false);
    g_sayoO3cCandidate.store(false);
    for(auto& h:halljoy::sayo::layout::windowsHid) h.store(0);
    for (auto& m : g_sayoPhysicalMilli) m.store(0);
    for (auto& h : g_sayoIndexToHid) h.store(0);
    g_sayoLastDepthMs.store(0, std::memory_order_relaxed);
    g_sayoLastDepthUs.store(0, std::memory_order_relaxed);
    g_sayoAvgDepthIntervalUs.store(0, std::memory_order_relaxed);
    g_sayoMaxDepthIntervalUs.store(0, std::memory_order_relaxed);
    g_sayoLastPacketMs.store(0, std::memory_order_relaxed);
    g_sayoPollAttempts.store(0, std::memory_order_relaxed);
    g_sayoPollSuccess.store(0, std::memory_order_relaxed);
    g_sayoPollFail.store(0, std::memory_order_relaxed);
    g_sayoDepthPackets.store(0, std::memory_order_relaxed);
    g_sayoMaxInputReportBytes.store(0, std::memory_order_relaxed);
    g_sayoMaxOutputReportBytes.store(0, std::memory_order_relaxed);
    g_sayoWriteCapableReaders.store(0, std::memory_order_relaxed);
    for (auto& keyWords : g_sayoObservedDepthValues)
        for (auto& word : keyWords)
            word.store(0, std::memory_order_relaxed);
    for (auto& count : g_sayoObservedDepthCounts)
        count.store(0, std::memory_order_relaxed);
    for (size_t i = 0; i < g_sayoReaderExpected.size(); ++i)
    {
        g_sayoReaderExpected[i].store(false, std::memory_order_relaxed);
        g_sayoReaderExited[i].store(true, std::memory_order_relaxed);
        g_sayoReaderFaultKind[i].store(
            halljoy::worker::WorkerExceptionKind::None, std::memory_order_relaxed);
        g_sayoReaderFaultRecord[i] = {};
        g_sayoReaderSehCode[i].store(0, std::memory_order_relaxed);
    }
    g_sayoReaderStartupPublishing.store(false, std::memory_order_relaxed);


}

static bool SayoReleasePublishedInput() noexcept
{
    bool releasedAny = false;
    for (auto& value : g_sayoPhysicalMilli)
    {
        if (value.exchange(0, std::memory_order_relaxed) != 0)
            releasedAny = true;
    }
    halljoy::native_layout::Clear(g_sayoLayoutToken.exchange(0));
    g_sayoConnected.store(false, std::memory_order_release);
    g_sayoLastPacketMs.store(0, std::memory_order_relaxed);
    if (releasedAny)
        RealtimeLoop_NotifyInputChanged();
    return releasedAny;
}

static void SayoRecordDepthTiming()
{
    uint64_t nowUs = SparkNowUs();
    uint64_t prevUs = g_sayoLastDepthUs.exchange(nowUs, std::memory_order_relaxed);
    if (prevUs == 0 || nowUs <= prevUs)
        return;

    uint32_t intervalUs = (uint32_t)std::min<uint64_t>(nowUs - prevUs, 60ull * 1000ull * 1000ull);
    uint32_t avg = g_sayoAvgDepthIntervalUs.load(std::memory_order_relaxed);
    if (avg == 0)
        avg = intervalUs;
    else if (intervalUs > avg)
        avg += (intervalUs - avg + 3u) / 4u;
    else
        avg -= (avg - intervalUs + 3u) / 4u;
    g_sayoAvgDepthIntervalUs.store(avg, std::memory_order_relaxed);
    SparkAtomicMax(g_sayoMaxDepthIntervalUs, intervalUs);
}

static void SayoParseDepthResponse(const uint8_t* data, DWORD len, ULONGLONG nowMs)
{
    so3c::Frame frame;
    std::array<uint16_t,3> raw{};
    if (!so3c::Parse(data,len,frame) || !so3c::Depth(frame,raw)) return;
    // Exact known model plus validated three-channel travel establishes geometry,
    // independently of optional configuration reads.
    if(g_sayoO3cCandidate.load() && !g_sayoLayoutToken.load() &&
        WaitForSingleObject(g_sayoStopEvent,0)!=WAIT_OBJECT_0) {
        std::array<uint16_t,3> bindings{};
        for(size_t i=0;i<3;++i) bindings[i]=g_sayoIndexToHid[i].load();
        const auto keys=so3c::Layout(bindings);
        const auto token=halljoy::layout_identity::Token("sayo-o3c","O3C-8089-0009");
        if(halljoy::native_layout::Publish(token,keys.data(),keys.size()))
            g_sayoLayoutToken.store(token,std::memory_order_release);
    }
    for (size_t i=0;i<raw.size();++i) {
        SayoRecordObservedDepth(static_cast<uint8_t>(i),raw[i]);
        g_sayoPhysicalMilli[i].store(so3c::Normalize(raw[i]),std::memory_order_relaxed);
    }
    g_sayoLastDepthMs.store(nowMs,std::memory_order_release);
    g_sayoDepthPackets.fetch_add(1,std::memory_order_relaxed);
    SayoRecordDepthTiming();
    RealtimeLoop_NotifyInputChanged();
}

// Startup-only configuration read on the same worker/handle as depth polling.
// No timer-driven layout rebuild and no device configuration writes.
static void SayoReadConfiguration(SayoReader* reader)
{
    if (reader->productId!=kSayoKnownProductId || reader->usagePage!=0xFF12 ||
        reader->usage!=2 || !reader->writeCapable || reader->outputLen!=1024 || reader->inputLen<64) return;
    std::vector<uint8_t> response(reader->inputLen,0);
    auto exchange=[&](uint8_t command,uint8_t index,so3c::Frame& frame) {
        const auto request=so3c::Read(command,index);
        for (unsigned attempt=0;attempt<2;++attempt) {
            if (WaitForSingleObject(g_sayoStopEvent,0)==WAIT_OBJECT_0) return false;
            DWORD error=0;
            if (!SparkWriteFileOverlapped(reader->handle,request.data(),1024,20,&error)) continue;
            const auto deadline=GetTickCount64()+100;
            while (GetTickCount64()<deadline && WaitForSingleObject(g_sayoStopEvent,0)!=WAIT_OBJECT_0) {
                DWORD got=0;
                if (!SparkReadFileOverlapped(reader->handle,response.data(),reader->inputLen,20,&got,&error)) {
                    if(error==WAIT_TIMEOUT || error==ERROR_OPERATION_ABORTED) continue;
                    return false;
                }
                if(so3c::Parse(response.data(),got,frame) && frame.echo==so3c::ConfigEcho &&
                    frame.command==command && frame.index==index) return true;
            }
        }
        return false;
    };
    g_sayoO3cCandidate.store(true);
    so3c::Frame frame;
    if (!exchange(0,0,frame)) {
        DebugLog_Write(L"[backend.sayo] config unavailable: identity read timeout; three physical keys retained after travel proof");
        return;
    }
    if (!so3c::Identity(frame)) {
        g_sayoO3cCandidate.store(false);
        DebugLog_Write(L"[backend.sayo] model identity mismatch; O3C auto layout not claimed");
        return;
    }
    unsigned readCount=0;
    for (uint8_t i=0;i<3;++i) {
        uint16_t hid=0;
        if (!exchange(0x10,i,frame) || !so3c::Binding(frame,i,hid)) {
            DebugLog_Write(L"[backend.sayo] key=%u binding unavailable; physical key retained",unsigned(i));
            continue;
        }
        g_sayoIndexToHid[i].store(hid,std::memory_order_relaxed);
        halljoy::sayo::layout::windowsHid[i].store(hid,std::memory_order_relaxed);
        ++readCount;
    }
    g_sayoConfigRead.store(readCount!=0,std::memory_order_release);
    DebugLog_Write(L"[backend.sayo] config read: %u of 3 assignments; all three physical keys retained",readCount);

}

static bool SayoSendDepthPoll(SayoReader* reader)
{
    if (!reader || !reader->handle || !reader->writeCapable || reader->outputLen < 1024)
        return false;

    DWORD len = (DWORD)reader->outputLen;
    std::vector<uint8_t> cmd(len, 0);
    static constexpr uint8_t kPollPrefix[] = {
        0x22, 0x12, 0x3C, 0x13, 0x05, 0x00, 0x15, 0x01
    };
    memcpy(cmd.data(), kPollPrefix, sizeof(kPollPrefix));

    DWORD err = ERROR_SUCCESS;
    return SparkWriteFileOverlapped(reader->handle, cmd.data(), len, 20, &err);
}

static bool SayoProbeDepthProtocol(SayoReader* reader)
{
    if (!reader || !reader->handle || !reader->writeCapable ||
        reader->inputLen < 14 || reader->outputLen < 1024)
        return false;

    const DWORD writeLen = static_cast<DWORD>(reader->outputLen);
    std::vector<uint8_t> cmd(writeLen, 0);
    static constexpr uint8_t kPollPrefix[] = {
        0x22, 0x12, 0x3C, 0x13, 0x05, 0x00, 0x15, 0x01
    };
    memcpy(cmd.data(), kPollPrefix, sizeof(kPollPrefix));
    DWORD error = ERROR_SUCCESS;
    if (!SparkWriteFileOverlapped(reader->handle, cmd.data(), writeLen, 30, &error))
        return false;

    std::vector<uint8_t> response(std::max<DWORD>(reader->inputLen, 14u), 0);
    const ULONGLONG deadline = GetTickCount64() + 250u;
    while (GetTickCount64() < deadline)
    {
        DWORD got = 0;
        error = ERROR_SUCCESS;
        if (!SparkReadFileOverlapped(reader->handle, response.data(),
                static_cast<DWORD>(response.size()), 25, &got, &error))
        {
            if (error == WAIT_TIMEOUT || error == ERROR_OPERATION_ABORTED)
                continue;
            return false;
        }
        so3c::Frame frame;
        std::array<uint16_t,3> raw{};
        if(so3c::Parse(response.data(),got,frame) && so3c::Depth(frame,raw)) return true;
    }
    return false;
}

static DWORD SayoReaderThreadBody(SayoReader* reader)
{
#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    if (commandLine && wcsstr(commandLine, L"--halljoy-test-sayo-stop-timeout"))
    {
        StabilityTrace_Write(L"WARN", L"sayo", L"test.stop_timeout.injected",
            L"simulator_only=1");
        Sleep(INFINITE);
    }
    if (commandLine && wcsstr(commandLine, L"--halljoy-test-sayo-reader-cpp-fault"))
        throw std::runtime_error("injected Sayo reader C++ fault");
#endif

    if (!reader || !reader->handle || !g_sayoStopEvent)
        return 0;

    const DWORD readLen = std::max<DWORD>(reader->inputLen, 8);
    std::vector<uint8_t> buf(readLen, 0);
    DebugLog_Write(
        L"[backend.sayo] reader start in_len=%u usage_page=0x%04X usage=0x%04X",
        (unsigned)reader->inputLen,
        (unsigned)reader->usagePage,
        (unsigned)reader->usage);

    SayoReadConfiguration(reader);
    ULONGLONG lastPollMs = 0;
    int pollFailStreak = 0;
    while (WaitForSingleObject(g_sayoStopEvent, 0) != WAIT_OBJECT_0)
    {
        ULONGLONG loopMs = GetTickCount64();
        if (reader->writeCapable && reader->outputLen >= 1024 && loopMs - lastPollMs >= kSayoPollIntervalMs)
        {
            lastPollMs = loopMs;
            g_sayoPollAttempts.fetch_add(1, std::memory_order_relaxed);
            if (SayoSendDepthPoll(reader))
            {
                g_sayoPollSuccess.fetch_add(1, std::memory_order_relaxed);
                pollFailStreak = 0;
            }
            else
            {
                g_sayoPollFail.fetch_add(1, std::memory_order_relaxed);
                if (++pollFailStreak == 5)
                    DebugLog_Write(L"[backend.sayo] depth poll writes failing");
            }
        }

        DWORD got = 0;
        DWORD err = ERROR_SUCCESS;
        if (!SparkReadFileOverlapped(reader->handle, buf.data(), readLen, 25, &got, &err))
        {
            if (err == WAIT_TIMEOUT || err == ERROR_OPERATION_ABORTED)
                continue;
            DebugLog_Write(L"[backend.sayo] read failed err=%lu", err);
            break;
        }
        if (got == 0)
            continue;

        ULONGLONG nowMs = GetTickCount64();
        g_sayoLastPacketMs.store(nowMs, std::memory_order_relaxed);
        if (buf[0]==0x22) SayoParseDepthResponse(buf.data(),got,nowMs);
    }

    DebugLog_Write(L"[backend.sayo] reader stop");
    return 0;
}

static size_t SayoReaderIndex(const SayoReader* reader) noexcept
{
    const auto address = reinterpret_cast<std::uintptr_t>(reader);
    const auto base = reinterpret_cast<std::uintptr_t>(g_sayoReaders.data());
    const auto bytes = sizeof(SayoReader) * g_sayoReaders.size();
    if (!reader || address < base || address >= base + bytes ||
        (address - base) % sizeof(SayoReader) != 0)
        return g_sayoReaders.size();
    return static_cast<size_t>((address - base) / sizeof(SayoReader));
}

static int SayoPublishReaderCompletion(size_t index) noexcept
{
    if (index >= g_sayoReaderExited.size())
        return g_sayoReaderCount.load(std::memory_order_relaxed);

    g_sayoReaderExited[index].store(true, std::memory_order_release);
    int liveReaders = 0;
    bool anyExpected = false;
    for (size_t i = 0; i < g_sayoReaderExpected.size(); ++i)
    {
        if (!g_sayoReaderExpected[i].load(std::memory_order_acquire))
            continue;
        anyExpected = true;
        if (!g_sayoReaderExited[i].load(std::memory_order_acquire))
            ++liveReaders;
    }
    g_sayoReaderCount.store(liveReaders, std::memory_order_release);
    if (anyExpected && liveReaders == 0 &&
        !g_sayoReaderStartupPublishing.load(std::memory_order_acquire))
    {
        const bool releasedAny = SayoReleasePublishedInput();
        StabilityTrace_WriteCritical(L"WARN", L"sayo", L"reader_group.exit",
            L"all_readers_exited=1 neutralized=1 released_any=%d",
            releasedAny ? 1 : 0);
    }
    return liveReaders;
}

static void SayoReaderOnCppFault(
    size_t index,
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    if (index < g_sayoReaderFaultRecord.size())
    {
        g_sayoReaderFaultRecord[index] = record;
        g_sayoReaderFaultKind[index].store(record.kind, std::memory_order_release);
    }
    const bool releasedAny = SayoReleasePublishedInput();
    if (g_sayoStopEvent)
        SetEvent(g_sayoStopEvent);
    StabilityTrace_WriteCritical(L"ERROR", L"sayo", L"worker.fault",
        L"reader=%llu kind=%u neutralized=1 released_any=%d group_stop_signaled=1",
        static_cast<unsigned long long>(index),
        static_cast<unsigned>(record.kind), releasedAny ? 1 : 0);
    OutputDebugStringA("[HallJoy] Sayo reader exception: ");
    OutputDebugStringA(record.message[0] ? record.message : "unknown");
    OutputDebugStringA("\n");
}

static void SayoReaderOnCppCompletion(
    size_t index,
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    const int liveReaders = SayoPublishReaderCompletion(index);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"sayo", L"worker.exit", L"reader=%llu fault_kind=%u live_readers=%d",
        static_cast<unsigned long long>(index),
        static_cast<unsigned>(record.kind), liveReaders);
}

static DWORD SayoReaderThreadProcCpp(SayoReader* reader, size_t index) noexcept
{
    return static_cast<DWORD>(halljoy::worker::RunWorkerEntryBarrier(
        [reader] { return static_cast<std::uint32_t>(SayoReaderThreadBody(reader)); },
        [index](const halljoy::worker::WorkerExceptionRecord& record) noexcept {
            SayoReaderOnCppFault(index, record);
        },
        [index](const halljoy::worker::WorkerExceptionRecord& record) noexcept {
            SayoReaderOnCppCompletion(index, record);
        },
        0xE0520005u));
}

static void SayoReaderOnStructuredFault(size_t index, DWORD code) noexcept
{
    halljoy::worker::WorkerExceptionRecord record{};
    record.kind = halljoy::worker::WorkerExceptionKind::UnknownException;
    halljoy::worker::CopyWorkerExceptionMessage(record, "structured exception");
    if (index < g_sayoReaderSehCode.size())
        g_sayoReaderSehCode[index].store(code, std::memory_order_release);
    SayoReaderOnCppFault(index, record);
    SayoReaderOnCppCompletion(index, record);
    StabilityTrace_WriteCritical(L"ERROR", L"sayo", L"worker.seh",
        L"reader=%llu code=0x%08lX", static_cast<unsigned long long>(index), code);
}

static DWORD WINAPI SayoReaderThreadProc(LPVOID param) noexcept
{
    auto* reader = static_cast<SayoReader*>(param);
    const size_t index = SayoReaderIndex(reader);
    StabilityTrace_Write(L"INFO", L"sayo", L"worker.start",
        L"reader=%llu", static_cast<unsigned long long>(index));
    __try
    {
        return SayoReaderThreadProcCpp(reader, index);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        const DWORD code = GetExceptionCode();
        SayoReaderOnStructuredFault(index, code);
        OutputDebugStringA("[HallJoy] Sayo reader structured exception\n");
        return 0;
    }
}

static bool SayoCollectMatchingDevices(std::vector<SayoReader>* outReaders)
{
    if (!outReaders)
        return false;
    outReaders->clear();

    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO devInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE)
        return false;

    for (DWORD idx = 0; outReaders->size() < kSayoMaxDevices; ++idx)
    {
        SP_DEVICE_INTERFACE_DATA ifData{};
        ifData.cbSize = sizeof(ifData);
        if (!SetupDiEnumDeviceInterfaces(devInfo, nullptr, &hidGuid, idx, &ifData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }

        DWORD detailBytes = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, nullptr, 0, &detailBytes, nullptr);
        if (detailBytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
            continue;

        std::vector<uint8_t> detailBuf((size_t)detailBytes, 0);
        auto* detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)detailBuf.data();
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, detail, detailBytes, nullptr, nullptr))
            continue;

        const bool routedToSayo = NativeAnalogRouting_IsClaimedBy(
            detail->DevicePath, NativeAnalogProtocol::SayoDepth);
        if (NativeAnalogRouting_IsClaimed(detail->DevicePath) && !routedToSayo)
            continue;
        if (NativeAnalogRouting_ProtocolHasClaims(NativeAnalogProtocol::SayoDepth) &&
            !routedToSayo)
            continue;

        bool writeCapable = true;
        HANDLE h = CreateFileW(
            detail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
            nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            writeCapable = false;
            h = CreateFileW(
                detail->DevicePath,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                nullptr);
        }
        if (h == INVALID_HANDLE_VALUE)
            continue;

        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        if (!HidD_GetAttributes(h, &attr) ||
            attr.VendorID != kSayoVendorId)
        {
            CloseHandle(h);
            continue;
        }
        uint16_t inLen = 0, outLen = 0, usagePage = 0, usage = 0;
        if (!SparkReadCaps(h, &inLen, &outLen, &usagePage, &usage) || inLen < 8)
        {
            CloseHandle(h);
            continue;
        }

        if (!HidD_SetNumInputBuffers(h, 64))
            DebugLog_Write(L"[backend.sayo] input-buffer tuning unavailable err=%lu", GetLastError());
        SayoReader reader{};
        reader.path = detail->DevicePath;
        reader.handle = h;
        reader.inputLen = inLen;
        reader.outputLen = outLen;
        reader.usagePage = usagePage;
        reader.usage = usage;
        reader.vendorId = attr.VendorID;
        reader.productId = attr.ProductID;
        reader.writeCapable = writeCapable;
        outReaders->push_back(reader);

        DebugLog_Write(
            L"[backend.sayo] candidate path=%s vid=%04X pid=%04X write_capable=%d in_len=%u out_len=%u usage_page=0x%04X usage=0x%04X",
            detail->DevicePath, (unsigned)attr.VendorID, (unsigned)attr.ProductID,
            writeCapable ? 1 : 0,
            (unsigned)inLen,
            (unsigned)outLen,
            (unsigned)usagePage,
            (unsigned)usage);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    if (outReaders->empty())
        return false;

    std::vector<uint16_t> products;
    for (const auto& reader : *outReaders)
        if (std::find(products.begin(), products.end(), reader.productId) == products.end())
            products.push_back(reader.productId);

    uint16_t selectedPid = 0;
    bool selected = false;

    // Preserve the established O3C route when it is present. Only when the
    // audited PID is absent do we send the existing read-only depth poll to
    // another Sayo PID and require a valid 0x22 response.
    if (std::find(products.begin(), products.end(), kSayoKnownProductId) != products.end())
    {
        selectedPid = kSayoKnownProductId;
        selected = true;
    }
    else
    {
        for (uint16_t pid : products)
        {
            for (auto& reader : *outReaders)
            {
                if (reader.productId == pid && SayoProbeDepthProtocol(&reader))
                {
                    selectedPid = pid;
                    selected = true;
                    break;
                }
            }
            if (selected)
                break;
        }
    }

    if (!selected)
    {
        for (auto& reader : *outReaders)
            if (reader.handle) CloseHandle(reader.handle);
        outReaders->clear();
        return false;
    }

    bool keptDepthReader=false;
    for (auto it=outReaders->begin();it!=outReaders->end();) {
        const bool keep=!keptDepthReader && it->productId==selectedPid && it->usagePage==0xFF12 &&
            it->usage==2 && it->writeCapable && it->outputLen==1024 && it->inputLen>=14;
        if (!keep) {
            if (it->handle) CloseHandle(it->handle);
            it=outReaders->erase(it);
        } else {keptDepthReader=true;++it;}
    }
    return !outReaders->empty();
}

static halljoy::lifecycle::StopResult SayoStopLocked()
{
    const auto lifecycleState = g_sayoLifecycle.State();
    if (lifecycleState == halljoy::lifecycle::WorkerState::Stopped ||
        lifecycleState == halljoy::lifecycle::WorkerState::Joined)
    {
        return g_sayoLifecycle.RequestStop();
    }
    if (lifecycleState == halljoy::lifecycle::WorkerState::Poisoned)
    {
        return g_sayoLifecycle.RequestStop(g_sayoLifecycle.Generation());
    }
    const auto requested = g_sayoLifecycle.RequestStop(g_sayoLifecycle.Generation());
    if (requested.status != halljoy::lifecycle::StopStatus::StopRequested)
        return requested;

    const bool releasedBeforeJoin = SayoReleasePublishedInput();
    if (g_sayoStopEvent)
        SetEvent(g_sayoStopEvent);

    std::array<HANDLE, kSayoMaxDevices> threadHandles{};
    DWORD threadCount = 0;
    for (auto& reader : g_sayoReaders)
    {
        if (reader.handle)
            CancelIoEx(reader.handle, nullptr);
        if (reader.thread)
            threadHandles[threadCount++] = reader.thread;
    }

    if (threadCount == 0)
    {
        g_sayoRestartBlocked.store(true, std::memory_order_release);
        const auto poisoned = g_sayoLifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed,
            ERROR_INVALID_HANDLE);
        StabilityTrace_WriteCritical(L"ERROR", L"sayo", L"stop.invalid_threads",
            L"generation=%llu reader_group_retained=1 stop_event_retained=%d restart_blocked=1",
            static_cast<unsigned long long>(requested.generation.Value()),
            g_sayoStopEvent ? 1 : 0);
        return poisoned;
    }

    const DWORD waitResult = WaitForMultipleObjects(
        threadCount, threadHandles.data(), TRUE, 3000);
    const bool allJoined = waitResult >= WAIT_OBJECT_0 &&
        waitResult < WAIT_OBJECT_0 + threadCount;
    const DWORD waitError = allJoined
        ? ERROR_SUCCESS
        : (waitResult == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT);
    const auto observedJoin = halljoy::lifecycle::ObserveWorkerJoin(
        requested.generation,
        allJoined
            ? halljoy::lifecycle::JoinWaitStatus::Joined
            : (waitResult == WAIT_FAILED
                ? halljoy::lifecycle::JoinWaitStatus::Failed
                : halljoy::lifecycle::JoinWaitStatus::TimedOut),
        waitError);
    if (!observedJoin.Completed())
    {
        bool hidHandlesRetained = false;
        for (const auto& reader : g_sayoReaders)
            hidHandlesRetained = hidHandlesRetained || reader.handle != nullptr;
        g_sayoRestartBlocked.store(true, std::memory_order_release);
        const auto poisoned = g_sayoLifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            observedJoin.error.code, observedJoin.error.native_error);
        DebugLog_Write(L"[backend.sayo] reader group join incomplete wait=%lu err=%lu; resources retained and restart blocked",
            static_cast<unsigned long>(waitResult), static_cast<unsigned long>(waitError));
        StabilityTrace_WriteCritical(L"ERROR", L"sayo", L"stop.timeout",
            L"generation=%llu readers=%lu wait=%lu win32=%lu reader_group_retained=1 hid_handles_retained=%d stop_event_retained=1 restart_blocked=1",
            static_cast<unsigned long long>(requested.generation.Value()),
            static_cast<unsigned long>(threadCount),
            static_cast<unsigned long>(waitResult),
            static_cast<unsigned long>(waitError), hidHandlesRetained ? 1 : 0);
        return poisoned;
    }

    for (auto& reader : g_sayoReaders)
    {
        if (reader.thread)
            CloseHandle(reader.thread);
        if (reader.handle)
            CloseHandle(reader.handle);
        reader = SayoReader{};
    }

    if (g_sayoStopEvent)
    {
        CloseHandle(g_sayoStopEvent);
        g_sayoStopEvent = nullptr;
    }

    g_sayoReaderCount.store(0, std::memory_order_relaxed);
    g_sayoConnectedVid.store(0, std::memory_order_relaxed);
    g_sayoConnectedPid.store(0, std::memory_order_relaxed);
    const bool releasedAfterJoin = SayoReleasePublishedInput();
    StabilityTrace_Write(L"INFO", L"sayo", L"neutralized",
        L"released_before_join=%d released_after_join=%d",
        releasedBeforeJoin ? 1 : 0, releasedAfterJoin ? 1 : 0);
    return g_sayoLifecycle.ConfirmJoined(requested.generation);
}

static halljoy::lifecycle::StopResult SayoStop()
{
    std::unique_lock<std::timed_mutex> lifecycleGuard(g_sayoLifecycleMutex, std::defer_lock);
    if (!lifecycleGuard.try_lock_for(std::chrono::milliseconds(500)))
    {
        g_sayoRestartBlocked.store(true, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"sayo", L"stop.lock_timeout",
            L"wait_ms=500 lifecycle_lock_retained=1 restart_blocked=1");
        return {
            halljoy::lifecycle::StopStatus::TimedOut,
            halljoy::lifecycle::WorkerState::Poisoned,
            {},
            { halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,
                halljoy::lifecycle::LifecycleOperation::RequestStop,
                halljoy::lifecycle::WorkerState::Running,
                halljoy::lifecycle::WorkerState::Poisoned,
                {}, {}, ERROR_TIMEOUT }
        };
    }
    return SayoStopLocked();
}

static bool SayoStart()
{
    if (g_sayoRestartBlocked.load(std::memory_order_acquire))
        return false;
    const std::lock_guard<std::timed_mutex> lifecycleGuard(g_sayoLifecycleMutex);
    if (g_sayoRestartBlocked.load(std::memory_order_acquire))
        return false;

    if (g_sayoConnected.load(std::memory_order_acquire))
        return true;
    if (g_sayoStopEvent && !SayoStopLocked().RestartSafe())
        return false;

    const auto start = g_sayoLifecycle.BeginStart();
    if (start.status != halljoy::lifecycle::StartStatus::Starting)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"sayo", L"start.rejected",
            L"state=%u generation=%llu error=%u restart_blocked=1",
            static_cast<unsigned>(start.state),
            static_cast<unsigned long long>(start.generation.Value()),
            static_cast<unsigned>(start.error.code));
        return false;
    }

#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    const bool injectReaderCppFault = commandLine &&
        wcsstr(commandLine, L"--halljoy-test-sayo-reader-cpp-fault");
    if (commandLine &&
        (wcsstr(commandLine, L"--halljoy-test-sayo-stop-timeout") ||
         injectReaderCppFault))
    {
        g_sayoStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!g_sayoStopEvent)
        {
            const DWORD error = GetLastError();
            (void)g_sayoLifecycle.FailStartBeforeWorker(start.generation, error);
            return false;
        }
        Sayo_ResetKeyState();
        g_sayoReaderStartupPublishing.store(true, std::memory_order_release);
        g_sayoReaderExpected[0].store(true, std::memory_order_release);
        g_sayoReaderExited[0].store(false, std::memory_order_release);
        DWORD tid = 0;
        g_sayoReaders[0].thread = CreateThread(
            nullptr, 0, SayoReaderThreadProc, &g_sayoReaders[0], 0, &tid);
        if (!g_sayoReaders[0].thread)
        {
            const DWORD error = GetLastError();
            g_sayoReaderExpected[0].store(false, std::memory_order_release);
            g_sayoReaderExited[0].store(true, std::memory_order_release);
            g_sayoReaderStartupPublishing.store(false, std::memory_order_release);
            CloseHandle(g_sayoStopEvent);
            g_sayoStopEvent = nullptr;
            (void)g_sayoLifecycle.FailStartBeforeWorker(start.generation, error);
            return false;
        }
        if (injectReaderCppFault)
            (void)WaitForSingleObject(g_sayoReaders[0].thread, 1000);
        g_sayoReaderStartupPublishing.store(false, std::memory_order_release);
        if (g_sayoReaderExited[0].load(std::memory_order_acquire) ||
            g_sayoReaderFaultKind[0].load(std::memory_order_acquire) !=
                halljoy::worker::WorkerExceptionKind::None)
        {
            StabilityTrace_WriteCritical(L"ERROR", L"sayo", L"start.failed",
                L"stage=reader_early_exit live_readers=0 faulted=1");
            (void)SayoStopLocked();
            return false;
        }
        const auto running = g_sayoLifecycle.ConfirmRunning(start.generation);
        if (!running.IsRunning())
            return false;
        g_sayoReaderCount.store(1, std::memory_order_relaxed);
        g_sayoConnectedVid.store(0xFFFF, std::memory_order_relaxed);
        g_sayoConnectedPid.store(0x0002, std::memory_order_relaxed);
        g_sayoConnected.store(true, std::memory_order_release);
        StabilityTrace_Write(L"WARN", L"sayo", L"test.start",
            L"simulator_only=1 generation=%llu readers=1",
            static_cast<unsigned long long>(start.generation.Value()));
        return true;
    }
#endif

    std::vector<SayoReader> readers;
    if (!SayoCollectMatchingDevices(&readers))
    {
        (void)g_sayoLifecycle.FailStartBeforeWorker(start.generation);
        return false;
    }
    if (g_sayoRestartBlocked.load(std::memory_order_acquire))
    {
        for (auto& reader : readers)
            if (reader.handle) CloseHandle(reader.handle);
        (void)g_sayoLifecycle.FailStartBeforeWorker(start.generation, ERROR_CANCELLED);
        return false;
    }

    g_sayoStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_sayoStopEvent)
    {
        const DWORD error = GetLastError();
        for (auto& r : readers)
            if (r.handle) CloseHandle(r.handle);
        (void)g_sayoLifecycle.FailStartBeforeWorker(start.generation, error);
        return false;
    }

    Sayo_ResetKeyState();
    uint32_t maxInputBytes = 0;
    uint32_t maxOutputBytes = 0;
    uint32_t writeCapableReaders = 0;
    for (const auto& reader : readers)
    {
        maxInputBytes = std::max<uint32_t>(maxInputBytes, reader.inputLen);
        maxOutputBytes = std::max<uint32_t>(maxOutputBytes, reader.outputLen);
        if (reader.writeCapable)
            ++writeCapableReaders;
    }
    g_sayoMaxInputReportBytes.store(maxInputBytes, std::memory_order_relaxed);
    g_sayoMaxOutputReportBytes.store(maxOutputBytes, std::memory_order_relaxed);
    g_sayoWriteCapableReaders.store(writeCapableReaders, std::memory_order_relaxed);

    int started = 0;
    g_sayoReaderStartupPublishing.store(true, std::memory_order_release);
    for (size_t i = 0; i < readers.size() && i < g_sayoReaders.size(); ++i)
    {
        g_sayoReaders[i] = readers[i];
        g_sayoReaderExpected[i].store(true, std::memory_order_release);
        g_sayoReaderExited[i].store(false, std::memory_order_release);
        DWORD tid = 0;
        g_sayoReaders[i].thread = CreateThread(nullptr, 0, SayoReaderThreadProc, &g_sayoReaders[i], 0, &tid);
        if (!g_sayoReaders[i].thread)
        {
            g_sayoReaderExpected[i].store(false, std::memory_order_release);
            g_sayoReaderExited[i].store(true, std::memory_order_release);
            CloseHandle(g_sayoReaders[i].handle);
            g_sayoReaders[i] = SayoReader{};
            continue;
        }
        ++started;
    }
    g_sayoReaderStartupPublishing.store(false, std::memory_order_release);

    if (started == 0)
    {
        CloseHandle(g_sayoStopEvent);
        g_sayoStopEvent = nullptr;
        (void)g_sayoLifecycle.FailStartBeforeWorker(start.generation, ERROR_NOT_FOUND);
        return false;
    }
    int liveReaders = 0;
    bool readerFaulted = false;
    for (size_t i = 0; i < g_sayoReaderExpected.size(); ++i)
    {
        if (!g_sayoReaderExpected[i].load(std::memory_order_acquire))
            continue;
        if (!g_sayoReaderExited[i].load(std::memory_order_acquire))
            ++liveReaders;
        readerFaulted = readerFaulted ||
            g_sayoReaderFaultKind[i].load(std::memory_order_acquire) !=
                halljoy::worker::WorkerExceptionKind::None;
    }
    if (liveReaders == 0 || readerFaulted)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"sayo", L"start.failed",
            L"stage=reader_early_exit live_readers=%d faulted=%d",
            liveReaders, readerFaulted ? 1 : 0);
        (void)SayoStopLocked();
        return false;
    }
    const bool claimedAllReaders = std::all_of(readers.begin(), readers.end(), [](const auto& reader) {
        return NativeAnalogRouting_Claim(
            reader.vendorId, reader.productId, reader.path.c_str(),
            NativeAnalogProtocol::SayoDepth);
    });
    if (!claimedAllReaders)
    {
        (void)SayoStopLocked();
        return false;
    }

    if (g_sayoRestartBlocked.load(std::memory_order_acquire))
    {
        (void)SayoStopLocked();
        return false;
    }

    g_sayoReaderCount.store(liveReaders, std::memory_order_relaxed);
    g_sayoLastReconnectTryMs = GetTickCount64();
    g_sayoConnectedVid.store(readers.front().vendorId, std::memory_order_relaxed);
    g_sayoConnectedPid.store(readers.front().productId, std::memory_order_relaxed);
    g_sayoConnected.store(true, std::memory_order_release);
    const auto running = g_sayoLifecycle.ConfirmRunning(start.generation);
    if (!running.IsRunning())
        return false;
    readerFaulted = false;
    for (size_t i = 0; i < g_sayoReaderExpected.size(); ++i)
    {
        readerFaulted = readerFaulted ||
            g_sayoReaderFaultKind[i].load(std::memory_order_acquire) !=
                halljoy::worker::WorkerExceptionKind::None;
    }
    if (g_sayoReaderCount.load(std::memory_order_acquire) == 0 || readerFaulted)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"sayo", L"start.failed",
            L"stage=publish_race live_readers=%d faulted=%d",
            g_sayoReaderCount.load(std::memory_order_relaxed), readerFaulted ? 1 : 0);
        (void)SayoStopLocked();
        return false;
    }
    DebugLog_Write(L"[backend.sayo] connected readers=%d", liveReaders);
    return true;
}

static bool SayoStartService()
{
    // Keep the outer registry generation alive even when no device is present
    // initially, so a later hotplug reader group always receives final stop.
    (void)SayoStart();
    return true;
}

static void SayoTickHotplug(ULONGLONG nowMs)
{
    if (g_sayoConnected.load(std::memory_order_acquire))
    {
        ULONGLONG lastPacket = g_sayoLastPacketMs.load(std::memory_order_relaxed);
        const ULONGLONG silenceMs = halljoy::monotonic_time::SaturatingAgeMs(nowMs, lastPacket);
        if (halljoy::monotonic_time::IsStale(nowMs, lastPacket, kSayoNoPacketRestartMs))
        {
            DebugLog_Write(
                L"[backend.sayo] no packets for %llu ms; restarting",
                (unsigned long long)silenceMs);
            SayoStop();
        }
        return;
    }

    if (nowMs - g_sayoLastReconnectTryMs < kSayoReconnectIntervalMs)
        return;
    g_sayoLastReconnectTryMs = nowMs;
    if (SayoStart())
        DebugLog_Write(L"[backend.sayo] hotplug reconnect ok");
}
