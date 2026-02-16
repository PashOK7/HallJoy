#include "key_settings.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <thread>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
#endif

// Data storage
// Fast path: HID < 256
static std::array<KeyDeadzone, 256> g_fastData{};
static std::shared_mutex g_fastMutex;

struct FastSnapshot
{
    std::atomic<uint32_t> seq{ 0 };
    std::atomic<uint8_t> useUnique{ 0 };
    std::atomic<uint8_t> invert{ 0 };
    std::atomic<uint8_t> curveMode{ 1 };
    std::atomic<int16_t> lowM{ 80 };
    std::atomic<int16_t> highM{ 900 };
    std::atomic<int16_t> antiDeadzoneM{ 0 };
    std::atomic<int16_t> outputCapM{ 1000 };
    std::atomic<int16_t> cp1xM{ 380 };
    std::atomic<int16_t> cp1yM{ 330 };
    std::atomic<int16_t> cp2xM{ 680 };
    std::atomic<int16_t> cp2yM{ 660 };
    std::atomic<int16_t> cp1wM{ 1000 };
    std::atomic<int16_t> cp2wM{ 1000 };
};

static std::array<FastSnapshot, 256> g_fastSnapshot{};

// Slow path: HID >= 256
static std::unordered_map<uint16_t, KeyDeadzone> g_mapData;
static std::shared_mutex g_mapMutex;

static inline void CpuRelax()
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    _mm_pause();
#else
    std::this_thread::yield();
#endif
}

static int16_t ToMilli(float v)
{
    int m = (int)lroundf(std::clamp(v, 0.0f, 1.0f) * 1000.0f);
    return (int16_t)std::clamp(m, 0, 1000);
}

static float FromMilli(int16_t m)
{
    return (float)std::clamp((int)m, 0, 1000) / 1000.0f;
}

static void FastSnapshotStore(uint16_t hid, const KeyDeadzone& s)
{
    FastSnapshot& snap = g_fastSnapshot[hid];
    snap.seq.fetch_add(1u, std::memory_order_acq_rel); // odd => writer in progress

    snap.useUnique.store(s.useUnique ? 1u : 0u, std::memory_order_relaxed);
    snap.invert.store(s.invert ? 1u : 0u, std::memory_order_relaxed);
    snap.curveMode.store((s.curveMode == 0) ? 0u : 1u, std::memory_order_relaxed);

    snap.lowM.store(ToMilli(s.low), std::memory_order_relaxed);
    snap.highM.store(ToMilli(s.high), std::memory_order_relaxed);
    snap.antiDeadzoneM.store(ToMilli(s.antiDeadzone), std::memory_order_relaxed);
    snap.outputCapM.store(ToMilli(s.outputCap), std::memory_order_relaxed);
    snap.cp1xM.store(ToMilli(s.cp1_x), std::memory_order_relaxed);
    snap.cp1yM.store(ToMilli(s.cp1_y), std::memory_order_relaxed);
    snap.cp2xM.store(ToMilli(s.cp2_x), std::memory_order_relaxed);
    snap.cp2yM.store(ToMilli(s.cp2_y), std::memory_order_relaxed);
    snap.cp1wM.store(ToMilli(s.cp1_w), std::memory_order_relaxed);
    snap.cp2wM.store(ToMilli(s.cp2_w), std::memory_order_relaxed);

    snap.seq.fetch_add(1u, std::memory_order_release); // even => stable
}

