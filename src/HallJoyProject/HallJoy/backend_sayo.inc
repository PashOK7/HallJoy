// ---- native HID path for SayoDevice O3C-compatible protocol ----
static constexpr USHORT kSayoVendorId = 0x8089;
static constexpr USHORT kSayoKnownProductId = 0x0009;
static constexpr size_t kSayoMaxDevices = 8;
static constexpr ULONGLONG kSayoReconnectIntervalMs = 2000;
static constexpr ULONGLONG kSayoNoPacketRestartMs = 1800;
static constexpr ULONGLONG kSayoKeyboardMatchWindowMs = 80;
static constexpr ULONGLONG kSayoPollIntervalMs = 8;
static constexpr ULONGLONG kSayoDepthFreshMs = 160;
static constexpr uint16_t kSayoRawFullScale = 4000;

struct SayoReader
{
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
static std::array<std::atomic<uint16_t>, 256> g_sayoAnalogMilli{};
static std::array<std::atomic<uint8_t>, 16>   g_sayoIndexToHid{};
static std::array<std::atomic<uint8_t>, 16>   g_sayoIndexDown{};
static std::array<std::atomic<uint8_t>, 256>  g_sayoKeyboardDown{};
static std::atomic<uint8_t>       g_sayoPendingIndex{ 0xFF };
static std::atomic<ULONGLONG>     g_sayoPendingIndexMs{ 0 };
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
    for (auto& m : g_sayoAnalogMilli)
        m.store(0, std::memory_order_relaxed);
    for (auto& h : g_sayoIndexToHid)
        h.store(0, std::memory_order_relaxed);
    for (auto& d : g_sayoIndexDown)
        d.store(0, std::memory_order_relaxed);
    for (auto& d : g_sayoKeyboardDown)
        d.store(0, std::memory_order_relaxed);
    g_sayoPendingIndex.store(0xFF, std::memory_order_relaxed);
    g_sayoPendingIndexMs.store(0, std::memory_order_relaxed);
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

