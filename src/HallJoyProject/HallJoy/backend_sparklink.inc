#include "worker_exception_barrier.h"
#include "worker_join_policy.h"

#include <chrono>
#include <mutex>

// ---- native HID path for SparkLink / XD keyboard protocol ----
static constexpr uint16_t kSparkUsagePageV2 = 0xFFB0;
static constexpr uint16_t kSparkUsagePageLegacy = 0xFFA0;
static constexpr uint16_t kSparkKnownUsage = 0x0001;
static constexpr size_t kSparkPayloadSize = 64;
static constexpr int kSparkMaxRows = 8;
static constexpr int kSparkColsPerRow = 21;
static constexpr ULONGLONG kSparkReconnectIntervalMs = 2000;
static constexpr ULONGLONG kSparkNoPacketRestartMs = 1800;
static constexpr uint16_t kSparkObservedRouteRawDefault = 3500;
static constexpr UINT kSparkMinRoutePollIntervalMs = 1;

static HANDLE                     g_sparkHandle = nullptr;
static HANDLE                     g_sparkThread = nullptr;
static HANDLE                     g_sparkStopEvent = nullptr;
static std::atomic<bool>          g_sparkConnected{ false };
static std::atomic<uint16_t>      g_sparkConnectedVid{ 0 };
static std::atomic<uint16_t>      g_sparkConnectedPid{ 0 };
static std::atomic<ULONGLONG>     g_sparkLastPacketMs{ 0 };
static ULONGLONG                  g_sparkLastReconnectTryMs = 0;
static std::array<std::atomic<uint16_t>, 256> g_sparkAnalogMilli{};
static std::array<std::atomic<uint16_t>, 256> g_sparkObservedRouteRaw{};
static std::array<std::atomic<uint8_t>, kSparkMaxRows * kSparkColsPerRow> g_sparkRowColToHid{};
static std::array<std::atomic<uint8_t>, kSparkMaxRows> g_sparkRowActive{};
static std::atomic<int>           g_sparkRowCount{ 0 };
static uint16_t                   g_sparkInputReportLen = (uint16_t)kSparkPayloadSize;
static uint16_t                   g_sparkOutputReportLen = (uint16_t)kSparkPayloadSize;
static std::atomic<bool>          g_sparkWriteCapable{ false };
static std::atomic<uint16_t>      g_sparkUsagePage{ 0 };
static std::atomic<ULONGLONG>     g_sparkLastActiveLogMs{ 0 };
static std::atomic<uint32_t>      g_sparkRouteQuerySeq{ 0 };
static std::atomic<uint32_t>      g_sparkRouteOkSeq{ 0 };
static std::atomic<uint32_t>      g_sparkRouteFailSeq{ 0 };
static std::atomic<uint32_t>      g_sparkChangedRowSeq{ 0 };
static std::atomic<uint32_t>      g_sparkInputNotifySeq{ 0 };
static std::atomic<uint8_t>       g_sparkLastRouteRow{ 0xFF };
static std::atomic<uint8_t>       g_sparkLastRouteOk{ 0 };
static std::atomic<ULONGLONG>     g_sparkLastRouteMs{ 0 };
static std::atomic<uint64_t>      g_sparkLastRouteUs{ 0 };
static std::atomic<uint32_t>      g_sparkAvgRouteIntervalUs{ 0 };
static std::atomic<uint32_t>      g_sparkMaxRouteIntervalUs{ 0 };
static std::atomic<uint32_t>      g_sparkAvgRouteTxUs{ 0 };
static std::atomic<uint32_t>      g_sparkMaxRouteTxUs{ 0 };
static std::array<std::atomic<ULONGLONG>, kSparkMaxRows> g_sparkRowLastOkMs{};
static std::atomic<ULONGLONG>     g_sparkLastTelemetryLogMs{ 0 };
static std::atomic<halljoy::worker::WorkerExceptionKind> g_sparkWorkerFaultKind{
    halljoy::worker::WorkerExceptionKind::None };
static halljoy::worker::WorkerExceptionRecord g_sparkWorkerFaultRecord{};
static std::atomic<DWORD>         g_sparkWorkerSehCode{ 0 };
static std::atomic<bool>          g_sparkWorkerExited{ true };
static std::timed_mutex           g_sparkLifecycleMutex;
static halljoy::lifecycle::WorkerLifecycle g_sparkLifecycle;
static std::atomic<bool>          g_sparkRestartBlocked{ false };
static std::atomic<bool>          g_sparkServiceRunning{ false };
static std::atomic<bool>          g_sparkServiceStopRequested{ true };

static bool SparkServiceAllowsStart() noexcept
{
    return g_sparkServiceRunning.load(std::memory_order_acquire) &&
        !g_sparkServiceStopRequested.load(std::memory_order_acquire);
}

static constexpr size_t kSparkFastReportBytes = 1024;

static void Spark_ResetKeyState()
{
    for (auto& m : g_sparkAnalogMilli)
        m.store(0, std::memory_order_relaxed);
    for (auto& r : g_sparkObservedRouteRaw)
        r.store(kSparkObservedRouteRawDefault, std::memory_order_relaxed);
    for (auto& c : g_sparkRowColToHid)
        c.store(0, std::memory_order_relaxed);
    for (auto& r : g_sparkRowActive)
        r.store(0, std::memory_order_relaxed);
    g_sparkRowCount.store(0, std::memory_order_relaxed);
    g_sparkLastPacketMs.store(0, std::memory_order_relaxed);
    g_sparkLastActiveLogMs.store(0, std::memory_order_relaxed);
    g_sparkRouteQuerySeq.store(0, std::memory_order_relaxed);
    g_sparkRouteOkSeq.store(0, std::memory_order_relaxed);
    g_sparkRouteFailSeq.store(0, std::memory_order_relaxed);
    g_sparkChangedRowSeq.store(0, std::memory_order_relaxed);
    g_sparkInputNotifySeq.store(0, std::memory_order_relaxed);
    g_sparkLastRouteRow.store(0xFF, std::memory_order_relaxed);
    g_sparkLastRouteOk.store(0, std::memory_order_relaxed);
    g_sparkLastRouteMs.store(0, std::memory_order_relaxed);
    g_sparkLastRouteUs.store(0, std::memory_order_relaxed);
    g_sparkAvgRouteIntervalUs.store(0, std::memory_order_relaxed);
    g_sparkMaxRouteIntervalUs.store(0, std::memory_order_relaxed);
    g_sparkAvgRouteTxUs.store(0, std::memory_order_relaxed);
    g_sparkMaxRouteTxUs.store(0, std::memory_order_relaxed);
    for (auto& rowMs : g_sparkRowLastOkMs)
        rowMs.store(0, std::memory_order_relaxed);
    g_sparkLastTelemetryLogMs.store(0, std::memory_order_relaxed);
}

static bool SparkReleasePublishedInput() noexcept
{
    bool releasedAny = false;
    for (auto& m : g_sparkAnalogMilli)
    {
        if (m.exchange(0, std::memory_order_relaxed) != 0)
            releasedAny = true;
    }

    g_sparkConnected.store(false, std::memory_order_release);
    g_sparkLastPacketMs.store(0, std::memory_order_relaxed);
    if (releasedAny)
    {
        // Deliver a final all-zero snapshot immediately so a cable removal,
        // worker fault or backend restart cannot leave a virtual control held.
        g_sparkInputNotifySeq.fetch_add(1u, std::memory_order_relaxed);
        RealtimeLoop_NotifyInputChanged();
    }
    return releasedAny;
}

static uint64_t SparkNowUs()
{
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return f;
    }();

    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000ll) / freq.QuadPart);
}

static void SparkAtomicMax(std::atomic<uint32_t>& target, uint32_t value)
{
    uint32_t cur = target.load(std::memory_order_relaxed);
    while (value > cur && !target.compare_exchange_weak(cur, value, std::memory_order_relaxed))
    {
    }
}

static int SparkActiveRowCount()
{
    int rowCount = std::clamp(g_sparkRowCount.load(std::memory_order_relaxed), 0, kSparkMaxRows);
    UINT limit = Settings_GetSparkRowLimit();
    if (limit > 0)
        rowCount = std::min(rowCount, std::clamp((int)limit, 1, kSparkMaxRows));
    int active = 0;
    for (int row = 0; row < rowCount; ++row)
    {
        if (g_sparkRowActive[(size_t)row].load(std::memory_order_relaxed) != 0)
            ++active;
    }
    return active;
}

