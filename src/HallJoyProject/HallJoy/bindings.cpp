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

static bool IsValidPadIndex(int padIndex)
{
    return padIndex >= 0 && padIndex < BINDINGS_MAX_GAMEPADS;
}

// Thread-safe storage (backend thread reads, UI thread writes)
static std::array<std::array<std::atomic<uint32_t>, 4>, BINDINGS_MAX_GAMEPADS> g_axes{};     // packed AxisBinding: minus|plus
static std::array<std::array<std::atomic<uint16_t>, 2>, BINDINGS_MAX_GAMEPADS> g_triggers{}; // LT,RT

// Buttons: 14 buttons * 4 chunks (0..255)
static std::array<std::array<std::array<std::atomic<uint64_t>, 4>, 15>, BINDINGS_MAX_GAMEPADS> g_btnMask{};
// Pad accent/color identity (1..4), kept separate from pad index so removing a middle pad
// does not force remaining pads to change visual identity.
static std::array<int, BINDINGS_MAX_GAMEPADS> g_padStyle{ 1, 2, 3, 4 };

static int ClampStyleVariant(int v) { return std::clamp(v, 1, BINDINGS_MAX_GAMEPADS); }

// ---- Axes ----
void Bindings_SetAxisMinusForPad(int padIndex, Axis a, uint16_t hid)
{
    if (!IsValidPadIndex(padIndex)) return;
    auto& atom = g_axes[(size_t)padIndex][AxisIdx(a)];
    uint32_t old = atom.load(std::memory_order_relaxed);
    for (;;)
    {
        AxisBinding b = UnpackAxis(old);
        uint32_t nw = PackAxis(hid, b.plusHid);
        if (atom.compare_exchange_weak(old, nw, std::memory_order_release, std::memory_order_relaxed))
            return;
    }
}

void Bindings_SetAxisPlusForPad(int padIndex, Axis a, uint16_t hid)
{
    if (!IsValidPadIndex(padIndex)) return;
    auto& atom = g_axes[(size_t)padIndex][AxisIdx(a)];
    uint32_t old = atom.load(std::memory_order_relaxed);
    for (;;)
    {
        AxisBinding b = UnpackAxis(old);
        uint32_t nw = PackAxis(b.minusHid, hid);
        if (atom.compare_exchange_weak(old, nw, std::memory_order_release, std::memory_order_relaxed))
            return;
    }
}

AxisBinding Bindings_GetAxisForPad(int padIndex, Axis a)
{
    if (!IsValidPadIndex(padIndex)) return AxisBinding{};
    uint32_t p = g_axes[(size_t)padIndex][AxisIdx(a)].load(std::memory_order_acquire);
    return UnpackAxis(p);
}

void Bindings_SetAxisMinus(Axis a, uint16_t hid) { Bindings_SetAxisMinusForPad(0, a, hid); }
void Bindings_SetAxisPlus(Axis a, uint16_t hid) { Bindings_SetAxisPlusForPad(0, a, hid); }
AxisBinding Bindings_GetAxis(Axis a) { return Bindings_GetAxisForPad(0, a); }

// ---- Triggers ----
void Bindings_SetTriggerForPad(int padIndex, Trigger t, uint16_t hid)
{
    if (!IsValidPadIndex(padIndex)) return;
    g_triggers[(size_t)padIndex][TrigIdx(t)].store(hid, std::memory_order_release);
}

uint16_t Bindings_GetTriggerForPad(int padIndex, Trigger t)
{
    if (!IsValidPadIndex(padIndex)) return 0;
    return g_triggers[(size_t)padIndex][TrigIdx(t)].load(std::memory_order_acquire);
}

void Bindings_SetTrigger(Trigger t, uint16_t hid) { Bindings_SetTriggerForPad(0, t, hid); }
uint16_t Bindings_GetTrigger(Trigger t) { return Bindings_GetTriggerForPad(0, t); }

// ---- Buttons (bitmask HID<256) ----
static bool HidToChunkBit(uint16_t hid, int& outChunk, int& outBit)
{
    if (hid == 0 || hid >= 256) return false;
    outChunk = (int)(hid / 64);
    outBit = (int)(hid % 64);
    return true;
}