    // F/G/H are only a fallback for the current O3C capture. The runtime remaps
    // these indices from normal keyboard HID reports when the device is pressed.
    g_sayoIndexToHid[0].store(0x09, std::memory_order_relaxed);
    g_sayoIndexToHid[1].store(0x0A, std::memory_order_relaxed);
    g_sayoIndexToHid[2].store(0x0B, std::memory_order_relaxed);
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

static void SayoSetIndexState(uint8_t index, bool down, ULONGLONG nowMs)
{
    if (index >= g_sayoIndexDown.size())
        return;

    uint8_t mappedHid = g_sayoIndexToHid[index].load(std::memory_order_relaxed);

    if (mappedHid == 0 || mappedHid >= 256)
        return;

    g_sayoIndexDown[index].store(down ? 1 : 0, std::memory_order_relaxed);
    ULONGLONG lastDepth = g_sayoLastDepthMs.load(std::memory_order_relaxed);
    bool depthFresh = (lastDepth != 0 && nowMs - lastDepth <= kSayoDepthFreshMs);
    if (!down)
    {
        g_sayoAnalogMilli[mappedHid].store(0, std::memory_order_relaxed);
    }
    else if (!depthFresh)
    {
        g_sayoAnalogMilli[mappedHid].store(1000, std::memory_order_relaxed);
    }
    if (down)
    {
        g_sayoPendingIndex.store(index, std::memory_order_relaxed);
        g_sayoPendingIndexMs.store(nowMs, std::memory_order_relaxed);
    }
    DebugLog_Write(
        L"[backend.sayo] edge index=%u hid=%u down=%d",
        (unsigned)index,
        (unsigned)mappedHid,
        down ? 1 : 0);
    RealtimeLoop_NotifyInputChanged();
}

static void SayoParseKeyboardReport(const uint8_t* data, DWORD len, ULONGLONG nowMs)
{
    if (!data || len < 8)
        return;

    bool current[256]{};
    uint8_t addedHid = 0;
    int addedCount = 0;
    DWORD keyStart = (data[0] == 0x00 && len >= 9) ? 3u : 2u;
    DWORD keyEnd = (data[0] == 0x00 && len >= 9) ? 9u : 8u;
    for (DWORD i = keyStart; i < len && i < keyEnd; ++i)
    {
        uint8_t hid = data[i];
        if (hid == 0)
            continue;

        current[hid] = true;
        if (g_sayoKeyboardDown[hid].load(std::memory_order_relaxed) == 0)
        {
            addedHid = hid;
            ++addedCount;
        }
    }

    for (uint16_t hid = 0; hid < 256; ++hid)
        g_sayoKeyboardDown[hid].store(current[hid] ? 1 : 0, std::memory_order_relaxed);

    uint8_t pending = g_sayoPendingIndex.load(std::memory_order_relaxed);
    ULONGLONG pendingMs = g_sayoPendingIndexMs.load(std::memory_order_relaxed);
    if (pending < g_sayoIndexToHid.size() &&
        nowMs - pendingMs <= kSayoKeyboardMatchWindowMs &&
        addedCount == 1 &&
        addedHid != 0)
    {
        uint8_t oldHid = g_sayoIndexToHid[pending].exchange(addedHid, std::memory_order_relaxed);
        if (oldHid != 0 && oldHid != addedHid)
            g_sayoAnalogMilli[oldHid].store(0, std::memory_order_relaxed);
        ULONGLONG lastDepth = g_sayoLastDepthMs.load(std::memory_order_relaxed);
        bool depthFresh = (lastDepth != 0 && nowMs - lastDepth <= kSayoDepthFreshMs);
        if (!depthFresh && g_sayoIndexDown[pending].load(std::memory_order_relaxed) != 0)
            g_sayoAnalogMilli[addedHid].store(1000, std::memory_order_relaxed);
        g_sayoPendingIndex.store(0xFF, std::memory_order_relaxed);
        DebugLog_Write(
            L"[backend.sayo] mapped pending index=%u hid=%u from keyboard add",
            (unsigned)pending,
            (unsigned)addedHid);
        RealtimeLoop_NotifyInputChanged();
    }
}

static void SayoParseAnalogReport(const uint8_t* data, DWORD len, ULONGLONG nowMs)
{
    if (!data || len < 12 || data[0] != 0x21)
        return;

    // Observed from SayoDevice OSU O3C web-driver captures:
    // byte 8: 0x10 = key down edge, 0x11 = key up edge
    // byte 9: physical key index (0..2 on O3C)
    if ((data[8] == 0x10 || data[8] == 0x11) && data[9] < 16)
    {
        SayoSetIndexState(data[9], data[8] == 0x10, nowMs);
        return;
    }
}

static void SayoParseDepthResponse(const uint8_t* data, DWORD len, ULONGLONG nowMs)
{
    if (!data || len < 14 || data[0] != 0x22)
        return;

    // Observed in SayoDevice OSU O3C web-driver captures. The browser polls
    // command 22 12 3c 13..., and the 1024-byte IN response carries the three
    // top-button depth values at u16 offsets 8/10/12. Raw full press is ~4000.
    uint16_t raw[3] = {
        (uint16_t)(data[8] | (data[9] << 8)),
        (uint16_t)(data[10] | (data[11] << 8)),
        (uint16_t)(data[12] | (data[13] << 8)),
    };

    for (uint8_t index = 0; index < 3; ++index)
    {
        SayoRecordObservedDepth(index, raw[index]);
        uint8_t hid = g_sayoIndexToHid[index].load(std::memory_order_relaxed);
        if (hid == 0 || hid >= 256)
            continue;

        uint32_t milli = ((uint32_t)raw[index] * 1000u + (kSayoRawFullScale / 2u)) / kSayoRawFullScale;
        if (milli > 1000u)
            milli = 1000u;
        if (milli < 4u)
            milli = 0u;
        g_sayoAnalogMilli[hid].store((uint16_t)milli, std::memory_order_relaxed);
    }
    g_sayoLastDepthMs.store(nowMs, std::memory_order_relaxed);
    g_sayoDepthPackets.fetch_add(1, std::memory_order_relaxed);
    SayoRecordDepthTiming();
    RealtimeLoop_NotifyInputChanged();
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
        if (got < 14 || response[0] != 0x22)
            continue;

        const uint16_t raw0 = static_cast<uint16_t>(response[8] | (response[9] << 8));
        const uint16_t raw1 = static_cast<uint16_t>(response[10] | (response[11] << 8));
        const uint16_t raw2 = static_cast<uint16_t>(response[12] | (response[13] << 8));
        const uint16_t plausibleMax = static_cast<uint16_t>(kSayoRawFullScale * 2u);
        return raw0 <= plausibleMax && raw1 <= plausibleMax && raw2 <= plausibleMax;
    }
    return false;
}

