// ---- native HID path for Aula analog keyboards ----
static constexpr USHORT kAulaVendorId = 0x372E;
static constexpr uint16_t kAulaKnownUsagePage = 0xFF60;
static constexpr uint16_t kAulaKnownUsage = 0x0061;
static constexpr size_t kAulaPayloadSize = 64;
static constexpr ULONGLONG kAulaReconnectIntervalMs = 2000;
static constexpr ULONGLONG kAulaEnableRetryMs = 1500;
static constexpr ULONGLONG kAulaNoPacketRetryMs = 3000;
static constexpr ULONGLONG kAulaKeyStaleMs = 1500;
static constexpr ULONGLONG kAulaCalibPollIntervalMs = 180;
static constexpr ULONGLONG kAulaCalibBootstrapRetryMs = 1800;
static constexpr ULONGLONG kAulaCalibNoPacketMs = 1200;
static constexpr ULONGLONG kAulaPreferCalibOver98WindowMs = 700;

static const wchar_t* AulaCommModeName(UINT mode)
{
    switch (mode)
    {
    case SettingsAulaCommMode_98Only: return L"98_only";
    case SettingsAulaCommMode_94Passive: return L"94_passive";
    case SettingsAulaCommMode_94ActiveExperimental: return L"94_active_experimental";
    default: return L"unknown";
    }
}

static bool AulaAllowPassive94Reports()
{
    return Settings_GetAulaCommMode() != (UINT)SettingsAulaCommMode_98Only;
}

static int AulaUsageSupportScore(uint16_t usagePage, uint16_t usage)
{
    if (usagePage == kAulaKnownUsagePage && usage == kAulaKnownUsage)
        return 3;
    if (usagePage == kAulaKnownUsagePage && usage != 0)
        return 2;
    if ((usagePage & 0xFF00u) == 0xFF00u && usage != 0)
        return 1;
    return 0;
}

struct AulaKeyCalib
{
    uint16_t releaseRaw = 43000; // larger = key up
    uint16_t pressRaw = 34900;   // smaller = key down
    bool seen = false;
};

static HANDLE                     g_aulaHandle = nullptr;
static HANDLE                     g_aulaThread = nullptr;
static HANDLE                     g_aulaStopEvent = nullptr;
static std::atomic<bool>          g_aulaConnected{ false };
static std::atomic<uint16_t>      g_aulaConnectedVid{ 0 };
static std::atomic<uint16_t>      g_aulaConnectedPid{ 0 };
static std::atomic<ULONGLONG>     g_aulaLastPacketMs{ 0 };
static std::atomic<ULONGLONG>     g_aulaLastEnableTryMs{ 0 };
static ULONGLONG                  g_aulaLastReconnectTryMs = 0;
static std::array<std::atomic<uint16_t>, 256> g_aulaAnalogMilli{};
static std::array<std::atomic<ULONGLONG>, 256> g_aulaLastKeyUpdateMs{};
static std::array<AulaKeyCalib, 256>          g_aulaCalib{};
static std::array<std::atomic<uint8_t>, 256>  g_aulaKeyIdToHid{};
static std::array<std::atomic<uint8_t>, 256>  g_aulaLastFull{};
static uint16_t                   g_aulaInputReportLen = (uint16_t)kAulaPayloadSize;
static uint16_t                   g_aulaOutputReportLen = (uint16_t)kAulaPayloadSize;
static std::atomic<bool>          g_aulaWriteCapable{ false };
static std::atomic<ULONGLONG>     g_aulaLastUnknownLogMs{ 0 };
static std::atomic<ULONGLONG>     g_aulaLastCalibPacketMs{ 0 };
static std::atomic<ULONGLONG>     g_aulaLastCalibPollMs{ 0 };
static std::atomic<ULONGLONG>     g_aulaLastCalibBootstrapMs{ 0 };
static std::atomic<UINT>          g_aulaLastCommMode{ (UINT)-1 };
static std::array<std::atomic<uint8_t>, 256> g_physicalDown{};

static void Aula_ResetKeyState()
{
    for (auto& m : g_aulaAnalogMilli)
        m.store(0, std::memory_order_relaxed);
    for (auto& t : g_aulaLastKeyUpdateMs)
        t.store(0, std::memory_order_relaxed);
    for (auto& c : g_aulaCalib)
    {
        c.releaseRaw = 43000;
        c.pressRaw = 34900;
        c.seen = false;
    }
    for (auto& f : g_aulaLastFull)
        f.store(0, std::memory_order_relaxed);
    g_aulaLastPacketMs.store(0, std::memory_order_relaxed);
    g_aulaLastUnknownLogMs.store(0, std::memory_order_relaxed);
    g_aulaLastEnableTryMs.store(0, std::memory_order_relaxed);
    g_aulaLastCalibPacketMs.store(0, std::memory_order_relaxed);
    g_aulaLastCalibPollMs.store(0, std::memory_order_relaxed);
    g_aulaLastCalibBootstrapMs.store(0, std::memory_order_relaxed);
}