void Bindings_AddButtonHidForPad(int padIndex, GameButton b, uint16_t hid)
{
    if (!IsValidPadIndex(padIndex)) return;
    int chunk = 0, bit = 0;
    if (!HidToChunkBit(hid, chunk, bit)) return;

    g_btnMask[(size_t)padIndex][BtnIdx(b)][chunk].fetch_or(1ULL << bit, std::memory_order_release);
}

void Bindings_RemoveButtonHidForPad(int padIndex, GameButton b, uint16_t hid)
{
    if (!IsValidPadIndex(padIndex)) return;
    int chunk = 0, bit = 0;
    if (!HidToChunkBit(hid, chunk, bit)) return;

    g_btnMask[(size_t)padIndex][BtnIdx(b)][chunk].fetch_and(~(1ULL << bit), std::memory_order_release);
}

bool Bindings_ButtonHasHidForPad(int padIndex, GameButton b, uint16_t hid)
{
    if (!IsValidPadIndex(padIndex)) return false;
    int chunk = 0, bit = 0;
    if (!HidToChunkBit(hid, chunk, bit)) return false;

    uint64_t m = g_btnMask[(size_t)padIndex][BtnIdx(b)][chunk].load(std::memory_order_acquire);
    return (m & (1ULL << bit)) != 0;
}

uint64_t Bindings_GetButtonMaskChunkForPad(int padIndex, GameButton b, int chunk)
{
    if (!IsValidPadIndex(padIndex)) return 0;
    if (chunk < 0 || chunk >= 4) return 0;
    return g_btnMask[(size_t)padIndex][BtnIdx(b)][chunk].load(std::memory_order_acquire);
}

void Bindings_AddButtonHid(GameButton b, uint16_t hid) { Bindings_AddButtonHidForPad(0, b, hid); }
void Bindings_RemoveButtonHid(GameButton b, uint16_t hid) { Bindings_RemoveButtonHidForPad(0, b, hid); }
bool Bindings_ButtonHasHid(GameButton b, uint16_t hid) { return Bindings_ButtonHasHidForPad(0, b, hid); }
uint64_t Bindings_GetButtonMaskChunk(GameButton b, int chunk) { return Bindings_GetButtonMaskChunkForPad(0, b, chunk); }

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

uint16_t Bindings_GetButtonForPad(int padIndex, GameButton b)
{
    if (!IsValidPadIndex(padIndex)) return 0;
    return FindLowestHidInMask(g_btnMask[(size_t)padIndex][BtnIdx(b)]);
}

uint16_t Bindings_GetButton(GameButton b) { return Bindings_GetButtonForPad(0, b); }

// ---- Clear HID from everywhere ----
void Bindings_ClearHidForPad(int padIndex, uint16_t hid)
{
    if (!IsValidPadIndex(padIndex)) return;
    if (!hid) return;

    // axes (packed CAS update)
    for (auto& atom : g_axes[(size_t)padIndex])
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
    for (auto& t : g_triggers[(size_t)padIndex])
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

        for (auto& btn : g_btnMask[(size_t)padIndex])
        {
            btn[chunk].fetch_and(mask, std::memory_order_release);
        }
    }
}

bool Bindings_IsHidBoundForPad(int padIndex, uint16_t hid)
{
    if (!IsValidPadIndex(padIndex)) return false;
    if (!hid) return false;

    for (int i = 0; i < 4; ++i)
    {
        AxisBinding a = UnpackAxis(g_axes[(size_t)padIndex][(size_t)i].load(std::memory_order_acquire));
        if (a.minusHid == hid || a.plusHid == hid)
            return true;
    }

    for (int i = 0; i < 2; ++i)
    {
        if (g_triggers[(size_t)padIndex][(size_t)i].load(std::memory_order_acquire) == hid)
            return true;
    }

    if (hid < 256)
    {
        const int chunk = (int)(hid / 64);
        const int bit = (int)(hid % 64);
        const uint64_t mask = (1ULL << bit);
        for (size_t b = 0; b < g_btnMask[(size_t)padIndex].size(); ++b)
        {
            if (g_btnMask[(size_t)padIndex][b][(size_t)chunk].load(std::memory_order_acquire) & mask)
                return true;
        }
    }

    return false;
}

