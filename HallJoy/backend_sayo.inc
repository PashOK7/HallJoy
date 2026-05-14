// ---- native HID path for SayoDevice OSU O3C ----
static constexpr USHORT kSayoVendorId = 0x8089;
static constexpr USHORT kSayoProductId = 0x0009;
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
static std::atomic<uint8_t>       g_sayoLastKeyboardHid{ 0 };
static std::atomic<ULONGLONG>     g_sayoLastKeyboardHidMs{ 0 };
static std::atomic<uint8_t>       g_sayoPendingIndex{ 0xFF };
static std::atomic<ULONGLONG>     g_sayoPendingIndexMs{ 0 };
static std::atomic<ULONGLONG>     g_sayoLastDepthMs{ 0 };
static std::atomic<int>           g_sayoReaderCount{ 0 };

static void Sayo_ResetKeyState()
{
    for (auto& m : g_sayoAnalogMilli)
        m.store(0, std::memory_order_relaxed);
    for (auto& h : g_sayoIndexToHid)
        h.store(0, std::memory_order_relaxed);
    for (auto& d : g_sayoIndexDown)
        d.store(0, std::memory_order_relaxed);
    g_sayoLastKeyboardHid.store(0, std::memory_order_relaxed);
    g_sayoLastKeyboardHidMs.store(0, std::memory_order_relaxed);
    g_sayoPendingIndex.store(0xFF, std::memory_order_relaxed);
    g_sayoPendingIndexMs.store(0, std::memory_order_relaxed);
    g_sayoLastDepthMs.store(0, std::memory_order_relaxed);
    g_sayoLastPacketMs.store(0, std::memory_order_relaxed);

    // F/G/H are only a fallback for the current O3C capture. The runtime remaps
    // these indices from normal keyboard HID reports when the device is pressed.
    g_sayoIndexToHid[0].store(0x09, std::memory_order_relaxed);
    g_sayoIndexToHid[1].store(0x0A, std::memory_order_relaxed);
    g_sayoIndexToHid[2].store(0x0B, std::memory_order_relaxed);
}

static void SayoSetIndexState(uint8_t index, bool down, ULONGLONG nowMs)
{
    if (index >= g_sayoIndexDown.size())
        return;

    uint8_t mappedHid = g_sayoIndexToHid[index].load(std::memory_order_relaxed);
    uint8_t keyboardHid = g_sayoLastKeyboardHid.load(std::memory_order_relaxed);
    ULONGLONG keyboardMs = g_sayoLastKeyboardHidMs.load(std::memory_order_relaxed);
    if (down && keyboardHid != 0 && nowMs - keyboardMs <= kSayoKeyboardMatchWindowMs)
    {
        mappedHid = keyboardHid;
        g_sayoIndexToHid[index].store(mappedHid, std::memory_order_relaxed);
        DebugLog_Write(
            L"[backend.sayo] mapped index=%u hid=%u from keyboard report",
            (unsigned)index,
            (unsigned)mappedHid);
    }

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
}

static void SayoParseKeyboardReport(const uint8_t* data, DWORD len, ULONGLONG nowMs)
{
    if (!data || len < 8)
        return;

    uint8_t hid = 0;
    for (DWORD i = 2; i < len && i < 8; ++i)
    {
        if (data[i] != 0)
        {
            hid = data[i];
            break;
        }
    }

    if (hid != 0)
    {
        g_sayoLastKeyboardHid.store(hid, std::memory_order_relaxed);
        g_sayoLastKeyboardHidMs.store(nowMs, std::memory_order_relaxed);

        uint8_t pending = g_sayoPendingIndex.load(std::memory_order_relaxed);
        ULONGLONG pendingMs = g_sayoPendingIndexMs.load(std::memory_order_relaxed);
        if (pending < g_sayoIndexToHid.size() && nowMs - pendingMs <= kSayoKeyboardMatchWindowMs)
        {
            uint8_t oldHid = g_sayoIndexToHid[pending].exchange(hid, std::memory_order_relaxed);
            if (oldHid != 0 && oldHid != hid)
                g_sayoAnalogMilli[oldHid].store(0, std::memory_order_relaxed);
            ULONGLONG lastDepth = g_sayoLastDepthMs.load(std::memory_order_relaxed);
            bool depthFresh = (lastDepth != 0 && nowMs - lastDepth <= kSayoDepthFreshMs);
            if (!depthFresh && g_sayoIndexDown[pending].load(std::memory_order_relaxed) != 0)
                g_sayoAnalogMilli[hid].store(1000, std::memory_order_relaxed);
            g_sayoPendingIndex.store(0xFF, std::memory_order_relaxed);
            DebugLog_Write(
                L"[backend.sayo] mapped pending index=%u hid=%u from keyboard report",
                (unsigned)pending,
                (unsigned)hid);
        }
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
            if (SayoSendDepthPoll(reader))
            {
                pollFailStreak = 0;
            }
            else if (++pollFailStreak == 5)
            {
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
        else if (got >= 8 && buf[0] == 0x00)
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
            attr.VendorID != kSayoVendorId ||
            attr.ProductID != kSayoProductId)
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
        reader.writeCapable = writeCapable;
        outReaders->push_back(reader);

        DebugLog_Write(
            L"[backend.sayo] candidate path=%s write_capable=%d in_len=%u out_len=%u usage_page=0x%04X usage=0x%04X",
            detail->DevicePath,
            writeCapable ? 1 : 0,
            (unsigned)inLen,
            (unsigned)outLen,
            (unsigned)usagePage,
            (unsigned)usage);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return !outReaders->empty();
}

static void SayoStop()
{
    if (g_sayoStopEvent)
        SetEvent(g_sayoStopEvent);

    for (auto& reader : g_sayoReaders)
    {
        if (reader.handle)
            CancelIoEx(reader.handle, nullptr);
    }

    for (auto& reader : g_sayoReaders)
    {
        if (reader.thread)
        {
            WaitForSingleObject(reader.thread, 1000);
            CloseHandle(reader.thread);
        }
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
    g_sayoConnected.store(false, std::memory_order_release);
}

static bool SayoStart()
{
    if (g_sayoConnected.load(std::memory_order_acquire))
        return true;
    if (g_sayoStopEvent)
        SayoStop();

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

    g_sayoReaderCount.store(started, std::memory_order_relaxed);
    g_sayoLastReconnectTryMs = GetTickCount64();
    g_sayoConnectedVid.store(kSayoVendorId, std::memory_order_relaxed);
    g_sayoConnectedPid.store(kSayoProductId, std::memory_order_relaxed);
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