static void SparkRecordRouteResult(uint8_t row, bool ok, ULONGLONG nowMs)
{
    g_sparkRouteQuerySeq.fetch_add(1u, std::memory_order_relaxed);
    g_sparkLastRouteRow.store(row, std::memory_order_relaxed);
    g_sparkLastRouteOk.store(ok ? 1u : 0u, std::memory_order_relaxed);
    g_sparkLastRouteMs.store(nowMs, std::memory_order_relaxed);

    if (!ok)
    {
        g_sparkRouteFailSeq.fetch_add(1u, std::memory_order_relaxed);
        return;
    }

    g_sparkRouteOkSeq.fetch_add(1u, std::memory_order_relaxed);
    if (row < kSparkMaxRows)
        g_sparkRowLastOkMs[(size_t)row].store(nowMs, std::memory_order_relaxed);

    uint64_t nowUs = SparkNowUs();
    uint64_t prevUs = g_sparkLastRouteUs.exchange(nowUs, std::memory_order_relaxed);
    if (prevUs == 0 || nowUs <= prevUs)
        return;

    uint32_t intervalUs = (uint32_t)std::min<uint64_t>(nowUs - prevUs, 60ull * 1000ull * 1000ull);
    uint32_t avg = g_sparkAvgRouteIntervalUs.load(std::memory_order_relaxed);
    if (avg == 0)
    {
        avg = intervalUs;
    }
    else if (intervalUs > avg)
    {
        avg += (intervalUs - avg + 3u) / 4u;
    }
    else
    {
        avg -= (avg - intervalUs + 3u) / 4u;
    }
    g_sparkAvgRouteIntervalUs.store(avg, std::memory_order_relaxed);
    SparkAtomicMax(g_sparkMaxRouteIntervalUs, intervalUs);
}

static void SparkRecordRouteTxTime(uint32_t txUs)
{
    if (txUs == 0)
        return;

    uint32_t avg = g_sparkAvgRouteTxUs.load(std::memory_order_relaxed);
    if (avg == 0)
    {
        avg = txUs;
    }
    else if (txUs > avg)
    {
        avg += (txUs - avg + 3u) / 4u;
    }
    else
    {
        avg -= (avg - txUs + 3u) / 4u;
    }
    g_sparkAvgRouteTxUs.store(avg, std::memory_order_relaxed);
    SparkAtomicMax(g_sparkMaxRouteTxUs, txUs);
}

static void SparkLogDeviceInfo(const uint8_t rsp[kSparkPayloadSize])
{
    if (!rsp) return;

    uint32_t boardId = ((uint32_t)rsp[4] << 24) |
        ((uint32_t)rsp[5] << 16) |
        ((uint32_t)rsp[6] << 8) |
        ((uint32_t)rsp[7] << 0);

    DebugLog_Write(
        L"[backend.spark] device_info type=%u subtype=%u board=0x%08X app=%u.%u.%u.%u pcb=%u-%u-%u-%u run_mode=%u",
        (unsigned)rsp[2],
        (unsigned)rsp[3],
        (unsigned)boardId,
        (unsigned)rsp[8], (unsigned)rsp[9], (unsigned)rsp[10], (unsigned)rsp[11],
        (unsigned)rsp[12], (unsigned)rsp[13], (unsigned)rsp[14], (unsigned)rsp[15],
        (unsigned)rsp[16]);
}

static void SparkLogLayoutSummary()
{
    int rowCount = std::clamp(g_sparkRowCount.load(std::memory_order_relaxed), 0, kSparkMaxRows);
    for (int row = 0; row < rowCount; ++row)
    {
        wchar_t sample[256]{};
        bool first = true;
        int mapped = 0;
        for (int col = 0; col < kSparkColsPerRow; ++col)
        {
            uint8_t hid = g_sparkRowColToHid[(size_t)row * kSparkColsPerRow + (size_t)col].load(std::memory_order_relaxed);
            if (hid == 0)
                continue;

            ++mapped;
            if ((int)wcslen(sample) >= 180)
                continue;

            wchar_t chunk[24]{};
            swprintf_s(chunk, L"%s%d:%u", first ? L"" : L",", col, (unsigned)hid);
            wcscat_s(sample, chunk);
            first = false;
        }

        DebugLog_Write(
            L"[backend.spark] layout_row row=%d active=%d mapped=%d sample=%s",
            row,
            g_sparkRowActive[(size_t)row].load(std::memory_order_relaxed) ? 1 : 0,
            mapped,
            sample[0] ? sample : L"-");
    }
}

static void SparkLogActiveKeys(const std::array<uint16_t, 256>& nextMilli)
{
    struct PeakItem
    {
        uint16_t hid = 0;
        uint16_t milli = 0;
    };

    PeakItem top[6]{};
    for (uint16_t hid = 1; hid < 256; ++hid)
    {
        uint16_t milli = nextMilli[hid];
        if (milli < 20)
            continue;

        for (int i = 0; i < (int)_countof(top); ++i)
        {
            if (milli <= top[i].milli)
                continue;
            for (int j = (int)_countof(top) - 1; j > i; --j)
                top[j] = top[j - 1];
            top[i].hid = hid;
            top[i].milli = milli;
            break;
        }
    }

    if (top[0].milli == 0)
        return;

    wchar_t sample[256]{};
    for (int i = 0; i < (int)_countof(top); ++i)
    {
        if (top[i].milli == 0)
            break;
        wchar_t chunk[32]{};
        swprintf_s(chunk, L"%s%u=%u", (i == 0) ? L"" : L",", (unsigned)top[i].hid, (unsigned)top[i].milli);
        wcscat_s(sample, chunk);
    }

    DebugLog_Write(L"[backend.spark] active_keys top=%s", sample[0] ? sample : L"-");
}

static int SparkUsageSupportScore(uint16_t usagePage, uint16_t usage)
{
    if (usage == kSparkKnownUsage && usagePage == kSparkUsagePageV2)
        return 4;
    if (usage == kSparkKnownUsage && usagePage == kSparkUsagePageLegacy)
        return 3;
    if (usage == kSparkKnownUsage && (usagePage & 0xFF00u) == 0xFF00u)
        return 2;
    if ((usagePage == kSparkUsagePageV2 || usagePage == kSparkUsagePageLegacy) && usage != 0)
        return 1;
    return 0;
}

static bool SparkReadCaps(HANDLE h, uint16_t* inLen, uint16_t* outLen, uint16_t* usagePage, uint16_t* usage)
{
    if (!h || h == INVALID_HANDLE_VALUE) return false;
    PHIDP_PREPARSED_DATA ppd = nullptr;
    if (!HidD_GetPreparsedData(h, &ppd) || !ppd)
        return false;

    HIDP_CAPS caps{};
    NTSTATUS st = HidP_GetCaps(ppd, &caps);
    HidD_FreePreparsedData(ppd);
    if (st != HIDP_STATUS_SUCCESS)
        return false;

    if (inLen) *inLen = (uint16_t)caps.InputReportByteLength;
    if (outLen) *outLen = (uint16_t)caps.OutputReportByteLength;
    if (usagePage) *usagePage = (uint16_t)caps.UsagePage;
    if (usage) *usage = (uint16_t)caps.Usage;
    return true;
}

struct SparkThreadEvent
{
    HANDLE h = nullptr;
    SparkThreadEvent() : h(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}
    ~SparkThreadEvent()
    {
        if (h)
            CloseHandle(h);
    }
};

static HANDLE SparkGetThreadEvent()
{
    static thread_local SparkThreadEvent ev;
    if (ev.h)
        ResetEvent(ev.h);
    return ev.h;
}

struct SparkThreadBuffers
{
    std::array<uint8_t, kSparkFastReportBytes> write{};
    std::array<uint8_t, kSparkPayloadSize + 1> writeRid{};
    std::array<uint8_t, kSparkFastReportBytes> read{};
};

static SparkThreadBuffers& SparkGetThreadBuffers()
{
    static thread_local SparkThreadBuffers buffers;
    return buffers;
}