static DWORD WINAPI SayoReaderThreadProc(LPVOID param)
{
    SayoReader* reader = (SayoReader*)param;
    if (!reader || !reader->handle || !g_sayoStopEvent)
        return 0;

    const DWORD readLen = std::max<DWORD>(reader->inputLen, 8);
    std::vector<uint8_t> buf(readLen, 0);
    DebugLog_Write(
        L"[backend.sayo] reader start in_len=%u usage_page=0x%04X usage=0x%04X",
        (unsigned)reader->inputLen,
        (unsigned)reader->usagePage,
        (unsigned)reader->usage);

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
        if (got >= 64 && buf[0] == 0x21)
            SayoParseAnalogReport(buf.data(), got, nowMs);
        else if (got >= 14 && buf[0] == 0x22)
            SayoParseDepthResponse(buf.data(), got, nowMs);
        else if (got >= 8 && buf[0] == 0x00 && reader->usagePage == 0x0001 && reader->usage == 0x0006)
            SayoParseKeyboardReport(buf.data(), got, nowMs);
    }

    DebugLog_Write(L"[backend.sayo] reader stop");
    return 0;
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
        const bool routedToSayo = NativeAnalogRouting_IsClaimedBy(
            attr.VendorID, attr.ProductID, NativeAnalogProtocol::SayoDepth);
        if (NativeAnalogRouting_IsClaimed(attr.VendorID, attr.ProductID) && !routedToSayo)
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

        HidD_SetNumInputBuffers(h, 64);
        SayoReader reader{};
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

    for (auto it = outReaders->begin(); it != outReaders->end();)
    {
        if (it->productId != selectedPid)
        {
            if (it->handle) CloseHandle(it->handle);
            it = outReaders->erase(it);
        }
        else
        {
            ++it;
        }
    }
    return !outReaders->empty();
}

static bool SayoStop()
{
    g_sayoConnected.store(false, std::memory_order_release);
    if (g_sayoStopEvent)
        SetEvent(g_sayoStopEvent);

    for (auto& reader : g_sayoReaders)
    {
        if (reader.handle)
            CancelIoEx(reader.handle, nullptr);
    }

    bool allJoined = true;
    for (auto& reader : g_sayoReaders)
    {
        if (!reader.thread)
            continue;
        DWORD wr = WaitForSingleObject(reader.thread, 3000);
        if (wr != WAIT_OBJECT_0)
        {
            DebugLog_Write(L"[backend.sayo] stop join timeout wait=%lu err=%lu; forcing shutdown-only termination",
                (unsigned long)wr, (unsigned long)GetLastError());
            TerminateThread(reader.thread, 0xE0515341u);
            wr = WaitForSingleObject(reader.thread, 1000);
            if (wr != WAIT_OBJECT_0)
                allJoined = false;
        }
    }
    if (!allJoined)
        DebugLog_Write(L"[backend.sayo] one or more forced worker waits did not confirm completion; closing process-exit resources");

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
    return true;
}

static bool SayoStart()
{
    if (g_sayoConnected.load(std::memory_order_acquire))
        return true;
    if (g_sayoStopEvent && !SayoStop())
        return false;

    std::vector<SayoReader> readers;
    if (!SayoCollectMatchingDevices(&readers))
        return false;

    g_sayoStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_sayoStopEvent)
    {
        for (auto& r : readers)
            if (r.handle) CloseHandle(r.handle);
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
    for (size_t i = 0; i < readers.size() && i < g_sayoReaders.size(); ++i)
    {
        g_sayoReaders[i] = readers[i];
        DWORD tid = 0;
        g_sayoReaders[i].thread = CreateThread(nullptr, 0, SayoReaderThreadProc, &g_sayoReaders[i], 0, &tid);
        if (!g_sayoReaders[i].thread)
        {
            CloseHandle(g_sayoReaders[i].handle);
            g_sayoReaders[i] = SayoReader{};
            continue;
        }
        ++started;
    }

    if (started == 0)
    {
        SayoStop();
        return false;
    }
    if (!NativeAnalogRouting_Claim(
            readers.front().vendorId, readers.front().productId,
            NativeAnalogProtocol::SayoDepth))
    {
        SayoStop();
        return false;
    }

    g_sayoReaderCount.store(started, std::memory_order_relaxed);
    g_sayoLastReconnectTryMs = GetTickCount64();
    g_sayoConnectedVid.store(readers.front().vendorId, std::memory_order_relaxed);
    g_sayoConnectedPid.store(readers.front().productId, std::memory_order_relaxed);
    g_sayoConnected.store(true, std::memory_order_release);
    DebugLog_Write(L"[backend.sayo] connected readers=%d", started);
    return true;
}

static void SayoTickHotplug(ULONGLONG nowMs)
{
    if (g_sayoConnected.load(std::memory_order_acquire))
    {
        ULONGLONG lastPacket = g_sayoLastPacketMs.load(std::memory_order_relaxed);
        if (lastPacket != 0 && nowMs - lastPacket > kSayoNoPacketRestartMs)
        {
            DebugLog_Write(
                L"[backend.sayo] no packets for %llu ms; restarting",
                (unsigned long long)(nowMs - lastPacket));
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