static void Aula_InitDefaultKeyMap()
{
    for (auto& m : g_aulaKeyIdToHid)
        m.store(0, std::memory_order_relaxed);

    // Aula 84 HE key_id -> USB HID mapping derived from captured 0x83 map responses.
    static constexpr struct { uint8_t keyId; uint8_t hid; } kPairs[] = {
        { 0x01, 0x29 }, { 0x02, 0x3A }, { 0x03, 0x3B }, { 0x04, 0x3C }, { 0x05, 0x3D }, { 0x06, 0x3E }, { 0x07, 0x3F }, { 0x08, 0x40 },
        { 0x09, 0x41 }, { 0x0A, 0x42 }, { 0x0B, 0x43 }, { 0x0C, 0x44 }, { 0x0D, 0x45 }, { 0x0E, 0x35 }, { 0x0F, 0x1E }, { 0x10, 0x1F },
        { 0x11, 0x20 }, { 0x12, 0x21 }, { 0x13, 0x22 }, { 0x14, 0x23 }, { 0x15, 0x24 }, { 0x16, 0x25 }, { 0x17, 0x26 }, { 0x18, 0x27 },
        { 0x19, 0x2D }, { 0x1A, 0x2E }, { 0x1B, 0x2A }, { 0x1C, 0x2B }, { 0x1D, 0x14 }, { 0x1E, 0x1A }, { 0x1F, 0x08 }, { 0x20, 0x15 },
        { 0x21, 0x17 }, { 0x22, 0x1C }, { 0x23, 0x18 }, { 0x24, 0x0C }, { 0x25, 0x12 }, { 0x26, 0x13 }, { 0x27, 0x2F }, { 0x28, 0x30 },
        { 0x29, 0x31 }, { 0x2A, 0x39 }, { 0x2B, 0x04 }, { 0x2C, 0x16 }, { 0x2D, 0x07 }, { 0x2E, 0x09 }, { 0x2F, 0x0A }, { 0x30, 0x0B },
        { 0x31, 0x0D }, { 0x32, 0x0E }, { 0x33, 0x0F }, { 0x34, 0x33 }, { 0x35, 0x34 }, { 0x36, 0x28 }, { 0x38, 0x1D }, { 0x39, 0x1B },
        { 0x37, 0xE1 }, { 0x3A, 0x06 }, { 0x3B, 0x19 }, { 0x3C, 0x05 }, { 0x3D, 0x11 }, { 0x3E, 0x10 }, { 0x3F, 0x36 }, { 0x40, 0x37 }, { 0x41, 0x38 },
        { 0x43, 0xE0 }, { 0x44, 0xE3 }, { 0x45, 0xE6 },
        { 0x46, 0x2C }, { 0x4A, 0x52 }, { 0x4B, 0x51 }, { 0x4C, 0x50 }, { 0x4D, 0x4F }, { 0x5F, 0x46 }, { 0x62, 0x49 }, { 0x63, 0x4B },
        { 0x64, 0x4C }, { 0x65, 0x4A }, { 0x66, 0x4D }, { 0x67, 0x4E },
    };
    for (const auto& p : kPairs)
        g_aulaKeyIdToHid[p.keyId].store(p.hid, std::memory_order_relaxed);
}

static uint8_t AulaResolveHid(uint8_t keyId)
{
    if (keyId == 0) return 0;
    return g_aulaKeyIdToHid[keyId].load(std::memory_order_relaxed);
}

static uint8_t AulaPacketChecksum(const uint8_t* bytes, size_t len)
{
    if (!bytes || len == 0) return 0;
    uint32_t s = 0;
    size_t n = (len > 0) ? (len - 1) : 0;
    for (size_t i = 0; i < n; ++i)
        s += bytes[i];
    return (uint8_t)(0xFFu - (s & 0xFFu));
}

static bool AulaFillModePacket(bool enable, uint8_t outPacket[kAulaPayloadSize])
{
    if (!outPacket) return false;
    memset(outPacket, 0, kAulaPayloadSize);
    outPacket[0] = 0x09;
    outPacket[1] = 0x98;
    outPacket[2] = enable ? 0x00 : 0x02;
    outPacket[4] = 0x01;
    outPacket[kAulaPayloadSize - 1] = AulaPacketChecksum(outPacket, kAulaPayloadSize);
    return true;
}

static bool AulaFillPacket(
    uint8_t type,
    uint8_t subType,
    const uint8_t* payload,
    uint16_t payloadLen,
    uint8_t outPacket[kAulaPayloadSize])
{
    if (!outPacket) return false;
    if (payloadLen > 54) return false;
    memset(outPacket, 0, kAulaPayloadSize);
    outPacket[0] = 0x09;
    outPacket[1] = type;
    outPacket[2] = subType;
    outPacket[3] = 0x00;
    outPacket[4] = 0x01;
    outPacket[5] = 0x00;
    outPacket[6] = (uint8_t)(payloadLen & 0xFFu);
    outPacket[7] = (uint8_t)((payloadLen >> 8) & 0xFFu);
    if (payloadLen != 0)
    {
        if (!payload) return false;
        memcpy(outPacket + 8, payload, payloadLen);
    }
    outPacket[kAulaPayloadSize - 1] = AulaPacketChecksum(outPacket, kAulaPayloadSize);
    return true;
}

static bool AulaWriteFileOverlapped(const uint8_t* data, DWORD len, DWORD timeoutMs, DWORD* outErr)
{
    if (!data || len == 0 || !g_aulaHandle)
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
    BOOL ok = WriteFile(g_aulaHandle, data, len, nullptr, &ov);
    if (!ok)
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            DWORD wr = WaitForSingleObject(ev, timeoutMs);
            if (wr == WAIT_OBJECT_0)
            {
                if (!GetOverlappedResult(g_aulaHandle, &ov, &written, FALSE))
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
                CancelIoEx(g_aulaHandle, &ov);
                err = (wr == WAIT_TIMEOUT) ? WAIT_TIMEOUT : GetLastError();
                ok = FALSE;
            }
        }

        if (!ok && outErr) *outErr = err;
    }
    else
    {
        if (!GetOverlappedResult(g_aulaHandle, &ov, &written, TRUE))
        {
            if (outErr) *outErr = GetLastError();
            ok = FALSE;
        }
    }

    CloseHandle(ev);
    return (ok != FALSE && written == len);
}

