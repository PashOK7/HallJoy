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

static bool SparkWriteFileOverlapped(HANDLE h, const uint8_t* data, DWORD len, DWORD timeoutMs, DWORD* outErr)
{
    if (!h || h == INVALID_HANDLE_VALUE || !data || len == 0)
    {
        if (outErr) *outErr = ERROR_INVALID_PARAMETER;
        return false;
    }

    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ev)
    {
        if (outErr) *outErr = GetLastError();
        return false;
    }

    OVERLAPPED ov{};
    ov.hEvent = ev;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, len, nullptr, &ov);
    if (!ok)
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            DWORD wr = WaitForSingleObject(ev, timeoutMs);
            if (wr == WAIT_OBJECT_0)
            {
                if (!GetOverlappedResult(h, &ov, &written, FALSE))
                {
                    err = GetLastError();
                    ok = FALSE;
                }
                else
                {
                    ok = TRUE;
                }
            }
            else
            {
                CancelIoEx(h, &ov);
                err = (wr == WAIT_TIMEOUT) ? WAIT_TIMEOUT : GetLastError();
                ok = FALSE;
            }
        }
        if (!ok && outErr) *outErr = err;
    }
    else
    {
        if (!GetOverlappedResult(h, &ov, &written, TRUE))
        {
            if (outErr) *outErr = GetLastError();
            ok = FALSE;
        }
    }

    CloseHandle(ev);
    return (ok != FALSE && written == len);
}

static bool SparkReadFileOverlapped(HANDLE h, uint8_t* data, DWORD len, DWORD timeoutMs, DWORD* outRead, DWORD* outErr)
{
    if (!h || h == INVALID_HANDLE_VALUE || !data || len == 0)
    {
        if (outErr) *outErr = ERROR_INVALID_PARAMETER;
        return false;
    }

    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ev)
    {
        if (outErr) *outErr = GetLastError();
        return false;
    }

    OVERLAPPED ov{};
    ov.hEvent = ev;
    DWORD readBytes = 0;
    BOOL ok = ReadFile(h, data, len, nullptr, &ov);
    if (!ok)
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            DWORD wr = WaitForSingleObject(ev, timeoutMs);
            if (wr == WAIT_OBJECT_0)
            {
                if (!GetOverlappedResult(h, &ov, &readBytes, FALSE))
                {
                    err = GetLastError();
                    ok = FALSE;
                }
                else
                {
                    ok = TRUE;
                }
            }
            else
            {
                CancelIoEx(h, &ov);
                err = (wr == WAIT_TIMEOUT) ? WAIT_TIMEOUT : GetLastError();
                ok = FALSE;
            }
        }
        if (!ok && outErr) *outErr = err;
    }
    else
    {
        if (!GetOverlappedResult(h, &ov, &readBytes, TRUE))
        {
            if (outErr) *outErr = GetLastError();
            ok = FALSE;
        }
    }

    if (outRead) *outRead = readBytes;
    CloseHandle(ev);
    return (ok != FALSE && readBytes != 0);
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
    std::vector<uint8_t> out((size_t)g_sparkOutputReportLen, 0);
    size_t offset = (g_sparkOutputReportLen > kSparkPayloadSize) ? 1u : 0u;
    if (offset + kSparkPayloadSize <= out.size())
    {
        memcpy(out.data() + offset, payload, kSparkPayloadSize);
        if (SparkWriteFileOverlapped(g_sparkHandle, out.data(), (DWORD)out.size(), 250, &lastErr))
            return true;
    }

    std::vector<uint8_t> outRid((size_t)kSparkPayloadSize + 1, 0);
    memcpy(outRid.data() + 1, payload, kSparkPayloadSize);
    if (SparkWriteFileOverlapped(g_sparkHandle, outRid.data(), (DWORD)outRid.size(), 250, &lastErr))
        return true;

    if (HidD_SetOutputReport(g_sparkHandle, out.data(), (ULONG)out.size()))
        return true;
    if (HidD_SetOutputReport(g_sparkHandle, outRid.data(), (ULONG)outRid.size()))
        return true;

    if (HidD_SetFeature(g_sparkHandle, out.data(), (ULONG)out.size()))
        return true;
    if (HidD_SetFeature(g_sparkHandle, outRid.data(), (ULONG)outRid.size()))
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
    std::vector<uint8_t> inBuf((size_t)std::max<uint16_t>(g_sparkInputReportLen, (uint16_t)(kSparkPayloadSize + 1u)), 0);
    while (GetTickCount64() < deadline)
    {
        DWORD remain = (DWORD)std::clamp<ULONGLONG>(deadline - GetTickCount64(), 1, 80);
        DWORD readBytes = 0;
        DWORD err = 0;
        if (!SparkReadFileOverlapped(g_sparkHandle, inBuf.data(), (DWORD)inBuf.size(), remain, &readBytes, &err))
        {
            if (err == WAIT_TIMEOUT || err == ERROR_IO_INCOMPLETE)
                continue;
            if (err == ERROR_OPERATION_ABORTED || err == ERROR_INVALID_HANDLE || err == ERROR_DEVICE_NOT_CONNECTED)
                break;
            continue;
        }

        const uint8_t* p = SparkFindPacket(inBuf.data(), readBytes, b0, b1, b2, b3);
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
    while (WaitForSingleObject(g_sparkStopEvent, 0) != WAIT_OBJECT_0)
    {
        std::array<uint16_t, 256> nextMilli{};
        bool anyRowOk = false;

        rowCount = std::clamp(g_sparkRowCount.load(std::memory_order_relaxed), 0, kSparkMaxRows);
        for (uint8_t row = 0; row < (uint8_t)rowCount; ++row)
        {
            if (WaitForSingleObject(g_sparkStopEvent, 0) == WAIT_OBJECT_0)
                break;
            if (g_sparkRowActive[(size_t)row].load(std::memory_order_relaxed) == 0)
                continue;

            std::array<uint16_t, kSparkColsPerRow> route{};
            if (!SparkQueryRouteRow(row, route))
                continue;

            anyRowOk = true;
            for (int col = 0; col < kSparkColsPerRow; ++col)
            {
                uint8_t hid = g_sparkRowColToHid[(size_t)row * kSparkColsPerRow + (size_t)col].load(std::memory_order_relaxed);
                if (hid == 0)
                    continue;
                uint16_t milli = SparkNormalizeRouteToMilli(hid, route[(size_t)col]);
                if (milli > nextMilli[hid])
                    nextMilli[hid] = milli;
            }
        }

        if (!anyRowOk)
        {
            ++failStreak;
            if (failStreak >= 6)
                break;
            Sleep(15);
            continue;
        }

        failStreak = 0;
        for (uint16_t hid = 1; hid < 256; ++hid)
            g_sparkAnalogMilli[hid].store(nextMilli[hid], std::memory_order_relaxed);
        ULONGLONG nowMs = GetTickCount64();
        g_sparkLastPacketMs.store(nowMs, std::memory_order_relaxed);

        ULONGLONG lastActiveLog = g_sparkLastActiveLogMs.load(std::memory_order_relaxed);
        if (nowMs - lastActiveLog >= 1200)
        {
            g_sparkLastActiveLogMs.store(nowMs, std::memory_order_relaxed);
            SparkLogActiveKeys(nextMilli);
        }

        UINT pollMs = std::max<UINT>(1u, Settings_GetPollingMs() / (UINT)std::max(rowCount, 1));
        Sleep(pollMs);
    }

    for (auto& m : g_sparkAnalogMilli)
        m.store(0, std::memory_order_relaxed);
    g_sparkConnected.store(false, std::memory_order_release);
    DebugLog_Write(L"[backend.spark] poller stop");
    return 0;
}