void Bindings_SetPadStyleVariant(int padIndex, int styleVariant)
{
    if (!IsValidPadIndex(padIndex)) return;
    g_padStyle[(size_t)padIndex] = ClampStyleVariant(styleVariant);
}

int Bindings_GetPadStyleVariant(int padIndex)
{
    if (!IsValidPadIndex(padIndex)) return 1;
    int v = g_padStyle[(size_t)padIndex];
    if (v < 1 || v > BINDINGS_MAX_GAMEPADS)
    {
        v = padIndex + 1;
        g_padStyle[(size_t)padIndex] = v;
    }
    return v;
}

static void CopyPadBindingsAtomic(int dstPad, int srcPad)
{
    if (!IsValidPadIndex(dstPad) || !IsValidPadIndex(srcPad)) return;

    for (int a = 0; a < 4; ++a)
    {
        uint32_t v = g_axes[(size_t)srcPad][(size_t)a].load(std::memory_order_acquire);
        g_axes[(size_t)dstPad][(size_t)a].store(v, std::memory_order_release);
    }

    for (int t = 0; t < 2; ++t)
    {
        uint16_t v = g_triggers[(size_t)srcPad][(size_t)t].load(std::memory_order_acquire);
        g_triggers[(size_t)dstPad][(size_t)t].store(v, std::memory_order_release);
    }

    for (int b = 0; b < 15; ++b)
    {
        for (int c = 0; c < 4; ++c)
        {
            uint64_t v = g_btnMask[(size_t)srcPad][(size_t)b][(size_t)c].load(std::memory_order_acquire);
            g_btnMask[(size_t)dstPad][(size_t)b][(size_t)c].store(v, std::memory_order_release);
        }
    }
}

static void ClearPadBindingsAtomic(int padIndex)
{
    if (!IsValidPadIndex(padIndex)) return;

    for (int a = 0; a < 4; ++a)
        g_axes[(size_t)padIndex][(size_t)a].store(0u, std::memory_order_release);

    for (int t = 0; t < 2; ++t)
        g_triggers[(size_t)padIndex][(size_t)t].store(0u, std::memory_order_release);

    for (int b = 0; b < 15; ++b)
        for (int c = 0; c < 4; ++c)
            g_btnMask[(size_t)padIndex][(size_t)b][(size_t)c].store(0ull, std::memory_order_release);
}

void Bindings_RemovePadAndCompact(int removePadIndex, int activePadCount)
{
    activePadCount = std::clamp(activePadCount, 1, BINDINGS_MAX_GAMEPADS);
    if (!IsValidPadIndex(removePadIndex)) return;
    if (removePadIndex <= 0) return; // pad #1 is always present
    if (removePadIndex >= activePadCount) return;

    for (int p = removePadIndex; p < activePadCount - 1; ++p)
    {
        CopyPadBindingsAtomic(p, p + 1);
        g_padStyle[(size_t)p] = g_padStyle[(size_t)(p + 1)];
    }

    ClearPadBindingsAtomic(activePadCount - 1);

    // Keep a stable pool of unique style ids among active pads.
    bool used[BINDINGS_MAX_GAMEPADS + 1]{};
    for (int p = 0; p < activePadCount - 1; ++p)
    {
        int sv = ClampStyleVariant(g_padStyle[(size_t)p]);
        used[sv] = true;
    }
    int freeSv = 1;
    for (int sv = 1; sv <= BINDINGS_MAX_GAMEPADS; ++sv)
    {
        if (!used[sv]) { freeSv = sv; break; }
    }
    g_padStyle[(size_t)(activePadCount - 1)] = freeSv;
}

void Bindings_ClearHid(uint16_t hid)
{
    for (int pad = 0; pad < BINDINGS_MAX_GAMEPADS; ++pad)
        Bindings_ClearHidForPad(pad, hid);
}

bool Bindings_IsHidBound(uint16_t hid)
{
    for (int pad = 0; pad < BINDINGS_MAX_GAMEPADS; ++pad)
    {
        if (Bindings_IsHidBoundForPad(pad, hid))
            return true;
    }
    return false;
}