static KeyDeadzone FastSnapshotLoad(uint16_t hid)
{
    KeyDeadzone out{};
    FastSnapshot& snap = g_fastSnapshot[hid];

    for (;;)
    {
        uint32_t s1 = snap.seq.load(std::memory_order_acquire);
        if (s1 & 1u)
        {
            CpuRelax();
            continue;
        }

        out.useUnique = snap.useUnique.load(std::memory_order_relaxed) != 0;
        out.invert = snap.invert.load(std::memory_order_relaxed) != 0;
        out.curveMode = (uint8_t)(snap.curveMode.load(std::memory_order_relaxed) == 0 ? 0 : 1);

        out.low = FromMilli(snap.lowM.load(std::memory_order_relaxed));
        out.high = FromMilli(snap.highM.load(std::memory_order_relaxed));
        out.antiDeadzone = FromMilli(snap.antiDeadzoneM.load(std::memory_order_relaxed));
        out.outputCap = FromMilli(snap.outputCapM.load(std::memory_order_relaxed));
        out.cp1_x = FromMilli(snap.cp1xM.load(std::memory_order_relaxed));
        out.cp1_y = FromMilli(snap.cp1yM.load(std::memory_order_relaxed));
        out.cp2_x = FromMilli(snap.cp2xM.load(std::memory_order_relaxed));
        out.cp2_y = FromMilli(snap.cp2yM.load(std::memory_order_relaxed));
        out.cp1_w = FromMilli(snap.cp1wM.load(std::memory_order_relaxed));
        out.cp2_w = FromMilli(snap.cp2wM.load(std::memory_order_relaxed));

        uint32_t s2 = snap.seq.load(std::memory_order_acquire);
        if (s1 == s2)
            return out;

        CpuRelax();
    }
}

static KeyDeadzone Normalize(KeyDeadzone s)
{
    // Normalize curveMode: only 0 or 1 for now
    s.curveMode = (s.curveMode == 0) ? 0 : 1;

    // Start / End constraints
    s.low = std::clamp(s.low, 0.0f, 0.99f);

    // Ensure High > Low (minimal gap)
    if (s.high < s.low + 0.01f) s.high = s.low + 0.01f;
    s.high = std::clamp(s.high, 0.01f, 1.0f);

    // Endpoint Y
    s.antiDeadzone = std::clamp(s.antiDeadzone, 0.0f, 0.99f);
    s.outputCap = std::clamp(s.outputCap, 0.01f, 1.0f);

    // Ensure Output Cap >= Anti-Deadzone
    if (s.outputCap < s.antiDeadzone + 0.01f)
        s.outputCap = s.antiDeadzone + 0.01f;

    // Control Points constraints (0..1 bounding box)
    s.cp1_x = std::clamp(s.cp1_x, 0.0f, 1.0f);
    s.cp1_y = std::clamp(s.cp1_y, 0.0f, 1.0f);

    s.cp2_x = std::clamp(s.cp2_x, 0.0f, 1.0f);
    s.cp2_y = std::clamp(s.cp2_y, 0.0f, 1.0f);

    // NEW: clamp CP weights (0..1)
    s.cp1_w = std::clamp(s.cp1_w, 0.0f, 1.0f);
    s.cp2_w = std::clamp(s.cp2_w, 0.0f, 1.0f);

    // NEW: enforce monotonic-ish order on X to avoid invalid curves coming from INI or other callers.
    // Keep a tiny gap so later inverse-evaluation is stable.
    {
        const float minGap = 0.001f;

        // clamp to [low..high]
        s.cp1_x = std::clamp(s.cp1_x, s.low, s.high);
        s.cp2_x = std::clamp(s.cp2_x, s.low, s.high);

        // ensure cp1_x <= cp2_x
        if (s.cp2_x < s.cp1_x)
            std::swap(s.cp1_x, s.cp2_x);

        // enforce small separation and keep inside end points
        s.cp1_x = std::clamp(s.cp1_x, s.low, s.high - minGap);
        s.cp2_x = std::clamp(s.cp2_x, s.cp1_x + minGap, s.high);
    }

    // We do NOT force cp y into [antiDeadzone..outputCap] (max flexibility).

    return s;
}

void KeySettings_Set(uint16_t hid, const KeyDeadzone& in)
{
    if (!hid) return;
    KeyDeadzone norm = Normalize(in);

    if (hid < 256)
    {
        std::unique_lock lock(g_fastMutex);
        g_fastData[hid] = norm;
        FastSnapshotStore(hid, norm);
        return;
    }

    {
        std::unique_lock lock(g_mapMutex);
        g_mapData[hid] = norm;
    }
}