static bool SparkWriteFileOverlapped(HANDLE h, const uint8_t* data, DWORD len, DWORD timeoutMs, DWORD* outErr)
{
    if (outErr) *outErr = ERROR_SUCCESS;
    if (!h || h == INVALID_HANDLE_VALUE || !data || len == 0)
    {
        if (outErr) *outErr = ERROR_INVALID_PARAMETER;
        return false;
    }

    HANDLE ev = SparkGetThreadEvent();
    HidIoOperation io(h, ev);
    if (!io.IsValid())
    {
        if (outErr) *outErr = ev ? ERROR_INVALID_HANDLE : GetLastError();
        return false;
    }

    DWORD err = ERROR_SUCCESS;
    auto started = io.StartWrite(data, len, &err);
    if (started == HidIoOperation::StartResult::Failed)
    {
        if (outErr) *outErr = err;
        return false;
    }

    if (started == HidIoOperation::StartResult::Pending)
    {
        DWORD wr = io.Wait(timeoutMs);
        if (wr != WAIT_OBJECT_0)
        {
            DWORD requestedErr = (wr == WAIT_TIMEOUT) ? WAIT_TIMEOUT : GetLastError();
            io.CancelAndDrain(nullptr, nullptr);
            if (outErr) *outErr = requestedErr;
            return false;
        }
    }

    DWORD written = 0;
    if (!io.Finish(&written, &err, false))
    {
        if (outErr) *outErr = err;
        return false;
    }
    return written == len;
}

static bool SparkReadFileOverlapped(HANDLE h, uint8_t* data, DWORD len, DWORD timeoutMs, DWORD* outRead, DWORD* outErr)
{
    if (outRead) *outRead = 0;
    if (outErr) *outErr = ERROR_SUCCESS;
    if (!h || h == INVALID_HANDLE_VALUE || !data || len == 0)
    {
        if (outErr) *outErr = ERROR_INVALID_PARAMETER;
        return false;
    }

    HANDLE ev = SparkGetThreadEvent();
    HidIoOperation io(h, ev);
    if (!io.IsValid())
    {
        if (outErr) *outErr = ev ? ERROR_INVALID_HANDLE : GetLastError();
        return false;
    }

    DWORD err = ERROR_SUCCESS;
    auto started = io.StartRead(data, len, &err);
    if (started == HidIoOperation::StartResult::Failed)
    {
        if (outErr) *outErr = err;
        return false;
    }

    if (started == HidIoOperation::StartResult::Pending)
    {
        DWORD wr = io.Wait(timeoutMs);
        if (wr != WAIT_OBJECT_0)
        {
            DWORD requestedErr = (wr == WAIT_TIMEOUT) ? WAIT_TIMEOUT : GetLastError();
            io.CancelAndDrain(nullptr, nullptr);
            if (outErr) *outErr = requestedErr;
            return false;
        }
    }

    DWORD readBytes = 0;
    if (!io.Finish(&readBytes, &err, false))
    {
        if (outErr) *outErr = err;
        return false;
    }

    if (outRead) *outRead = readBytes;
    return readBytes != 0;
}

static const uint8_t* SparkFindPacket(const uint8_t* bytes, size_t len, int b0, int b1, int b2 = -1, int b3 = -1)
{
    if (!bytes || len < kSparkPayloadSize)
        return nullptr;

    for (size_t off = 0; off <= 1; ++off)
    {
        if (off + kSparkPayloadSize > len)
            continue;
        const uint8_t* p = bytes + off;
        if ((b0 >= 0 && p[0] != (uint8_t)b0) ||
            (b1 >= 0 && p[1] != (uint8_t)b1) ||
            (b2 >= 0 && p[2] != (uint8_t)b2) ||
            (b3 >= 0 && p[3] != (uint8_t)b3))
            continue;
        return p;
    }
    return nullptr;
}

static bool SparkWritePacket(const uint8_t payload[kSparkPayloadSize])
{
    if (!payload || !g_sparkHandle) return false;
    if (g_sparkOutputReportLen < kSparkPayloadSize) return false;
    if (!g_sparkWriteCapable.load(std::memory_order_acquire)) return false;

    DWORD lastErr = 0;
    SparkThreadBuffers& buffers = SparkGetThreadBuffers();
    uint8_t* out = nullptr;
    size_t outSize = (size_t)g_sparkOutputReportLen;
    std::vector<uint8_t> fallbackOut;
    if (outSize <= buffers.write.size())
    {
        out = buffers.write.data();
        memset(out, 0, outSize);
    }
    else
    {
        fallbackOut.assign(outSize, 0);
        out = fallbackOut.data();
    }

    size_t offset = (g_sparkOutputReportLen > kSparkPayloadSize) ? 1u : 0u;
    if (out && offset + kSparkPayloadSize <= outSize)
    {
        memcpy(out + offset, payload, kSparkPayloadSize);
        if (SparkWriteFileOverlapped(g_sparkHandle, out, (DWORD)outSize, 250, &lastErr))
            return true;
    }

    uint8_t* outRid = buffers.writeRid.data();
    memset(outRid, 0, buffers.writeRid.size());
    memcpy(outRid + 1, payload, kSparkPayloadSize);
    if (SparkWriteFileOverlapped(g_sparkHandle, outRid, (DWORD)buffers.writeRid.size(), 250, &lastErr))
        return true;

    if (out && HidD_SetOutputReport(g_sparkHandle, out, (ULONG)outSize))
        return true;
    if (HidD_SetOutputReport(g_sparkHandle, outRid, (ULONG)buffers.writeRid.size()))
        return true;

    if (out && HidD_SetFeature(g_sparkHandle, out, (ULONG)outSize))
        return true;
    if (HidD_SetFeature(g_sparkHandle, outRid, (ULONG)buffers.writeRid.size()))
        return true;

    if (lastErr == 0)
        lastErr = ERROR_GEN_FAILURE;
    SetLastError(lastErr);
    return false;
}

static bool SparkTransact(
    const uint8_t payload[kSparkPayloadSize],
    uint8_t outResponse[kSparkPayloadSize],
    DWORD timeoutMs,
    int b0,
    int b1,
    int b2 = -1,
    int b3 = -1)
{
    if (!payload || !outResponse || !g_sparkHandle)
        return false;
    if (!SparkWritePacket(payload))
        return false;

    ULONGLONG deadline = GetTickCount64() + timeoutMs;
    SparkThreadBuffers& buffers = SparkGetThreadBuffers();
    size_t inSize = (size_t)std::max<uint16_t>(g_sparkInputReportLen, (uint16_t)(kSparkPayloadSize + 1u));
    uint8_t* inBuf = nullptr;
    std::vector<uint8_t> fallbackIn;
    if (inSize <= buffers.read.size())
    {
        inBuf = buffers.read.data();
    }
    else
    {
        fallbackIn.resize(inSize);
        inBuf = fallbackIn.data();
    }

    while (GetTickCount64() < deadline)
    {
        DWORD remain = (DWORD)std::clamp<ULONGLONG>(deadline - GetTickCount64(), 1, 80);
        DWORD readBytes = 0;
        DWORD err = 0;
        if (!SparkReadFileOverlapped(g_sparkHandle, inBuf, (DWORD)inSize, remain, &readBytes, &err))
        {
            if (err == WAIT_TIMEOUT || err == ERROR_IO_INCOMPLETE)
                continue;
            if (err == ERROR_OPERATION_ABORTED || err == ERROR_INVALID_HANDLE || err == ERROR_DEVICE_NOT_CONNECTED)
                break;
            continue;
        }

        const uint8_t* p = SparkFindPacket(inBuf, readBytes, b0, b1, b2, b3);
        if (!p)
            continue;

        memcpy(outResponse, p, kSparkPayloadSize);
        return true;
    }
    return false;
}

static void SparkFillPacket(std::initializer_list<uint8_t> init, uint8_t outPacket[kSparkPayloadSize])
{
    memset(outPacket, 0, kSparkPayloadSize);
    size_t idx = 0;
    for (uint8_t b : init)
    {
        if (idx >= kSparkPayloadSize)
            break;
        outPacket[idx++] = b;
    }
}

static bool SparkQueryDeviceInfo(uint8_t outResponse[kSparkPayloadSize])
{
    uint8_t req[kSparkPayloadSize]{};
    SparkFillPacket({ 0x01, 0x02 }, req);
    return SparkTransact(req, outResponse, 250, 0x01, 0x02);
}

static bool SparkQueryLayoutRow(uint8_t row, std::array<uint16_t, kSparkColsPerRow>& outCodes)
{
    outCodes.fill(0);
    uint8_t req[kSparkPayloadSize]{};
    uint8_t rsp[kSparkPayloadSize]{};
    SparkFillPacket({ 0x03, 0x01, 0x00, row }, req);
    if (!SparkTransact(req, rsp, 250, 0x03, 0x01, 0x00, row))
        return false;

    for (int col = 0; col < kSparkColsPerRow; ++col)
    {
        size_t off = 4u + (size_t)col * 2u;
        outCodes[(size_t)col] = (uint16_t)((uint16_t)rsp[off] | ((uint16_t)rsp[off + 1] << 8));
    }
    return true;
}