static bool AulaWritePacket(const uint8_t payload[kAulaPayloadSize])
{
    if (!payload || !g_aulaHandle) return false;
    if (g_aulaOutputReportLen < kAulaPayloadSize) return false;
    if (!g_aulaWriteCapable.load(std::memory_order_acquire)) return false;

    DWORD lastErr = 0;
    std::vector<uint8_t> out((size_t)g_aulaOutputReportLen, 0);
    size_t offset = (g_aulaOutputReportLen > kAulaPayloadSize) ? 1u : 0u; // report-id byte
    if (offset + kAulaPayloadSize > out.size())
        return false;
    memcpy(out.data() + offset, payload, kAulaPayloadSize);

    if (AulaWriteFileOverlapped(out.data(), (DWORD)out.size(), 300, &lastErr))
        return true;

    // Some firmware expects an explicit ReportID=0 prefix even when caps report 64 bytes.
    std::vector<uint8_t> outRid((size_t)kAulaPayloadSize + 1, 0);
    memcpy(outRid.data() + 1, payload, kAulaPayloadSize);
    if (AulaWriteFileOverlapped(outRid.data(), (DWORD)outRid.size(), 300, &lastErr))
        return true;

    if (HidD_SetOutputReport(g_aulaHandle, out.data(), (ULONG)out.size()))
        return true;
    if (HidD_SetOutputReport(g_aulaHandle, outRid.data(), (ULONG)outRid.size()))
        return true;

    if (HidD_SetFeature(g_aulaHandle, out.data(), (ULONG)out.size()))
        return true;
    if (HidD_SetFeature(g_aulaHandle, outRid.data(), (ULONG)outRid.size()))
        return true;

    if (lastErr == 0)
        lastErr = ERROR_GEN_FAILURE;
    SetLastError(lastErr);
    return false;
}

static bool AulaSetAnalogEnabled(bool enable)
{
    uint8_t p[kAulaPayloadSize]{};
    if (!AulaFillModePacket(enable, p))
        return false;

    g_aulaLastEnableTryMs.store(GetTickCount64(), std::memory_order_relaxed);
    bool ok = AulaWritePacket(p);
    DWORD err = ok ? 0 : GetLastError();
    DebugLog_Write(
        L"[backend.vendor] set_analog enable=%d ok=%d err=%lu write_capable=%d out_len=%u",
        enable ? 1 : 0,
        ok ? 1 : 0,
        (unsigned long)err,
        g_aulaWriteCapable.load(std::memory_order_acquire) ? 1 : 0,
        (unsigned)g_aulaOutputReportLen);
    return ok;
}

static bool AulaSendCalibKeepAlive()
{
    if (!g_aulaHandle) return false;
    if (!g_aulaWriteCapable.load(std::memory_order_acquire)) return false;
    uint8_t p[kAulaPayloadSize]{};
    if (!AulaFillPacket(0x94, 0x00, nullptr, 0, p))
        return false;
    bool ok = AulaWritePacket(p);
    if (ok)
        g_aulaLastCalibPollMs.store(GetTickCount64(), std::memory_order_relaxed);
    return ok;
}

static bool AulaSendCalibProbeForMappedKeys()
{
    if (!g_aulaHandle) return false;
    if (!g_aulaWriteCapable.load(std::memory_order_acquire)) return false;

    std::vector<uint8_t> keyIds;
    keyIds.reserve(96);
    for (int keyId = 1; keyId < 256; ++keyId)
    {
        if (g_aulaKeyIdToHid[(size_t)keyId].load(std::memory_order_relaxed) != 0)
            keyIds.push_back((uint8_t)keyId);
    }
    if (keyIds.empty())
        return false;

    int reqCount = 0;
    bool allOk = true;
    constexpr size_t kKeysPerReq = 9; // request_len=18, response_len=54 (max payload)
    for (size_t i = 0; i < keyIds.size(); i += kKeysPerReq)
    {
        size_t n = std::min(kKeysPerReq, keyIds.size() - i);
        std::array<uint8_t, 18> payload{};
        for (size_t j = 0; j < n; ++j)
        {
            payload[j * 2 + 0] = keyIds[i + j];
            payload[j * 2 + 1] = 0x00;
        }

        uint8_t p[kAulaPayloadSize]{};
        uint16_t payloadLen = (uint16_t)(n * 2);
        if (!AulaFillPacket(0x94, 0x05, payload.data(), payloadLen, p))
        {
            allOk = false;
            continue;
        }
        if (!AulaWritePacket(p))
            allOk = false;
        ++reqCount;
    }

    // Trigger/refresh 0x94/0x02 telemetry stream.
    bool keepAliveOk = AulaSendCalibKeepAlive();
    g_aulaLastCalibBootstrapMs.store(GetTickCount64(), std::memory_order_relaxed);
    DebugLog_Write(
        L"[backend.vendor] calib_bootstrap req=%d keys=%d ok=%d keepalive=%d",
        reqCount,
        (int)keyIds.size(),
        allOk ? 1 : 0,
        keepAliveOk ? 1 : 0);
    return allOk;
}

static bool AulaPacketChecksumValid(const uint8_t* p)
{
    if (!p) return false;
    uint32_t sum = 0;
    for (size_t i = 0; i < kAulaPayloadSize; ++i)
        sum += p[i];
    return ((sum & 0xFFu) == 0xFFu);
}

static const uint8_t* AulaTryGetPayload64(const uint8_t* bytes, size_t len)
{
    if (!bytes || len < kAulaPayloadSize)
        return nullptr;
    for (size_t off = 0; off <= 1; ++off)
    {
        if (off + kAulaPayloadSize > len)
            continue;
        const uint8_t* p = bytes + off;
        if (AulaPacketChecksumValid(p))
            return p;
    }
    return nullptr;
}

static bool AulaTryDecodeAnalogEvent(const uint8_t* bytes, size_t len, uint8_t* outKeyId, uint8_t* outFullFlag, uint16_t* outRaw)
{
    if (!outKeyId || !outFullFlag || !outRaw)
        return false;
    const uint8_t* p = AulaTryGetPayload64(bytes, len);
    if (!p)
        return false;
    if (!(p[0] == 0x09 && p[1] == 0x98 && p[2] == 0x01 && p[3] == 0x00 &&
        p[4] == 0x01 && p[5] == 0x00 && p[6] == 0x06 && p[7] == 0x00))
        return false;

    *outKeyId = p[8];
    *outFullFlag = p[9];
    // NOTE: on Aula stream raw lives at bytes[10..11] (little endian).
    *outRaw = (uint16_t)p[10] | ((uint16_t)p[11] << 8);
    return true;
}