KeyDeadzone KeySettings_Get(uint16_t hid)
{
    KeyDeadzone def;
    if (!hid) return def;

    if (hid < 256)
    {
        return FastSnapshotLoad(hid);
    }

    {
        std::shared_lock lock(g_mapMutex);
        auto it = g_mapData.find(hid);
        if (it == g_mapData.end()) return def;
        return it->second;
    }
}

bool KeySettings_GetUseUnique(uint16_t hid)
{
    if (!hid) return false;

    if (hid < 256)
    {
        return g_fastSnapshot[hid].useUnique.load(std::memory_order_acquire) != 0;
    }

    // HID >= 256: slow path
    {
        std::shared_lock lock(g_mapMutex);
        auto it = g_mapData.find(hid);
        if (it == g_mapData.end()) return false;
        return it->second.useUnique;
    }
}

void KeySettings_SetUseUnique(uint16_t hid, bool on)
{
    auto s = KeySettings_Get(hid);
    s.useUnique = on;
    KeySettings_Set(hid, s);
}

void KeySettings_SetLow(uint16_t hid, float low)
{
    auto s = KeySettings_Get(hid);
    s.low = low;
    KeySettings_Set(hid, s);
}

void KeySettings_SetHigh(uint16_t hid, float high)
{
    auto s = KeySettings_Get(hid);
    s.high = high;
    KeySettings_Set(hid, s);
}

void KeySettings_SetAntiDeadzone(uint16_t hid, float val)
{
    auto s = KeySettings_Get(hid);
    s.antiDeadzone = val;
    KeySettings_Set(hid, s);
}

void KeySettings_SetOutputCap(uint16_t hid, float val)
{
    auto s = KeySettings_Get(hid);
    s.outputCap = val;
    KeySettings_Set(hid, s);
}

void KeySettings_ClearAll()
{
    {
        std::unique_lock lock(g_fastMutex);
        for (uint16_t hid = 0; hid < 256; ++hid)
        {
            g_fastData[hid] = KeyDeadzone{};
            FastSnapshotStore(hid, g_fastData[hid]);
        }
    }
    {
        std::unique_lock lock(g_mapMutex);
        g_mapData.clear();
    }
}

static bool NearlyEq(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

static bool IsDefaultLike(const KeyDeadzone& a)
{
    KeyDeadzone def; // default struct values

    if (a.useUnique != def.useUnique) return false;
    if (a.invert != def.invert) return false;
    if (a.curveMode != def.curveMode) return false;

    if (!NearlyEq(a.low, def.low)) return false;
    if (!NearlyEq(a.high, def.high)) return false;

    if (!NearlyEq(a.antiDeadzone, def.antiDeadzone)) return false;
    if (!NearlyEq(a.outputCap, def.outputCap)) return false;

    if (!NearlyEq(a.cp1_x, def.cp1_x)) return false;
    if (!NearlyEq(a.cp1_y, def.cp1_y)) return false;
    if (!NearlyEq(a.cp2_x, def.cp2_x)) return false;
    if (!NearlyEq(a.cp2_y, def.cp2_y)) return false;

    if (!NearlyEq(a.cp1_w, def.cp1_w)) return false;
    if (!NearlyEq(a.cp2_w, def.cp2_w)) return false;

    return true;
}

void KeySettings_Enumerate(std::vector<std::pair<uint16_t, KeyDeadzone>>& out)
{
    out.clear();

    // HID < 256
    {
        std::shared_lock lock(g_fastMutex);
        for (uint16_t hid = 1; hid < 256; ++hid)
        {
            const auto& d = g_fastData[hid];

            // IMPORTANT:
            // - if useUnique=true -> always save
            // - else save only if it deviates from defaults (rare, but safe)
            if (d.useUnique || !IsDefaultLike(d))
                out.emplace_back(hid, d);
        }
    }

    // HID >= 256
    {
        std::shared_lock lock(g_mapMutex);
        for (const auto& [hid, d] : g_mapData)
            out.emplace_back(hid, d);
    }
}