static bool SparkQueryRouteRow(uint8_t row, std::array<uint16_t, kSparkColsPerRow>& outValues)
{
    outValues.fill(0);
    uint8_t req[kSparkPayloadSize]{};
    uint8_t rsp[kSparkPayloadSize]{};
    SparkFillPacket({ 0x04, 0x03, 0x01, row }, req);
    if (!SparkTransact(req, rsp, 250, 0x04, 0x03, 0x01, row))
        return false;

    for (int col = 0; col < kSparkColsPerRow; ++col)
    {
        size_t off = 4u + (size_t)col * 2u;
        outValues[(size_t)col] = (uint16_t)((uint16_t)rsp[off] | ((uint16_t)rsp[off + 1] << 8));
    }
    return true;
}

static int SparkQueryRouteRowsBurst(
    const uint8_t* rows,
    int rowCount,
    std::array<std::array<uint16_t, kSparkColsPerRow>, kSparkMaxRows>& outValues,
    std::array<uint8_t, kSparkMaxRows>& outOk)
{
    if (!rows || rowCount <= 0 || !g_sparkHandle)
        return 0;

    rowCount = std::clamp(rowCount, 0, kSparkMaxRows);
    outOk.fill(0);
    for (auto& rowValues : outValues)
        rowValues.fill(0);

    uint8_t req[kSparkPayloadSize]{};
    int sent = 0;
    for (int i = 0; i < rowCount; ++i)
    {
        SparkFillPacket({ 0x04, 0x03, 0x01, rows[i] }, req);
        if (!SparkWritePacket(req))
            break;
        ++sent;
    }
    if (sent <= 0)
        return 0;

    ULONGLONG deadline = GetTickCount64() + 250;
    SparkThreadBuffers& buffers = SparkGetThreadBuffers();
    size_t inSize = (size_t)std::max<uint16_t>(g_sparkInputReportLen, (uint16_t)(kSparkPayloadSize + 1u));
    uint8_t* inBuf = nullptr;
    std::vector<uint8_t> fallbackIn;
    if (inSize <= buffers.read.size())
    {
        inBuf = buffers.read.data();
    }
    else
    {
        fallbackIn.resize(inSize);
        inBuf = fallbackIn.data();
    }

    int got = 0;
    while (got < sent && GetTickCount64() < deadline)
    {
        DWORD remain = (DWORD)std::clamp<ULONGLONG>(deadline - GetTickCount64(), 1, 20);
        DWORD readBytes = 0;
        DWORD err = 0;
        if (!SparkReadFileOverlapped(g_sparkHandle, inBuf, (DWORD)inSize, remain, &readBytes, &err))
        {
            if (err == WAIT_TIMEOUT || err == ERROR_IO_INCOMPLETE)
                continue;
            if (err == ERROR_OPERATION_ABORTED || err == ERROR_INVALID_HANDLE || err == ERROR_DEVICE_NOT_CONNECTED)
                break;
            continue;
        }

        const uint8_t* p = SparkFindPacket(inBuf, readBytes, 0x04, 0x03, 0x01);
        if (!p)
            continue;

        uint8_t row = p[3];
        int rowIndex = -1;
        for (int i = 0; i < sent; ++i)
        {
            if (rows[i] == row)
            {
                rowIndex = i;
                break;
            }
        }
        if (rowIndex < 0 || row >= kSparkMaxRows || outOk[(size_t)row] != 0)
            continue;

        for (int col = 0; col < kSparkColsPerRow; ++col)
        {
            size_t off = 4u + (size_t)col * 2u;
            outValues[(size_t)row][(size_t)col] = (uint16_t)((uint16_t)p[off] | ((uint16_t)p[off + 1] << 8));
        }
        outOk[(size_t)row] = 1;
        ++got;
    }

    return got;
}

static uint16_t SparkNormalizeRouteToMilli(uint16_t hid, uint16_t routeRaw)
{
    if (hid == 0 || routeRaw == 0)
        return 0;

    uint16_t observed = g_sparkObservedRouteRaw[hid].load(std::memory_order_relaxed);
    observed = std::clamp<uint16_t>(observed, 3000, 5000);
    if (routeRaw > observed && routeRaw <= 5000)
    {
        observed = routeRaw;
        g_sparkObservedRouteRaw[hid].store(observed, std::memory_order_relaxed);
    }

    float v01 = (float)routeRaw / (float)std::max<uint16_t>(observed, 1);
    return (uint16_t)std::clamp((int)std::lround(Clamp01(v01) * 1000.0f), 0, 1000);
}

static void SparkDiscoverLayout()
{
    for (auto& c : g_sparkRowColToHid)
        c.store(0, std::memory_order_relaxed);
    for (auto& r : g_sparkRowActive)
        r.store(0, std::memory_order_relaxed);
    g_sparkRowCount.store(0, std::memory_order_relaxed);

    int highestActiveRow = -1;
    int emptyRun = 0;
    for (uint8_t row = 0; row < (uint8_t)kSparkMaxRows; ++row)
    {
        std::array<uint16_t, kSparkColsPerRow> codes{};
        if (!SparkQueryLayoutRow(row, codes))
            break;

        bool rowHasAny = false;
        bool rowHasMappable = false;
        for (int col = 0; col < kSparkColsPerRow; ++col)
        {
            uint16_t code = codes[(size_t)col];
            if (code != 0)
                rowHasAny = true;

            uint8_t hid = (code <= 0x00FFu) ? (uint8_t)code : 0u;
            g_sparkRowColToHid[(size_t)row * kSparkColsPerRow + (size_t)col].store(hid, std::memory_order_relaxed);
            if (hid != 0)
                rowHasMappable = true;
        }

        g_sparkRowActive[(size_t)row].store(rowHasMappable ? 1u : 0u, std::memory_order_relaxed);
        if (rowHasAny)
        {
            highestActiveRow = row;
            emptyRun = 0;
        }
        else
        {
            ++emptyRun;
            if (highestActiveRow >= 0 && emptyRun >= 2)
                break;
        }
    }

    int rowCount = (highestActiveRow >= 0) ? (highestActiveRow + 1) : 0;
    g_sparkRowCount.store(rowCount, std::memory_order_relaxed);
    DebugLog_Write(L"[backend.spark] layout rows=%d", rowCount);
    SparkLogLayoutSummary();
}

