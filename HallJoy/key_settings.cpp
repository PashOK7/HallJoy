#include "key_settings.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <cmath>

// Data storage
// Fast path: HID < 256
static std::array<KeyDeadzone, 256> g_fastData{};
static std::shared_mutex g_fastMutex;

// Fast, lock-free mirror of useUnique for HID < 256 (UI hot path)
static std::array<std::atomic<uint8_t>, 256> g_fastUseUnique{};

// Slow path: HID >= 256
static std::unordered_map<uint16_t, KeyDeadzone> g_mapData;
static std::shared_mutex g_mapMutex;

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

        // keep atomic mirror in sync
        g_fastUseUnique[hid].store(norm.useUnique ? 1u : 0u, std::memory_order_release);
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
        std::shared_lock lock(g_fastMutex);
        return g_fastData[hid];
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
        return g_fastUseUnique[hid].load(std::memory_order_acquire) != 0;
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
            g_fastUseUnique[hid].store(0u, std::memory_order_release);
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