static bool AulaTryDecodeReleaseEvent(const uint8_t* bytes, size_t len, uint8_t* outKeyId)
{
    if (!outKeyId)
        return false;
    const uint8_t* p = AulaTryGetPayload64(bytes, len);
    if (!p)
        return false;
    if (!(p[0] == 0x09 && p[1] == 0x98 && p[2] == 0x01 && p[3] == 0x00 &&
        p[4] == 0x01 && p[5] == 0x00 && p[6] == 0x02 && p[7] == 0x00))
        return false;

    *outKeyId = p[8];
    return true;
}

static bool AulaIsIdleMarkerRaw(uint8_t fullFlag, uint16_t raw)
{
    if (fullFlag != 0)
        return false;

    // Aula stream often emits an "idle marker" around 0xA0xx..0xAAxx on key release.
    // Some firmware packets may expose the same range with dropped bit15 (0x20xx..0x2Axx).
    // Treating it as analog depth causes false 100% spikes after key-up.
    const uint8_t hi = (uint8_t)((raw >> 8) & 0xFFu);
    const uint8_t lo = (uint8_t)(raw & 0xFFu);
    const bool lowBand = (hi >= 0x20 && hi <= 0x2A);
    const bool highBand = (hi >= 0xA0 && hi <= 0xAA);
    return (lowBand || highBand) && lo <= 0x7F;
}

static uint16_t AulaNormalizeWireRaw(uint16_t raw)
{
    // On 0x98 analog reports, firmware occasionally drops bit15
    // (example: 0x0859 appears instead of 0x8859).
    // Restore the expected 0x8xxx..0xAxxx raw domain.
    if ((raw & 0x8000u) == 0u)
        raw = (uint16_t)(raw | 0x8000u);
    return raw;
}

static bool AulaTryApplyMapPacket(const uint8_t* bytes, size_t len)
{
    const uint8_t* p = AulaTryGetPayload64(bytes, len);
    if (!p)
        return false;
    if (!(p[0] == 0x09 && p[1] == 0x83 && p[2] == 0x00 && p[3] == 0x00 &&
        p[4] == 0x01 && p[5] == 0x00))
        return false;

    uint16_t ln = (uint16_t)p[6] | ((uint16_t)p[7] << 8);
    if (ln == 0 || ln > 54 || (ln % 6) != 0)
        return true;

    int updates = 0;
    for (uint16_t i = 0; i + 5 < ln; i = (uint16_t)(i + 6))
    {
        uint16_t keyId = (uint16_t)p[8 + i] | ((uint16_t)p[8 + i + 1] << 8);
        uint16_t hid = (uint16_t)p[8 + i + 4] | ((uint16_t)p[8 + i + 5] << 8);
        if (keyId == 0 || keyId >= 256 || hid == 0 || hid >= 256)
            continue;

        uint8_t old = g_aulaKeyIdToHid[keyId].exchange((uint8_t)hid, std::memory_order_relaxed);
        if (old != (uint8_t)hid)
            ++updates;
    }
    if (updates > 0)
        DebugLog_Write(L"[backend.vendor] map_update packet_len=%u updates=%d", (unsigned)ln, updates);
    return true;
}

static bool AulaTryDecodeCalibBatch(
    const uint8_t* bytes,
    size_t len,
    uint8_t* outSubtype,
    const uint8_t** outPayload,
    uint16_t* outPayloadLen)
{
    if (!outSubtype || !outPayload || !outPayloadLen)
        return false;

    *outSubtype = 0;
    *outPayload = nullptr;
    *outPayloadLen = 0;

    const uint8_t* p = AulaTryGetPayload64(bytes, len);
    if (!p)
        return false;
    if (!(p[0] == 0x09 && p[1] == 0x94 && p[3] == 0x00 &&
        p[4] == 0x01 && p[5] == 0x00))
        return false;

    uint16_t ln = (uint16_t)p[6] | ((uint16_t)p[7] << 8);
    if (ln > 54 || (ln % 6) != 0)
        return false;
    if ((size_t)8 + (size_t)ln > kAulaPayloadSize)
        return false;

    *outSubtype = p[2];
    *outPayload = p + 8;
    *outPayloadLen = ln;
    return true;
}

static float AulaNormalizeRawTo01(uint16_t hid, uint8_t fullFlag, uint16_t raw)
{
    if (hid == 0 || hid >= 256)
        return 0.0f;

    AulaKeyCalib& c = g_aulaCalib[hid];
    if (!c.seen)
    {
        c.seen = true;
        if (raw > c.releaseRaw)
            c.releaseRaw = raw;
        if (raw >= 20000 && raw < c.pressRaw)
            c.pressRaw = raw;
    }
    else
    {
        if (raw > c.releaseRaw)
            c.releaseRaw = raw;
        // Ignore low service-marker range from press calibration.
        if (raw >= 20000 && raw < c.pressRaw)
            c.pressRaw = raw;
    }

    if (fullFlag != 0 && raw >= 20000 && raw < c.pressRaw)
        c.pressRaw = raw;

    constexpr uint16_t kMinGap = 600;
    if (c.releaseRaw <= c.pressRaw + kMinGap)
        c.releaseRaw = (uint16_t)std::min<int>(65535, (int)c.pressRaw + kMinGap);

    const float num = (float)((int)c.releaseRaw - (int)raw);
    const float den = (float)((int)c.releaseRaw - (int)c.pressRaw);
    float v = (den > 1.0f) ? (num / den) : 0.0f;
    return std::clamp(v, 0.0f, 1.0f);
}