static HANDLE SparkTryOpenDevice(
    uint16_t* inLen,
    uint16_t* outLen,
    std::wstring* outPath,
    bool* outWriteCapable,
    uint16_t* outVid,
    uint16_t* outPid,
    uint16_t* outUsagePage)
{
    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE)
        return nullptr;

    HANDLE bestWritable = nullptr;
    int bestWritableScore = -1;
    std::wstring bestWritablePath;
    uint16_t bestWritableIn = 0;
    uint16_t bestWritableOut = 0;
    uint16_t bestWritableVid = 0;
    uint16_t bestWritablePid = 0;
    uint16_t bestWritableUsagePage = 0;

    HANDLE bestReadOnly = nullptr;
    int bestReadOnlyScore = -1;
    std::wstring bestReadOnlyPath;
    uint16_t bestReadOnlyIn = 0;
    uint16_t bestReadOnlyOut = 0;
    uint16_t bestReadOnlyVid = 0;
    uint16_t bestReadOnlyPid = 0;
    uint16_t bestReadOnlyUsagePage = 0;

    for (DWORD idx = 0;; ++idx)
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
        if (!HidD_GetAttributes(h, &attr))
        {
            CloseHandle(h);
            continue;
        }
        const bool routedToSpark = NativeAnalogRouting_IsClaimedBy(
            attr.VendorID, attr.ProductID, NativeAnalogProtocol::SparkLink);
        if (NativeAnalogRouting_IsClaimed(attr.VendorID, attr.ProductID) && !routedToSpark)
        {
            CloseHandle(h);
            continue;
        }

        uint16_t inR = 0, outR = 0, usagePage = 0, usage = 0;
        if (!SparkReadCaps(h, &inR, &outR, &usagePage, &usage))
        {
            CloseHandle(h);
            continue;
        }

        int score = SparkUsageSupportScore(usagePage, usage);
        if (inR < kSparkPayloadSize || outR < kSparkPayloadSize || score <= 0)
        {
            CloseHandle(h);
            continue;
        }

        DebugLog_Write(
            L"[backend.spark] candidate vid=0x%04X pid=0x%04X path=%s write_capable=%d score=%d in_len=%u out_len=%u usage_page=0x%04X usage=0x%04X",
            (unsigned)attr.VendorID,
            (unsigned)attr.ProductID,
            detail->DevicePath,
            writeCapable ? 1 : 0,
            score,
            (unsigned)inR,
            (unsigned)outR,
            (unsigned)usagePage,
            (unsigned)usage);

        HidD_SetNumInputBuffers(h, 64);
        if (writeCapable)
        {
            if (!bestWritable || score > bestWritableScore)
            {
                if (bestWritable)
                    CloseHandle(bestWritable);
                bestWritable = h;
                bestWritableScore = score;
                bestWritablePath = detail->DevicePath;
                bestWritableIn = inR;
                bestWritableOut = outR;
                bestWritableVid = attr.VendorID;
                bestWritablePid = attr.ProductID;
                bestWritableUsagePage = usagePage;
            }
            else
            {
                CloseHandle(h);
            }
            continue;
        }

        if (!bestReadOnly || score > bestReadOnlyScore)
        {
            if (bestReadOnly)
                CloseHandle(bestReadOnly);
            bestReadOnly = h;
            bestReadOnlyScore = score;
            bestReadOnlyPath = detail->DevicePath;
            bestReadOnlyIn = inR;
            bestReadOnlyOut = outR;
            bestReadOnlyVid = attr.VendorID;
            bestReadOnlyPid = attr.ProductID;
            bestReadOnlyUsagePage = usagePage;
        }
        else
        {
            CloseHandle(h);
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);

    if (bestWritable)
    {
        if (bestReadOnly)
            CloseHandle(bestReadOnly);
        if (inLen) *inLen = bestWritableIn;
        if (outLen) *outLen = bestWritableOut;
        if (outPath) *outPath = bestWritablePath;
        if (outWriteCapable) *outWriteCapable = true;
        if (outVid) *outVid = bestWritableVid;
        if (outPid) *outPid = bestWritablePid;
        if (outUsagePage) *outUsagePage = bestWritableUsagePage;
        return bestWritable;
    }

    if (bestReadOnly)
    {
        if (inLen) *inLen = bestReadOnlyIn;
        if (outLen) *outLen = bestReadOnlyOut;
        if (outPath) *outPath = bestReadOnlyPath;
        if (outWriteCapable) *outWriteCapable = false;
        if (outVid) *outVid = bestReadOnlyVid;
        if (outPid) *outPid = bestReadOnlyPid;
        if (outUsagePage) *outUsagePage = bestReadOnlyUsagePage;
    }
    return bestReadOnly;
}

static DWORD SparkPollThreadProcImpl()
{
#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    if (commandLine && wcsstr(commandLine, L"--halljoy-test-spark-stop-timeout"))
    {
        StabilityTrace_Write(L"WARN", L"spark", L"test.stop_timeout.injected",
            L"simulator_only=1");
        Sleep(INFINITE);
    }
    if (commandLine && wcsstr(commandLine, L"--halljoy-test-spark-service-shutdown"))
    {
        StabilityTrace_Write(L"INFO", L"spark", L"test.service_worker",
            L"simulator_only=1 cooperative_wait=1");
        if (g_sparkStopEvent)
            WaitForSingleObject(g_sparkStopEvent, INFINITE);
        return 0;
    }
#endif

    if (!g_sparkHandle || !g_sparkStopEvent)
        return 0;

    const uint16_t usagePage = g_sparkUsagePage.load(std::memory_order_relaxed);
    DebugLog_Write(L"[backend.spark] poller thread start usage_page=0x%04X", (unsigned)usagePage);

    uint8_t devInfo[kSparkPayloadSize]{};
    if (!SparkQueryDeviceInfo(devInfo) || devInfo[2] != 0x01)
    {
        DebugLog_Write(L"[backend.spark] device_info probe failed");
        g_sparkConnected.store(false, std::memory_order_release);
        return 0;
    }
    SparkLogDeviceInfo(devInfo);

    SparkDiscoverLayout();
    int rowCount = std::clamp(g_sparkRowCount.load(std::memory_order_relaxed), 0, kSparkMaxRows);
    if (rowCount <= 0)
    {
        DebugLog_Write(L"[backend.spark] no layout rows discovered");
        g_sparkConnected.store(false, std::memory_order_release);
        return 0;
    }

    DebugLog_Write(
        L"[backend.spark] poller start rows=%d in_len=%u out_len=%u usage_page=0x%04X",
        rowCount,
        (unsigned)g_sparkInputReportLen,
        (unsigned)g_sparkOutputReportLen,
        (unsigned)g_sparkUsagePage.load(std::memory_order_relaxed));

    int failStreak = 0;
    uint8_t nextRouteRow = 0;
    while (WaitForSingleObject(g_sparkStopEvent, 0) != WAIT_OBJECT_0)
    {
        rowCount = std::clamp(g_sparkRowCount.load(std::memory_order_relaxed), 0, kSparkMaxRows);
        UINT rowLimit = Settings_GetSparkRowLimit();
        if (rowLimit > 0)
            rowCount = std::min(rowCount, std::clamp((int)rowLimit, 1, kSparkMaxRows));
        bool attemptedRow = false;
        bool rowOk = false;
        bool inputChanged = false;
        if (false)
        {
            std::array<uint8_t, kSparkMaxRows> rows{};
            int burstRows = 0;
            int maxRows = std::max(rowCount, 1);
            for (int attempt = 0; attempt < maxRows && burstRows < kSparkMaxRows; ++attempt)
            {
                uint8_t row = (uint8_t)(nextRouteRow % (uint8_t)maxRows);
                nextRouteRow = (uint8_t)((row + 1u) % (uint8_t)maxRows);
                if (g_sparkRowActive[(size_t)row].load(std::memory_order_relaxed) == 0)
                    continue;
                rows[(size_t)burstRows++] = row;
            }

            attemptedRow = burstRows > 0;
            ULONGLONG nowRouteMs = GetTickCount64();
            uint64_t txStartUs = SparkNowUs();
            std::array<std::array<uint16_t, kSparkColsPerRow>, kSparkMaxRows> routes{};
            std::array<uint8_t, kSparkMaxRows> okRows{};
            int gotRows = SparkQueryRouteRowsBurst(rows.data(), burstRows, routes, okRows);
            uint64_t txEndUs = SparkNowUs();
            if (txEndUs > txStartUs && gotRows > 0)
                SparkRecordRouteTxTime((uint32_t)std::min<uint64_t>((txEndUs - txStartUs) / (uint64_t)gotRows, 60ull * 1000ull * 1000ull));

            for (int i = 0; i < burstRows; ++i)
            {
                uint8_t row = rows[(size_t)i];
                bool ok = okRows[(size_t)row] != 0;
                SparkRecordRouteResult(row, ok, nowRouteMs);
                if (!ok)
                    continue;

                rowOk = true;
                bool rowChanged = false;
                for (int col = 0; col < kSparkColsPerRow; ++col)
                {
                    uint8_t hid = g_sparkRowColToHid[(size_t)row * kSparkColsPerRow + (size_t)col].load(std::memory_order_relaxed);
                    if (hid == 0)
                        continue;
                    uint16_t milli = SparkNormalizeRouteToMilli(hid, routes[(size_t)row][(size_t)col]);
                    uint16_t previous = g_sparkAnalogMilli[hid].exchange(milli, std::memory_order_relaxed);
                    if (previous != milli)
                        rowChanged = true;
                }
                if (rowChanged)
                {
                    g_sparkChangedRowSeq.fetch_add(1u, std::memory_order_relaxed);
                    inputChanged = true;
                }
            }
        }
        else
        {
            for (int attempt = 0; attempt < rowCount; ++attempt)
            {
                if (WaitForSingleObject(g_sparkStopEvent, 0) == WAIT_OBJECT_0)
                    break;

                uint8_t row = (uint8_t)(nextRouteRow % (uint8_t)std::max(rowCount, 1));
                nextRouteRow = (uint8_t)((row + 1u) % (uint8_t)std::max(rowCount, 1));
                if (g_sparkRowActive[(size_t)row].load(std::memory_order_relaxed) == 0)
                    continue;

                attemptedRow = true;
                std::array<uint16_t, kSparkColsPerRow> route{};
                ULONGLONG nowRouteMs = GetTickCount64();
                uint64_t txStartUs = SparkNowUs();
                bool queryOk = SparkQueryRouteRow(row, route);
                uint64_t txEndUs = SparkNowUs();
                if (txEndUs > txStartUs)
                    SparkRecordRouteTxTime((uint32_t)std::min<uint64_t>(txEndUs - txStartUs, 60ull * 1000ull * 1000ull));

                if (!queryOk)
                {
                    SparkRecordRouteResult(row, false, nowRouteMs);
                    break;
                }

                rowOk = true;
                SparkRecordRouteResult(row, true, nowRouteMs);
                bool rowChanged = false;
                for (int col = 0; col < kSparkColsPerRow; ++col)
                {
                    uint8_t hid = g_sparkRowColToHid[(size_t)row * kSparkColsPerRow + (size_t)col].load(std::memory_order_relaxed);
                    if (hid == 0)
                        continue;
                    uint16_t milli = SparkNormalizeRouteToMilli(hid, route[(size_t)col]);
                    uint16_t previous = g_sparkAnalogMilli[hid].exchange(milli, std::memory_order_relaxed);
                    if (previous != milli)
                        rowChanged = true;
                }
                if (rowChanged)
                {
                    g_sparkChangedRowSeq.fetch_add(1u, std::memory_order_relaxed);
                    inputChanged = true;
                }
                break;
            }
        }

        if (!attemptedRow || !rowOk)
        {
            ++failStreak;
            if (failStreak >= 6)
                break;
            Sleep(15);
            continue;
        }

        failStreak = 0;
        ULONGLONG nowMs = GetTickCount64();
        g_sparkLastPacketMs.store(nowMs, std::memory_order_relaxed);
        // A successful route query is not automatically fresh input. The old
        // implementation signalled the realtime loop after every row, even
        // when all normalized values were unchanged. At ~4000 route queries/s
        // that produced a permanent wake storm while the keyboard was idle.
        // Publish only rows that actually changed. Auto-reset event semantics
        // still coalesce concurrent changed rows while Backend_Tick is running.
        if (inputChanged)
        {
            g_sparkInputNotifySeq.fetch_add(1u, std::memory_order_relaxed);
            RealtimeLoop_NotifyInputChanged();
        }

        ULONGLONG lastActiveLog = g_sparkLastActiveLogMs.load(std::memory_order_relaxed);
        if (nowMs - lastActiveLog >= 1200)
        {
            std::array<uint16_t, 256> snapshot{};
            for (uint16_t hid = 1; hid < 256; ++hid)
                snapshot[hid] = g_sparkAnalogMilli[hid].load(std::memory_order_relaxed);
            g_sparkLastActiveLogMs.store(nowMs, std::memory_order_relaxed);
            SparkLogActiveKeys(snapshot);
        }

        UINT mode = Settings_GetSparkPollMode();
        if (mode == SettingsSparkPollMode_Safe)
        {
            // Avoid bursting several vendor request/response transactions back-to-back:
            // the keyboard still sends normal HID key reports on another interface.
            UINT pollMs = std::max<UINT>(kSparkMinRoutePollIntervalMs, Settings_GetPollingMs());
            Sleep(pollMs);
        }
        else if (mode == SettingsSparkPollMode_FastYield)
        {
            SwitchToThread();
        }
        else
        {
            if ((g_sparkRouteQuerySeq.load(std::memory_order_relaxed) & 0x0Fu) == 0)
                SwitchToThread();
        }
    }

    const bool releasedAny = SparkReleasePublishedInput();
    StabilityTrace_Write(
        L"INFO", L"spark", L"worker.stats",
        L"route_queries=%u route_ok=%u route_fail=%u changed_rows=%u input_notifies=%u "
        L"avg_route_interval_us=%u max_route_interval_us=%u avg_route_tx_us=%u max_route_tx_us=%u "
        L"released_any=%d",
        g_sparkRouteQuerySeq.load(std::memory_order_relaxed),
        g_sparkRouteOkSeq.load(std::memory_order_relaxed),
        g_sparkRouteFailSeq.load(std::memory_order_relaxed),
        g_sparkChangedRowSeq.load(std::memory_order_relaxed),
        g_sparkInputNotifySeq.load(std::memory_order_relaxed),
        g_sparkAvgRouteIntervalUs.load(std::memory_order_relaxed),
        g_sparkMaxRouteIntervalUs.load(std::memory_order_relaxed),
        g_sparkAvgRouteTxUs.load(std::memory_order_relaxed),
        g_sparkMaxRouteTxUs.load(std::memory_order_relaxed),
        releasedAny ? 1 : 0);
    DebugLog_Write(
        L"[backend.spark] poller stop changed_rows=%u input_notifies=%u released_any=%d",
        g_sparkChangedRowSeq.load(std::memory_order_relaxed),
        g_sparkInputNotifySeq.load(std::memory_order_relaxed),
        releasedAny ? 1 : 0);
    return 0;
}

