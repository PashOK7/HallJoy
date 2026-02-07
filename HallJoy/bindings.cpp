#include "bindings.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <algorithm>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// ---- Axis packing (unchanged) ----
static uint32_t PackAxis(uint16_t minusHid, uint16_t plusHid)
{
    return (uint32_t)minusHid | ((uint32_t)plusHid << 16);
}

static AxisBinding UnpackAxis(uint32_t p)
{
    AxisBinding b{};
    b.minusHid = (uint16_t)(p & 0xFFFFu);
    b.plusHid = (uint16_t)((p >> 16) & 0xFFFFu);
    return b;
}

static int AxisIdx(Axis a) { return (int)a; }
static int TrigIdx(Trigger t) { return (int)t; }
static int BtnIdx(GameButton b) { return (int)b; }

// Thread-safe storage (backend thread reads, UI thread writes)
static std::array<std::atomic<uint32_t>, 4>  g_axes{};     // packed AxisBinding: minus|plus
static std::array<std::atomic<uint16_t>, 2>  g_triggers{}; // LT,RT

// Buttons: 14 buttons * 4 chunks (0..255)
static std::array<std::array<std::atomic<uint64_t>, 4>, 15> g_btnMask{};

// ---- Axes ----
void Bindings_SetAxisMinus(Axis a, uint16_t hid)
{
    auto& atom = g_axes[AxisIdx(a)];
    uint32_t old = atom.load(std::memory_order_relaxed);
    for (;;)
    {
        AxisBinding b = UnpackAxis(old);
        uint32_t nw = PackAxis(hid, b.plusHid);
        if (atom.compare_exchange_weak(old, nw, std::memory_order_release, std::memory_order_relaxed))
            return;
    }
}

void Bindings_SetAxisPlus(Axis a, uint16_t hid)
{
    auto& atom = g_axes[AxisIdx(a)];
    uint32_t old = atom.load(std::memory_order_relaxed);
    for (;;)
    {
        AxisBinding b = UnpackAxis(old);
        uint32_t nw = PackAxis(b.minusHid, hid);
        if (atom.compare_exchange_weak(old, nw, std::memory_order_release, std::memory_order_relaxed))
            return;
    }
}

AxisBinding Bindings_GetAxis(Axis a)
{
    uint32_t p = g_axes[AxisIdx(a)].load(std::memory_order_acquire);
    return UnpackAxis(p);
}

// ---- Triggers ----
void Bindings_SetTrigger(Trigger t, uint16_t hid)
{
    g_triggers[TrigIdx(t)].store(hid, std::memory_order_release);
}

uint16_t Bindings_GetTrigger(Trigger t)
{
    return g_triggers[TrigIdx(t)].load(std::memory_order_acquire);
}

// ---- Buttons (bitmask HID<256) ----
static bool HidToChunkBit(uint16_t hid, int& outChunk, int& outBit)
{
    if (hid == 0 || hid >= 256) return false;
    outChunk = (int)(hid / 64);
    outBit = (int)(hid % 64);
    return true;
}

void Bindings_AddButtonHid(GameButton b, uint16_t hid)
{
    int chunk = 0, bit = 0;
    if (!HidToChunkBit(hid, chunk, bit)) return;

    g_btnMask[BtnIdx(b)][chunk].fetch_or(1ULL << bit, std::memory_order_release);
}

void Bindings_RemoveButtonHid(GameButton b, uint16_t hid)
{
    int chunk = 0, bit = 0;
    if (!HidToChunkBit(hid, chunk, bit)) return;

    g_btnMask[BtnIdx(b)][chunk].fetch_and(~(1ULL << bit), std::memory_order_release);
}

bool Bindings_ButtonHasHid(GameButton b, uint16_t hid)
{
    int chunk = 0, bit = 0;
    if (!HidToChunkBit(hid, chunk, bit)) return false;

    uint64_t m = g_btnMask[BtnIdx(b)][chunk].load(std::memory_order_acquire);
    return (m & (1ULL << bit)) != 0;
}

uint64_t Bindings_GetButtonMaskChunk(GameButton b, int chunk)
{
    if (chunk < 0 || chunk >= 4) return 0;
    return g_btnMask[BtnIdx(b)][chunk].load(std::memory_order_acquire);
}

// Legacy: return lowest HID set, or 0
static uint16_t FindLowestHidInMask(const std::array<std::atomic<uint64_t>, 4>& m)
{
    for (int chunk = 0; chunk < 4; ++chunk)
    {
        uint64_t v = m[chunk].load(std::memory_order_acquire);
        if (!v) continue;

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        unsigned long idx = 0;
        _BitScanForward64(&idx, v);
        return (uint16_t)(chunk * 64 + (int)idx);
#else
        for (int bit = 0; bit < 64; ++bit)
            if (v & (1ULL << bit))
                return (uint16_t)(chunk * 64 + bit);
#endif
    }
    return 0;
}

uint16_t Bindings_GetButton(GameButton b)
{
    return FindLowestHidInMask(g_btnMask[BtnIdx(b)]);
}

// ---- Clear HID from everywhere ----
void Bindings_ClearHid(uint16_t hid)
{
    if (!hid) return;

    // axes (packed CAS update)
    for (auto& atom : g_axes)
    {
        uint32_t old = atom.load(std::memory_order_relaxed);
        for (;;)
        {
            AxisBinding b = UnpackAxis(old);
            bool changed = false;

            if (b.minusHid == hid) { b.minusHid = 0; changed = true; }
            if (b.plusHid == hid) { b.plusHid = 0; changed = true; }

            if (!changed) break;

            uint32_t nw = PackAxis(b.minusHid, b.plusHid);
            if (atom.compare_exchange_weak(old, nw, std::memory_order_release, std::memory_order_relaxed))
                break;
        }
    }

    // triggers
    for (auto& t : g_triggers)
    {
        uint16_t cur = t.load(std::memory_order_relaxed);
        if (cur == hid)
            t.compare_exchange_strong(cur, 0, std::memory_order_release, std::memory_order_relaxed);
    }

    // buttons (mask)
    if (hid < 256)
    {
        int chunk = (int)(hid / 64);
        int bit = (int)(hid % 64);
        uint64_t mask = ~(1ULL << bit);

        for (auto& btn : g_btnMask)
        {
            btn[chunk].fetch_and(mask, std::memory_order_release);
        }
    }
}