static void AulaProcessInputReport(const uint8_t* bytes, size_t len)
{
    if (!bytes || len < 8)
        return;

    if (AulaTryApplyMapPacket(bytes, len))
        return;

    uint8_t calibSubtype = 0;
    const uint8_t* calibPayload = nullptr;
    uint16_t calibLen = 0;
    if (AulaAllowPassive94Reports() &&
        AulaTryDecodeCalibBatch(bytes, len, &calibSubtype, &calibPayload, &calibLen))
    {
        ULONGLONG now = GetTickCount64();
        g_aulaLastPacketMs.store(now, std::memory_order_relaxed);
        g_aulaLastCalibPacketMs.store(now, std::memory_order_relaxed);

        // 0x94/0x02 is observed in calibration mode as batched analog events.
        // Other 0x94 subtypes include service/config payloads and are ignored here.
        if (calibLen == 0)
            return;
        if (calibSubtype != 0x02)
        {
            ULONGLONG lastLog = g_aulaLastUnknownLogMs.load(std::memory_order_relaxed);
            if (now - lastLog >= 2000)
            {
                g_aulaLastUnknownLogMs.store(now, std::memory_order_relaxed);
                DebugLog_Write(
                    L"[backend.vendor] calib_report sub=0x%02X len=%u ignored",
                    (unsigned)calibSubtype,
                    (unsigned)calibLen);
            }
            return;
        }

        for (uint16_t i = 0; i + 5 < calibLen; i = (uint16_t)(i + 6))
        {
            uint8_t keyId = calibPayload[i + 0];
            if (keyId == 0)
                continue;

            uint16_t wireRaw = ((uint16_t)calibPayload[i + 1] << 8) | (uint16_t)calibPayload[i + 2];
            uint16_t raw = AulaNormalizeWireRaw(wireRaw);
            uint8_t hid = AulaResolveHid(keyId);
            if (hid == 0)
                continue;

            float v01 = AulaNormalizeRawTo01(hid, 0, raw);
            int m = std::clamp((int)std::lround(v01 * 1000.0f), 0, 1000);
            if (m <= 1)
                m = 0;

            uint16_t prev = g_aulaAnalogMilli[hid].exchange((uint16_t)m, std::memory_order_relaxed);
            uint8_t prevFull = g_aulaLastFull[hid].load(std::memory_order_relaxed);
            g_aulaLastFull[hid].store(0, std::memory_order_relaxed);
            g_aulaLastKeyUpdateMs[hid].store(now, std::memory_order_relaxed);
            if (std::abs((int)m - (int)prev) >= 20 || (prev == 0) != (m == 0) || prevFull != 0)
            {
                uint8_t phys = g_physicalDown[hid].load(std::memory_order_relaxed);
                DebugLog_Write(
                    L"[backend.vendor.ev] analog94 sub=0x%02X key_id=0x%02X hid=%u raw=%u wire=%u out=%u prev=%u phys=%u aux=%02X %02X %02X",
                    (unsigned)calibSubtype,
                    (unsigned)keyId,
                    (unsigned)hid,
                    (unsigned)raw,
                    (unsigned)wireRaw,
                    (unsigned)m,
                    (unsigned)prev,
                    (unsigned)phys,
                    (unsigned)calibPayload[i + 3],
                    (unsigned)calibPayload[i + 4],
                    (unsigned)calibPayload[i + 5]);
            }
        }
        return;
    }

    ULONGLONG nowForPrefer = GetTickCount64();
    ULONGLONG lastCalibPacket = g_aulaLastCalibPacketMs.load(std::memory_order_relaxed);
    if (AulaAllowPassive94Reports() &&
        lastCalibPacket != 0 &&
        nowForPrefer - lastCalibPacket <= kAulaPreferCalibOver98WindowMs)
    {
        const uint8_t* p = AulaTryGetPayload64(bytes, len);
        if (p && p[0] == 0x09 && p[1] == 0x98)
        {
            g_aulaLastPacketMs.store(nowForPrefer, std::memory_order_relaxed);
            return;
        }
    }

    uint8_t releaseKeyId = 0;
    if (AulaTryDecodeReleaseEvent(bytes, len, &releaseKeyId))
    {
        uint8_t hid = AulaResolveHid(releaseKeyId);
        if (hid != 0)
        {
            uint16_t prev = g_aulaAnalogMilli[hid].exchange(0, std::memory_order_relaxed);
            g_aulaLastFull[hid].store(0, std::memory_order_relaxed);
            g_aulaLastKeyUpdateMs[hid].store(GetTickCount64(), std::memory_order_relaxed);
            if (prev != 0)
            {
                uint8_t phys = g_physicalDown[hid].load(std::memory_order_relaxed);
                DebugLog_Write(
                    L"[backend.vendor.ev] release key_id=0x%02X hid=%u prev=%u phys=%u",
                    (unsigned)releaseKeyId,
                    (unsigned)hid,
                    (unsigned)prev,
                    (unsigned)phys);
            }
            g_aulaLastPacketMs.store(GetTickCount64(), std::memory_order_relaxed);
        }
        return;
    }

    uint8_t keyId = 0;
    uint8_t full = 0;
    uint16_t raw = 0;
    if (!AulaTryDecodeAnalogEvent(bytes, len, &keyId, &full, &raw))
    {
        ULONGLONG now = GetTickCount64();
        const uint8_t* p = AulaTryGetPayload64(bytes, len);
        bool serviceReport = false;
        if (p && p[0] == 0x09)
        {
            const uint8_t t = p[1];
            serviceReport = (t == 0x90 || t == 0x94 || t == 0x96 || t == 0x99);
            if (t == 0x98 || serviceReport)
                g_aulaLastPacketMs.store(now, std::memory_order_relaxed);
        }
        ULONGLONG lastLog = g_aulaLastUnknownLogMs.load(std::memory_order_relaxed);
        if (now - lastLog >= 2000 && len >= 8)
        {
            g_aulaLastUnknownLogMs.store(now, std::memory_order_relaxed);
            if (serviceReport && p)
            {
                DebugLog_Write(
                    L"[backend.vendor] service_report type=0x%02X len=%u ignored",
                    (unsigned)p[1],
                    (unsigned)len);
            }
            else
            {
                DebugLog_Write(
                    L"[backend.vendor] unknown_report len=%u head=%02X %02X %02X %02X %02X %02X %02X %02X",
                    (unsigned)len,
                    (unsigned)bytes[0], (unsigned)bytes[1], (unsigned)bytes[2], (unsigned)bytes[3],
                    (unsigned)bytes[4], (unsigned)bytes[5], (unsigned)bytes[6], (unsigned)bytes[7]);
            }
        }
        return;
    }

    ULONGLONG now = GetTickCount64();
    g_aulaLastPacketMs.store(now, std::memory_order_relaxed);
    const uint16_t wireRaw = raw;
    raw = AulaNormalizeWireRaw(raw);

    uint8_t hid = AulaResolveHid(keyId);
    if (hid == 0)
    {
        ULONGLONG lastLog = g_aulaLastUnknownLogMs.load(std::memory_order_relaxed);
        if (now - lastLog >= 800)
        {
            g_aulaLastUnknownLogMs.store(now, std::memory_order_relaxed);
            DebugLog_Write(
                L"[backend.vendor.ev] unmapped key_id=0x%02X full=%u raw=%u wire=%u",
                (unsigned)keyId,
                (unsigned)full,
                (unsigned)raw,
                (unsigned)wireRaw);
        }
        return;
    }

    if (AulaIsIdleMarkerRaw(full, raw))
    {
        g_aulaLastKeyUpdateMs[hid].store(now, std::memory_order_relaxed);
        uint16_t prev = g_aulaAnalogMilli[hid].exchange(0, std::memory_order_relaxed);
        g_aulaLastFull[hid].store(0, std::memory_order_relaxed);
        if (prev != 0)
        {
            uint8_t phys = g_physicalDown[hid].load(std::memory_order_relaxed);
            DebugLog_Write(
                L"[backend.vendor.ev] idle_release key_id=0x%02X hid=%u raw=%u wire=%u prev=%u phys=%u",
                (unsigned)keyId,
                (unsigned)hid,
                (unsigned)raw,
                (unsigned)wireRaw,
                (unsigned)prev,
                (unsigned)phys);
        }
        return;
    }

    float v01 = AulaNormalizeRawTo01(hid, full, raw);
    int m = std::clamp((int)std::lround(v01 * 1000.0f), 0, 1000);
    uint16_t prev = g_aulaAnalogMilli[hid].exchange((uint16_t)m, std::memory_order_relaxed);
    uint8_t prevFull = g_aulaLastFull[hid].load(std::memory_order_relaxed);
    g_aulaLastFull[hid].store(full, std::memory_order_relaxed);
    g_aulaLastKeyUpdateMs[hid].store(now, std::memory_order_relaxed);
    if (std::abs((int)m - (int)prev) >= 20 || (prev == 0) != (m == 0) || prevFull != full)
    {
        uint8_t phys = g_physicalDown[hid].load(std::memory_order_relaxed);
        DebugLog_Write(
            L"[backend.vendor.ev] analog key_id=0x%02X hid=%u full=%u raw=%u wire=%u out=%u prev=%u phys=%u",
            (unsigned)keyId,
            (unsigned)hid,
            (unsigned)full,
            (unsigned)raw,
            (unsigned)wireRaw,
            (unsigned)m,
            (unsigned)prev,
            (unsigned)phys);
    }
}