static void SparkWorkerOnCppFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_sparkWorkerFaultRecord = record;
    g_sparkWorkerFaultKind.store(record.kind, std::memory_order_release);
    g_sparkWriteCapable.store(false, std::memory_order_release);
    const bool releasedAny = SparkReleasePublishedInput();
    StabilityTrace_WriteCritical(L"ERROR", L"spark", L"worker.fault",
        L"kind=%u neutralized=1 released_any=%d", static_cast<unsigned>(record.kind), releasedAny ? 1 : 0);
    OutputDebugStringA("[HallJoy] SparkLink native worker exception: ");
    OutputDebugStringA(record.message[0] ? record.message : "unknown");
    OutputDebugStringA("\n");
}

static void SparkWorkerOnCppCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    // Covers normal early returns as well as the exception path. The regular
    // loop tail may already have published zeroes; this helper is idempotent.
    const bool releasedAny = SparkReleasePublishedInput();
    g_sparkWorkerExited.store(true, std::memory_order_release);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"spark", L"worker.exit", L"fault_kind=%u neutralized=1 released_any=%d",
        static_cast<unsigned>(record.kind), releasedAny ? 1 : 0);
}

static DWORD SparkPollThreadProcCpp() noexcept
{
    return static_cast<DWORD>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return static_cast<std::uint32_t>(SparkPollThreadProcImpl()); },
        SparkWorkerOnCppFault,
        SparkWorkerOnCppCompletion,
        0xE0520003u));
}

static void SparkWorkerOnStructuredFault(DWORD code) noexcept
{
    halljoy::worker::WorkerExceptionRecord record{};
    record.kind = halljoy::worker::WorkerExceptionKind::UnknownException;
    halljoy::worker::CopyWorkerExceptionMessage(record, "structured exception");
    g_sparkWorkerFaultRecord = record;
    g_sparkWorkerSehCode.store(code, std::memory_order_release);
    g_sparkWorkerFaultKind.store(record.kind, std::memory_order_release);
    g_sparkWriteCapable.store(false, std::memory_order_release);
    const bool releasedAny = SparkReleasePublishedInput();
    g_sparkWorkerExited.store(true, std::memory_order_release);
    StabilityTrace_WriteCritical(L"ERROR", L"spark", L"worker.fault",
        L"kind=seh code=0x%08lX neutralized=1 released_any=%d", code, releasedAny ? 1 : 0);
    StabilityTrace_WriteCritical(L"ERROR", L"spark", L"worker.exit",
        L"fault_kind=seh code=0x%08lX", code);
}

static DWORD WINAPI SparkPollThreadProc(LPVOID) noexcept
{
    StabilityTrace_Write(L"INFO", L"spark", L"worker.start");
    __try
    {
        return SparkPollThreadProcCpp();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        const DWORD code = GetExceptionCode();
        SparkWorkerOnStructuredFault(code);
        OutputDebugStringA("[HallJoy] SparkLink native worker structured exception\n");
        return 0;
    }
}