static DWORD WINAPI SparkPollThreadProc(LPVOID)
{
    __try
    {
        return SparkPollThreadProcImpl();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DWORD code = GetExceptionCode();
        DebugLog_Write(L"[backend.spark] poller crash code=0x%08lX", code);
        g_sparkConnected.store(false, std::memory_order_release);
        return 0;
    }
}

static void SparkStop()
{
    if (g_sparkStopEvent)
        SetEvent(g_sparkStopEvent);
    if (g_sparkHandle)
        CancelIoEx(g_sparkHandle, nullptr);

    if (g_sparkThread)
    {
        WaitForSingleObject(g_sparkThread, 1500);
        CloseHandle(g_sparkThread);
        g_sparkThread = nullptr;
    }

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
    g_sparkConnected.store(false, std::memory_order_release);
}

static bool SparkStart()
{
    if (g_sparkConnected.load(std::memory_order_acquire))
        return true;
    if (g_sparkThread || g_sparkHandle)
        SparkStop();

    uint16_t inLen = 0, outLen = 0, vid = 0, pid = 0, usagePage = 0;
    bool writeCapable = false;
    std::wstring path;
    HANDLE h = SparkTryOpenDevice(&inLen, &outLen, &path, &writeCapable, &vid, &pid, &usagePage);
    if (!h)
        return false;
    if (!writeCapable)
    {
        CloseHandle(h);
        return false;
    }

    g_sparkHandle = h;
    g_sparkInputReportLen = std::max<uint16_t>(inLen, (uint16_t)kSparkPayloadSize);
    g_sparkOutputReportLen = std::max<uint16_t>(outLen, (uint16_t)kSparkPayloadSize);
    g_sparkWriteCapable.store(true, std::memory_order_release);
    g_sparkUsagePage.store(usagePage, std::memory_order_relaxed);
    g_sparkStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_sparkStopEvent)
    {
        CloseHandle(g_sparkHandle);
        g_sparkHandle = nullptr;
        return false;
    }

    Spark_ResetKeyState();
    g_sparkLastReconnectTryMs = GetTickCount64();
    DWORD tid = 0;
    g_sparkThread = CreateThread(nullptr, 0, SparkPollThreadProc, nullptr, 0, &tid);
    if (!g_sparkThread)
    {
        CloseHandle(g_sparkStopEvent);
        g_sparkStopEvent = nullptr;
        CloseHandle(g_sparkHandle);
        g_sparkHandle = nullptr;
        return false;
    }

    g_sparkConnected.store(true, std::memory_order_release);
    g_sparkConnectedVid.store(vid, std::memory_order_relaxed);
    g_sparkConnectedPid.store(pid, std::memory_order_relaxed);
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

static void SparkTickHotplug(ULONGLONG nowMs)
{
    if (g_sparkConnected.load(std::memory_order_acquire))
    {
        ULONGLONG lastPacket = g_sparkLastPacketMs.load(std::memory_order_relaxed);
        if (lastPacket != 0 && nowMs - lastPacket > kSparkNoPacketRestartMs)
        {
            DebugLog_Write(
                L"[backend.spark] no packets for %llu ms; restarting",
                (unsigned long long)(nowMs - lastPacket));
            SparkStop();
        }
        return;
    }

    if (nowMs - g_sparkLastReconnectTryMs < kSparkReconnectIntervalMs)
        return;
    g_sparkLastReconnectTryMs = nowMs;
    if (SparkStart())
        DebugLog_Write(L"[backend.spark] hotplug reconnect ok");
}