static bool AulaReadCaps(HANDLE h, uint16_t* inLen, uint16_t* outLen, uint16_t* usagePage, uint16_t* usage)
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

static HANDLE AulaTryOpenDevice(
    uint16_t* inLen,
    uint16_t* outLen,
    std::wstring* outPath,
    bool* outWriteCapable,
    uint16_t* outVid,
    uint16_t* outPid)
{
    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE)
        return nullptr;

    HANDLE foundWritable = nullptr;
    int foundWritableScore = -1;
    std::wstring writablePath;
    uint16_t writableInLen = 0;
    uint16_t writableOutLen = 0;
    uint16_t writableVid = 0;
    uint16_t writablePid = 0;

    HANDLE fallbackReadOnly = nullptr;
    int fallbackReadScore = -1;
    std::wstring fallbackPath;
    uint16_t fallbackInLen = 0;
    uint16_t fallbackOutLen = 0;
    uint16_t fallbackVid = 0;
    uint16_t fallbackPid = 0;
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

        if (attr.VendorID != kAulaVendorId)
        {
            CloseHandle(h);
            continue;
        }

        uint16_t inR = 0, outR = 0, usagePage = 0, usage = 0;
        if (!AulaReadCaps(h, &inR, &outR, &usagePage, &usage))
        {
            CloseHandle(h);
            continue;
        }
        int score = AulaUsageSupportScore(usagePage, usage);
        if (inR < kAulaPayloadSize || outR < kAulaPayloadSize || score <= 0)
        {
            CloseHandle(h);
            continue;
        }

        DebugLog_Write(
            L"[backend.vendor] candidate vid=0x%04X pid=0x%04X path=%s write_capable=%d score=%d in_len=%u out_len=%u usage_page=0x%04X usage=0x%04X",
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
            if (!foundWritable || score > foundWritableScore)
            {
                if (foundWritable)
                    CloseHandle(foundWritable);
                foundWritable = h;
                foundWritableScore = score;
                writablePath = detail->DevicePath;
                writableInLen = inR;
                writableOutLen = outR;
                writableVid = attr.VendorID;
                writablePid = attr.ProductID;
            }
            else
            {
                CloseHandle(h);
            }
            continue;
        }

        if (!fallbackReadOnly || score > fallbackReadScore)
        {
            if (fallbackReadOnly)
                CloseHandle(fallbackReadOnly);
            fallbackReadOnly = h;
            fallbackReadScore = score;
            fallbackPath = detail->DevicePath;
            fallbackInLen = inR;
            fallbackOutLen = outR;
            fallbackVid = attr.VendorID;
            fallbackPid = attr.ProductID;
        }
        else
        {
            CloseHandle(h);
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);

    if (foundWritable)
    {
        if (fallbackReadOnly)
        {
            CloseHandle(fallbackReadOnly);
            fallbackReadOnly = nullptr;
        }
        if (inLen) *inLen = writableInLen;
        if (outLen) *outLen = writableOutLen;
        if (outPath) *outPath = writablePath;
        if (outWriteCapable) *outWriteCapable = true;
        if (outVid) *outVid = writableVid;
        if (outPid) *outPid = writablePid;
        return foundWritable;
    }

    if (fallbackReadOnly)
    {
        if (inLen) *inLen = fallbackInLen;
        if (outLen) *outLen = fallbackOutLen;
        if (outPath) *outPath = fallbackPath;
        if (outWriteCapable) *outWriteCapable = false;
        if (outVid) *outVid = fallbackVid;
        if (outPid) *outPid = fallbackPid;
    }
    return fallbackReadOnly;
}