static halljoy::lifecycle::StopResult SparkStopLocked()
{
    const auto lifecycleState = g_sparkLifecycle.State();
    if (lifecycleState == halljoy::lifecycle::WorkerState::Stopped ||
        lifecycleState == halljoy::lifecycle::WorkerState::Joined)
    {
        return g_sparkLifecycle.RequestStop();
    }
    if (lifecycleState == halljoy::lifecycle::WorkerState::Poisoned)
    {
        return g_sparkLifecycle.RequestStop(g_sparkLifecycle.Generation());
    }
    const auto requested = g_sparkLifecycle.RequestStop(g_sparkLifecycle.Generation());
    if (requested.status != halljoy::lifecycle::StopStatus::StopRequested)
        return requested;

    const bool hadResources = g_sparkThread || g_sparkHandle || g_sparkStopEvent;
    if (hadResources)
        StabilityTrace_Write(L"INFO", L"spark", L"stop.begin", L"has_thread=%d has_handle=%d",
            g_sparkThread ? 1 : 0, g_sparkHandle ? 1 : 0);
    g_sparkConnected.store(false, std::memory_order_release);
    if (g_sparkStopEvent)
        SetEvent(g_sparkStopEvent);
    if (g_sparkHandle)
        CancelIoEx(g_sparkHandle, nullptr);

    if (!g_sparkThread)
    {
        g_sparkRestartBlocked.store(true, std::memory_order_release);
        const auto poisoned = g_sparkLifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed,
            ERROR_INVALID_HANDLE);
        StabilityTrace_WriteCritical(L"ERROR", L"spark", L"stop.invalid_thread",
            L"generation=%llu win32=%lu resources_retained=1 restart_blocked=1",
            static_cast<unsigned long long>(requested.generation.Value()),
            static_cast<unsigned long>(ERROR_INVALID_HANDLE));
        return poisoned;
    }

    const DWORD waitResult = WaitForSingleObject(g_sparkThread, 3000);
    const DWORD waitError = waitResult == WAIT_OBJECT_0
        ? ERROR_SUCCESS
        : (waitResult == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT);
    const auto observedJoin = halljoy::lifecycle::ObserveWorkerJoin(
        requested.generation,
        waitResult == WAIT_OBJECT_0
            ? halljoy::lifecycle::JoinWaitStatus::Joined
            : (waitResult == WAIT_FAILED
                ? halljoy::lifecycle::JoinWaitStatus::Failed
                : halljoy::lifecycle::JoinWaitStatus::TimedOut),
        waitError);
    if (!observedJoin.Completed())
    {
        g_sparkRestartBlocked.store(true, std::memory_order_release);
        const auto poisoned = g_sparkLifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            observedJoin.error.code, observedJoin.error.native_error);
        DebugLog_Write(L"[backend.spark] stop join incomplete wait=%lu err=%lu; resources retained and restart blocked",
            static_cast<unsigned long>(waitResult), static_cast<unsigned long>(waitError));
        StabilityTrace_WriteCritical(L"ERROR", L"spark", L"stop.timeout",
            L"generation=%llu wait=%lu win32=%lu thread_handle_retained=1 hid_handle_retained=%d stop_event_retained=%d restart_blocked=1",
            static_cast<unsigned long long>(requested.generation.Value()),
            static_cast<unsigned long>(waitResult),
            static_cast<unsigned long>(waitError),
            g_sparkHandle ? 1 : 0, g_sparkStopEvent ? 1 : 0);
        return poisoned;
    }

    CloseHandle(g_sparkThread);
    g_sparkThread = nullptr;
    g_sparkWorkerExited.store(true, std::memory_order_release);
    if (g_sparkHandle)
    {
        CloseHandle(g_sparkHandle);
        g_sparkHandle = nullptr;
    }
    if (g_sparkStopEvent)
    {
        CloseHandle(g_sparkStopEvent);
        g_sparkStopEvent = nullptr;
    }

    g_sparkInputReportLen = (uint16_t)kSparkPayloadSize;
    g_sparkOutputReportLen = (uint16_t)kSparkPayloadSize;
    g_sparkWriteCapable.store(false, std::memory_order_release);
    g_sparkConnectedVid.store(0, std::memory_order_relaxed);
    g_sparkConnectedPid.store(0, std::memory_order_relaxed);
    g_sparkUsagePage.store(0, std::memory_order_relaxed);
    g_sparkLastActiveLogMs.store(0, std::memory_order_relaxed);
    if (hadResources)
    {
        StabilityTrace_Write(L"INFO", L"spark", L"neutralized", L"reason=stop");
        StabilityTrace_Write(L"INFO", L"spark", L"disconnected", L"reason=stop");
        StabilityTrace_Write(L"INFO", L"spark", L"stop.end");
    }
    return g_sparkLifecycle.ConfirmJoined(requested.generation);
}

static halljoy::lifecycle::StopResult SparkStop()
{
    std::unique_lock<std::timed_mutex> lifecycleGuard(g_sparkLifecycleMutex, std::defer_lock);
    if (!lifecycleGuard.try_lock_for(std::chrono::milliseconds(500)))
    {
        g_sparkRestartBlocked.store(true, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"spark", L"stop.lock_timeout",
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
    return SparkStopLocked();
}

static bool SparkStart()
{
    if (g_sparkRestartBlocked.load(std::memory_order_acquire) || !SparkServiceAllowsStart())
        return false;
    const std::lock_guard<std::timed_mutex> lifecycleGuard(g_sparkLifecycleMutex);
    if (g_sparkRestartBlocked.load(std::memory_order_acquire) || !SparkServiceAllowsStart())
        return false;

    if (g_sparkConnected.load(std::memory_order_acquire))
        return true;
    if ((g_sparkThread || g_sparkHandle || g_sparkStopEvent) &&
        !SparkStopLocked().RestartSafe())
        return false;

    const auto start = g_sparkLifecycle.BeginStart();
    if (start.status != halljoy::lifecycle::StartStatus::Starting)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"spark", L"start.rejected",
            L"state=%u generation=%llu error=%u restart_blocked=1",
            static_cast<unsigned>(start.state),
            static_cast<unsigned long long>(start.generation.Value()),
            static_cast<unsigned>(start.error.code));
        return false;
    }

#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    const bool injectStopTimeout = commandLine &&
        wcsstr(commandLine, L"--halljoy-test-spark-stop-timeout");
    const bool injectServiceShutdown = commandLine &&
        wcsstr(commandLine, L"--halljoy-test-spark-service-shutdown");
    if (injectStopTimeout || injectServiceShutdown)
    {
        g_sparkStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!g_sparkStopEvent)
        {
            const DWORD error = GetLastError();
            (void)g_sparkLifecycle.FailStartBeforeWorker(start.generation, error);
            return false;
        }
        g_sparkWorkerFaultRecord = {};
        g_sparkWorkerFaultKind.store(
            halljoy::worker::WorkerExceptionKind::None, std::memory_order_release);
        g_sparkWorkerSehCode.store(0, std::memory_order_release);
        g_sparkWorkerExited.store(false, std::memory_order_release);
        DWORD tid = 0;
        g_sparkThread = CreateThread(nullptr, 0, SparkPollThreadProc, nullptr, 0, &tid);
        if (!g_sparkThread)
        {
            const DWORD error = GetLastError();
            CloseHandle(g_sparkStopEvent);
            g_sparkStopEvent = nullptr;
            g_sparkWorkerExited.store(true, std::memory_order_release);
            (void)g_sparkLifecycle.FailStartBeforeWorker(start.generation, error);
            return false;
        }
        const auto running = g_sparkLifecycle.ConfirmRunning(start.generation);
        if (!running.IsRunning())
            return false;
        g_sparkConnectedVid.store(0xFFFF, std::memory_order_relaxed);
        g_sparkConnectedPid.store(0x0001, std::memory_order_relaxed);
        g_sparkConnected.store(true, std::memory_order_release);
        if (!SparkServiceAllowsStart())
        {
            (void)SparkStopLocked();
            return false;
        }
        StabilityTrace_Write(L"WARN", L"spark", L"test.start",
            L"simulator_only=1 generation=%llu stop_timeout=%d service_shutdown=%d",
            static_cast<unsigned long long>(start.generation.Value()),
            injectStopTimeout ? 1 : 0, injectServiceShutdown ? 1 : 0);
        return true;
    }