static DWORD WINAPI AulaReadThreadProc(LPVOID)
{
    HANDLE h = g_aulaHandle;
    HANDLE stopEv = g_aulaStopEvent;
    if (!h || !stopEv)
        return 0;

    HANDLE readEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!readEvent)
    {
        g_aulaConnectedVid.store(0, std::memory_order_relaxed);
        g_aulaConnectedPid.store(0, std::memory_order_relaxed);
        g_aulaConnected.store(false, std::memory_order_release);
        return 0;
    }

    std::vector<uint8_t> inBuf((size_t)std::max<uint16_t>(g_aulaInputReportLen, (uint16_t)kAulaPayloadSize), 0);
    OVERLAPPED ov{};
    ov.hEvent = readEvent;

    DebugLog_Write(
        L"[backend.vendor] reader start in_len=%u out_len=%u",
        (unsigned)g_aulaInputReportLen,
        (unsigned)g_aulaOutputReportLen);

    while (WaitForSingleObject(stopEv, 0) != WAIT_OBJECT_0)
    {
        ResetEvent(readEvent);
        DWORD readBytes = 0;
        BOOL ok = ReadFile(h, inBuf.data(), (DWORD)inBuf.size(), nullptr, &ov);
        if (!ok)
        {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING)
            {
                if (err == ERROR_DEVICE_NOT_CONNECTED || err == ERROR_INVALID_HANDLE || err == ERROR_OPERATION_ABORTED)
                    break;
                Sleep(2);
                continue;
            }

            HANDLE waits[2] = { stopEv, readEvent };
            DWORD wr = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
            if (wr == WAIT_OBJECT_0)
            {
                CancelIoEx(h, &ov);
                break;
            }
            if (wr != WAIT_OBJECT_0 + 1)
                continue;
        }

        if (!GetOverlappedResult(h, &ov, &readBytes, FALSE))
        {
            DWORD ge = GetLastError();
            if (ge == ERROR_DEVICE_NOT_CONNECTED || ge == ERROR_INVALID_HANDLE || ge == ERROR_OPERATION_ABORTED)
                break;
            continue;
        }
        if (readBytes == 0)
            continue;

        AulaProcessInputReport(inBuf.data(), readBytes);
    }

    CloseHandle(readEvent);
    g_aulaConnectedVid.store(0, std::memory_order_relaxed);
    g_aulaConnectedPid.store(0, std::memory_order_relaxed);
    g_aulaConnected.store(false, std::memory_order_release);
    DebugLog_Write(L"[backend.vendor] reader stop");
    return 0;
}

static void AulaStop()
{
    if (g_aulaHandle)
        AulaSetAnalogEnabled(false);

    if (g_aulaStopEvent)
        SetEvent(g_aulaStopEvent);
    if (g_aulaHandle)
        CancelIoEx(g_aulaHandle, nullptr);

    if (g_aulaThread)
    {
        WaitForSingleObject(g_aulaThread, 1500);
        CloseHandle(g_aulaThread);
        g_aulaThread = nullptr;
    }

    if (g_aulaHandle)
    {
        CloseHandle(g_aulaHandle);
        g_aulaHandle = nullptr;
    }
    if (g_aulaStopEvent)
    {
        CloseHandle(g_aulaStopEvent);
        g_aulaStopEvent = nullptr;
    }

    g_aulaInputReportLen = (uint16_t)kAulaPayloadSize;
    g_aulaOutputReportLen = (uint16_t)kAulaPayloadSize;
    g_aulaWriteCapable.store(false, std::memory_order_release);
    g_aulaLastEnableTryMs.store(0, std::memory_order_relaxed);
    g_aulaLastCalibPacketMs.store(0, std::memory_order_relaxed);
    g_aulaLastCalibPollMs.store(0, std::memory_order_relaxed);
    g_aulaLastCalibBootstrapMs.store(0, std::memory_order_relaxed);
    g_aulaLastCommMode.store((UINT)-1, std::memory_order_relaxed);
    g_aulaConnectedVid.store(0, std::memory_order_relaxed);
    g_aulaConnectedPid.store(0, std::memory_order_relaxed);
    g_aulaConnected.store(false, std::memory_order_release);
}