#endif

    uint16_t inLen = 0, outLen = 0, vid = 0, pid = 0, usagePage = 0;
    bool writeCapable = false;
    std::wstring path;
    HANDLE h = SparkTryOpenDevice(&inLen, &outLen, &path, &writeCapable, &vid, &pid, &usagePage);
    if (!h)
    {
        (void)g_sparkLifecycle.FailStartBeforeWorker(start.generation);
        return false;
    }
    StabilityTrace_Write(L"INFO", L"spark", L"device.open",
        L"vid=0x%04X pid=0x%04X usage_page=0x%04X write_capable=%d",
        (unsigned)vid, (unsigned)pid, (unsigned)usagePage, writeCapable ? 1 : 0);
    if (!writeCapable)
    {
        StabilityTrace_Write(L"WARN", L"spark", L"start.failed", L"stage=write_capability");
        CloseHandle(h);
        (void)g_sparkLifecycle.FailStartBeforeWorker(start.generation);
        return false;
    }

    g_sparkHandle = h;
    g_sparkInputReportLen = std::max<uint16_t>(inLen, (uint16_t)kSparkPayloadSize);
    g_sparkOutputReportLen = std::max<uint16_t>(outLen, (uint16_t)kSparkPayloadSize);
    g_sparkWriteCapable.store(true, std::memory_order_release);
    g_sparkUsagePage.store(usagePage, std::memory_order_relaxed);

    // Prove the SparkLink command family synchronously before publishing the
    // exact VID/PID exclusion to UAP. A vendor-page fingerprint alone is not
    // sufficient because unrelated keyboards may expose similar HID metadata.
    uint8_t routeProbe[kSparkPayloadSize]{};
    if (!SparkQueryDeviceInfo(routeProbe) || routeProbe[2] != 0x01)
    {
        CloseHandle(g_sparkHandle);
        g_sparkHandle = nullptr;
        g_sparkWriteCapable.store(false, std::memory_order_release);
        g_sparkUsagePage.store(0, std::memory_order_relaxed);
        StabilityTrace_Write(L"WARN", L"spark", L"start.failed", L"stage=protocol_probe");
        (void)g_sparkLifecycle.FailStartBeforeWorker(start.generation);
        return false;
    }

    g_sparkStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_sparkStopEvent)
    {
        const DWORD error = GetLastError();
        CloseHandle(g_sparkHandle);
        g_sparkHandle = nullptr;
        StabilityTrace_WriteCritical(L"ERROR", L"spark", L"start.failed", L"stage=create_event win32=%lu", error);
        (void)g_sparkLifecycle.FailStartBeforeWorker(start.generation, error);
        return false;
    }

    g_sparkWorkerFaultRecord = {};
    g_sparkWorkerFaultKind.store(
        halljoy::worker::WorkerExceptionKind::None, std::memory_order_release);
    g_sparkWorkerSehCode.store(0, std::memory_order_release);
    g_sparkWorkerExited.store(false, std::memory_order_release);
    Spark_ResetKeyState();
    g_sparkLastReconnectTryMs = GetTickCount64();
    DWORD tid = 0;
    g_sparkThread = CreateThread(nullptr, 0, SparkPollThreadProc, nullptr, 0, &tid);
    if (!g_sparkThread)
    {
        const DWORD error = GetLastError();
        CloseHandle(g_sparkStopEvent);
        g_sparkStopEvent = nullptr;
        CloseHandle(g_sparkHandle);
        g_sparkHandle = nullptr;
        g_sparkWorkerExited.store(true, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"spark", L"start.failed", L"stage=create_thread win32=%lu", error);
        (void)g_sparkLifecycle.FailStartBeforeWorker(start.generation, error);
        return false;
    }
    if (!NativeAnalogRouting_Claim(vid, pid, NativeAnalogProtocol::SparkLink))
    {
        StabilityTrace_Write(L"WARN", L"spark", L"start.failed", L"stage=routing_claim");
        (void)SparkStopLocked();
        return false;
    }

    if (g_sparkWorkerExited.load(std::memory_order_acquire))
    {
        DebugLog_Write(L"[backend.spark] poller exited during startup");
        StabilityTrace_WriteCritical(L"ERROR", L"spark", L"start.failed", L"stage=worker_early_exit");
        (void)SparkStopLocked();
        return false;
    }
    if (!SparkServiceAllowsStart())
    {
        (void)SparkStopLocked();
        return false;
    }

    g_sparkConnectedVid.store(vid, std::memory_order_relaxed);
    g_sparkConnectedPid.store(pid, std::memory_order_relaxed);
    g_sparkConnected.store(true, std::memory_order_release);
    if (g_sparkWorkerExited.load(std::memory_order_acquire))
    {
        DebugLog_Write(L"[backend.spark] poller exited while publishing startup");
        StabilityTrace_WriteCritical(L"ERROR", L"spark", L"start.failed", L"stage=publish_race");
        (void)SparkStopLocked();
        return false;
    }
    if (g_sparkRestartBlocked.load(std::memory_order_acquire))
    {
        (void)SparkStopLocked();
        return false;
    }
    if (!SparkServiceAllowsStart())
    {
        (void)SparkStopLocked();
        return false;
    }
    const auto running = g_sparkLifecycle.ConfirmRunning(start.generation);
    if (!running.IsRunning())
        return false;
    StabilityTrace_Write(L"INFO", L"spark", L"connected",
        L"vid=0x%04X pid=0x%04X usage_page=0x%04X in_len=%u out_len=%u poll_mode=%u row_limit=%u",
        (unsigned)vid, (unsigned)pid, (unsigned)usagePage,
        (unsigned)g_sparkInputReportLen, (unsigned)g_sparkOutputReportLen,
        (unsigned)Settings_GetSparkPollMode(), (unsigned)Settings_GetSparkRowLimit());
    DebugLog_Write(
        L"[backend.spark] connected vid=0x%04X pid=0x%04X usage_page=0x%04X in_len=%u out_len=%u path=%s",
        (unsigned)vid,
        (unsigned)pid,
        (unsigned)usagePage,
        (unsigned)g_sparkInputReportLen,
        (unsigned)g_sparkOutputReportLen,
        path.c_str());
    return true;
}

static bool SparkStartService()
{
    // The registry owns the long-lived hotplug service, not only the first HID
    // worker. A device may appear later through SparkTickHotplug(), so the outer
    // generation must remain Running and guarantee a final SparkStop() call.
    if (g_sparkRestartBlocked.load(std::memory_order_acquire))
        return false;
    g_sparkServiceStopRequested.store(false, std::memory_order_release);
    g_sparkServiceRunning.store(true, std::memory_order_release);
    (void)SparkStart();
    if (g_sparkRestartBlocked.load(std::memory_order_acquire))
    {
        g_sparkServiceStopRequested.store(true, std::memory_order_release);
        g_sparkServiceRunning.store(false, std::memory_order_release);
        return false;
    }
    StabilityTrace_Write(L"INFO", L"spark", L"service.start");
    return true;
}

static void SparkTickHotplug(ULONGLONG nowMs)
{
    if (!SparkServiceAllowsStart())
        return;
    if (g_sparkConnected.load(std::memory_order_acquire))
    {
        ULONGLONG lastPacket = g_sparkLastPacketMs.load(std::memory_order_relaxed);
        if (lastPacket != 0 && nowMs - lastPacket > kSparkNoPacketRestartMs)
        {
            DebugLog_Write(
                L"[backend.spark] no packets for %llu ms; restarting",
                (unsigned long long)(nowMs - lastPacket));
            StabilityTrace_Write(L"WARN", L"spark", L"hotplug.stale",
                L"silence_ms=%llu", (unsigned long long)(nowMs - lastPacket));
            SparkStop();
        }
        return;
    }

    if (nowMs - g_sparkLastReconnectTryMs < kSparkReconnectIntervalMs)
        return;
    g_sparkLastReconnectTryMs = nowMs;
    if (SparkStart())
    {
        StabilityTrace_Write(L"INFO", L"spark", L"hotplug.reconnect");
        DebugLog_Write(L"[backend.spark] hotplug reconnect ok");
    }
}

static halljoy::lifecycle::StopResult SparkStopService()
{
    const bool wasRunning = g_sparkServiceRunning.load(std::memory_order_acquire);
    g_sparkServiceStopRequested.store(true, std::memory_order_release);
    g_sparkServiceRunning.store(false, std::memory_order_release);
    StabilityTrace_Write(L"INFO", L"spark", L"service.stop.begin",
        L"was_running=%d reconnect_blocked=1", wasRunning ? 1 : 0);

    const auto stopped = SparkStop();
#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    if (commandLine && wcsstr(commandLine, L"--halljoy-test-spark-service-shutdown"))
    {
        SparkTickHotplug(GetTickCount64() + kSparkReconnectIntervalMs);
        StabilityTrace_Write(L"INFO", L"spark", L"test.service_stop_probe",
            L"simulator_only=1 reconnect_blocked=%d",
            !SparkServiceAllowsStart() && !g_sparkConnected.load(std::memory_order_acquire) ? 1 : 0);
    }
#endif
    StabilityTrace_Write(stopped.RestartSafe() ? L"INFO" : L"ERROR",
        L"spark", L"service.stop.end", L"joined=%d reconnect_blocked=1",
        stopped.RestartSafe() ? 1 : 0);
    return stopped;
}