static bool AulaStart()
{
    if (g_aulaConnected.load(std::memory_order_acquire))
        return true;
    if (g_aulaThread || g_aulaHandle)
        AulaStop();

    uint16_t inLen = 0, outLen = 0;
    uint16_t vid = 0, pid = 0;
    std::wstring path;
    bool writeCapable = false;
    HANDLE h = AulaTryOpenDevice(&inLen, &outLen, &path, &writeCapable, &vid, &pid);
    if (!h)
        return false;

    g_aulaHandle = h;
    g_aulaInputReportLen = std::max<uint16_t>(inLen, (uint16_t)kAulaPayloadSize);
    g_aulaOutputReportLen = std::max<uint16_t>(outLen, (uint16_t)kAulaPayloadSize);
    g_aulaWriteCapable.store(writeCapable, std::memory_order_release);
    g_aulaStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_aulaStopEvent)
    {
        CloseHandle(g_aulaHandle);
        g_aulaHandle = nullptr;
        return false;
    }

    Aula_ResetKeyState();
    g_aulaLastReconnectTryMs = GetTickCount64();
    DWORD tid = 0;
    g_aulaThread = CreateThread(nullptr, 0, AulaReadThreadProc, nullptr, 0, &tid);
    if (!g_aulaThread)
    {
        CloseHandle(g_aulaStopEvent);
        g_aulaStopEvent = nullptr;
        CloseHandle(g_aulaHandle);
        g_aulaHandle = nullptr;
        return false;
    }

    g_aulaConnected.store(true, std::memory_order_release);
    g_aulaConnectedVid.store(vid, std::memory_order_relaxed);
    g_aulaConnectedPid.store(pid, std::memory_order_relaxed);
    bool analogOn = AulaSetAnalogEnabled(true);
    UINT commMode = Settings_GetAulaCommMode();
    g_aulaLastCommMode.store(commMode, std::memory_order_relaxed);
    if (writeCapable && commMode == (UINT)SettingsAulaCommMode_94ActiveExperimental)
        AulaSendCalibProbeForMappedKeys();
    else if (writeCapable)
        DebugLog_Write(
            L"[backend.vendor] active_0x94_protocol disabled mode=%s(%u)",
            AulaCommModeName(commMode),
            (unsigned)commMode);
    DebugLog_Write(
        L"[backend.vendor] connected vid=0x%04X pid=0x%04X in_len=%u out_len=%u write_capable=%d analog_on=%d mode=%s(%u) path=%s",
        (unsigned)vid,
        (unsigned)pid,
        (unsigned)g_aulaInputReportLen,
        (unsigned)g_aulaOutputReportLen,
        writeCapable ? 1 : 0,
        analogOn ? 1 : 0,
        AulaCommModeName(commMode),
        (unsigned)commMode,
        path.c_str());
    return true;
}

static void AulaTickHotplug(ULONGLONG nowMs)
{
    if (g_aulaConnected.load(std::memory_order_acquire))
    {
        UINT modeNow = Settings_GetAulaCommMode();
        UINT prevMode = g_aulaLastCommMode.exchange(modeNow, std::memory_order_relaxed);
        if (prevMode != modeNow && prevMode != (UINT)-1)
        {
            DebugLog_Write(
                L"[backend.vendor] comm_mode_changed prev=%s(%u) now=%s(%u)",
                AulaCommModeName(prevMode),
                (unsigned)prevMode,
                AulaCommModeName(modeNow),
                (unsigned)modeNow);
        }

        if (g_aulaWriteCapable.load(std::memory_order_acquire))
        {
            if (prevMode != modeNow)
            {
                AulaSetAnalogEnabled(true);
                if (modeNow == (UINT)SettingsAulaCommMode_94ActiveExperimental)
                    AulaSendCalibProbeForMappedKeys();
            }

            ULONGLONG lastEnable = g_aulaLastEnableTryMs.load(std::memory_order_relaxed);
            ULONGLONG lastPacket = g_aulaLastPacketMs.load(std::memory_order_relaxed);
            if (nowMs - lastEnable >= kAulaEnableRetryMs &&
                (lastPacket == 0 || nowMs - lastPacket >= kAulaNoPacketRetryMs))
            {
                AulaSetAnalogEnabled(true);
            }

            if (modeNow == (UINT)SettingsAulaCommMode_94ActiveExperimental)
            {
                ULONGLONG lastPoll = g_aulaLastCalibPollMs.load(std::memory_order_relaxed);
                if (nowMs - lastPoll >= kAulaCalibPollIntervalMs)
                    AulaSendCalibKeepAlive();

                ULONGLONG lastCalib = g_aulaLastCalibPacketMs.load(std::memory_order_relaxed);
                ULONGLONG lastBootstrap = g_aulaLastCalibBootstrapMs.load(std::memory_order_relaxed);
                if ((lastCalib == 0 || nowMs - lastCalib >= kAulaCalibNoPacketMs) &&
                    nowMs - lastBootstrap >= kAulaCalibBootstrapRetryMs)
                {
                    AulaSendCalibProbeForMappedKeys();
                }
            }
        }
        return;
    }
    if (nowMs - g_aulaLastReconnectTryMs < kAulaReconnectIntervalMs)
        return;
    g_aulaLastReconnectTryMs = nowMs;
    if (AulaStart())
    {
        DebugLog_Write(L"[backend.vendor] hotplug reconnect ok");
    }
}

static void AulaDecayStaleKeys(ULONGLONG nowMs)
{
    for (uint16_t hid = 1; hid < 256; ++hid)
    {
        uint16_t cur = g_aulaAnalogMilli[hid].load(std::memory_order_relaxed);
        if (cur == 0)
            continue;

        ULONGLONG last = g_aulaLastKeyUpdateMs[hid].load(std::memory_order_relaxed);
        uint8_t phys = g_physicalDown[hid].load(std::memory_order_relaxed);
        ULONGLONG staleLimit = kAulaKeyStaleMs;
        if (last == 0 || nowMs - last <= staleLimit)
            continue;

        g_aulaAnalogMilli[hid].store(0, std::memory_order_relaxed);
        g_aulaLastFull[hid].store(0, std::memory_order_relaxed);
        DebugLog_Write(
            L"[backend.vendor.ev] stale_release hid=%u prev=%u age_ms=%llu phys=%u",
            (unsigned)hid,
            (unsigned)cur,
            (unsigned long long)(nowMs - last),
            (unsigned)phys);
    }
}


