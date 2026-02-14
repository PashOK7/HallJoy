// backend.cpp
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include <ViGEm/Client.h>
#include "wooting-analog-wrapper.h"

#include "backend.h"
#include "bindings.h"
#include "settings.h"
#include "key_settings.h"
#include "debug_log.h"
#include "mouse_bind_codes.h"

// shared curve math (single source of truth with UI)
#include "curve_math.h"

#pragma comment(lib, "setupapi.lib")

static PVIGEM_CLIENT g_client = nullptr;
static constexpr int kMaxVirtualPads = 4;
static std::array<PVIGEM_TARGET, kMaxVirtualPads> g_pads{};
static std::atomic<int> g_virtualPadCount{ 1 };
static std::atomic<bool> g_virtualPadsEnabled{ true };
static int g_connectedPadCount = 0;

static std::array<XUSB_REPORT, kMaxVirtualPads> g_reports{};
static std::array<XUSB_REPORT, kMaxVirtualPads> g_lastSentReports{};
static std::array<DWORD, kMaxVirtualPads> g_lastSentTicks{};
static std::array<uint8_t, kMaxVirtualPads> g_lastSentValid{};

// Thread-safe last-report snapshot (writer: realtime thread, reader: UI thread)
static std::array<std::atomic<uint32_t>, kMaxVirtualPads> g_lastSeq{};
static std::array<XUSB_REPORT, kMaxVirtualPads> g_lastReport{};
static std::array<std::atomic<SHORT>, kMaxVirtualPads> g_lastRX{};

// ---- UI snapshot ----
static std::array<std::atomic<uint16_t>, 256> g_uiAnalogM{}; // filtered output (after curve)
static std::array<std::atomic<uint16_t>, 256> g_uiRawM{};    // NEW: raw input
static std::array<std::atomic<uint64_t>, 4>   g_uiDirty{};   // dirty for filtered only

// list of HID codes to track (provided by UI)
static std::array<uint16_t, 256> g_trackedList{};
static std::atomic<int>          g_trackedCount{ 0 };

// bind-capture state (layout editor)
static std::atomic<bool>         g_bindCaptureEnabled{ false };
static std::atomic<uint32_t>     g_bindCapturedPacked{ 0 }; // low16=hid, high16=rawMilli
static std::atomic<bool>         g_bindHadDown{ false };

// ---- status / reconnect ----
static std::atomic<bool>         g_vigemOk{ false };
static std::atomic<VIGEM_ERROR>  g_vigemLastErr{ VIGEM_ERROR_NONE };
static std::atomic<uint32_t>     g_lastInitIssues{ BackendInitIssue_None };
static std::atomic<bool>         g_reconnectRequested{ false }; // immediate reconnect (settings change)
static std::atomic<bool>         g_deviceChangeReconnectRequested{ false }; // throttled reconnect (WM_DEVICECHANGE)
static std::atomic<ULONGLONG>    g_ignoreDeviceChangeUntilMs{ 0 };
static int                       g_vigemUpdateFailStreak = 0;
static ULONGLONG                 g_lastReconnectAttemptMs = 0;
static std::atomic<int>          g_lastAnalogErrorCode{ 0 };
static std::atomic<ULONGLONG>    g_lastAnalogErrorLogMs{ 0 };
static std::atomic<ULONGLONG>    g_lastWootingStateLogMs{ 0 };
static std::atomic<ULONGLONG>    g_lastInputStateLogMs{ 0 };
static std::atomic<int>          g_keycodeMode{ (int)WootingAnalog_KeycodeType_HID };
static std::atomic<ULONGLONG>    g_lastKeycodeSwitchMs{ 0 };
static std::atomic<uint32_t>     g_keyboardEventSeq{ 0 };
static std::atomic<uint16_t>     g_keyboardEventHid{ 0 };
static std::atomic<uint16_t>     g_keyboardEventScan{ 0 };
static std::atomic<uint16_t>     g_keyboardEventVk{ 0 };
static std::array<std::atomic<uint16_t>, 256> g_hidToScan{};
static std::array<std::atomic<uint16_t>, 256> g_hidToVk{};
static std::atomic<ULONGLONG>    g_lastFullBufferLogMs{ 0 };
static std::atomic<int>          g_zeroProbeStreak{ 0 };
static std::atomic<bool>         g_autoRecoverTried{ false };
static std::array<WootingAnalog_DeviceID, 16> g_knownDeviceIds{};
static std::atomic<int>          g_knownDeviceCount{ 0 };
static std::atomic<uint16_t>     g_tmTrackedMaxRawMilli{ 0 };
static std::atomic<uint16_t>     g_tmTrackedMaxOutMilli{ 0 };
static std::atomic<int>          g_tmFullBufferRet{ 0 };
static std::atomic<uint16_t>     g_tmFullBufferMaxMilli{ 0 };
static std::atomic<int>          g_tmFullBufferDeviceBestRet{ 0 };
static std::atomic<uint16_t>     g_tmFullBufferDeviceBestMaxMilli{ 0 };
static std::atomic<bool>         g_digitalFallbackWarnPending{ false };
static std::atomic<bool>         g_keycodeModeLocked{ false };
static constexpr bool            kEnableAdaptiveKeycodeModeProbe = false;
static constexpr bool            kEnableFullBufferAssist = false;
static POINT                     g_mouseLastPos{};
static bool                      g_mouseHasLastPos = false;
static std::atomic<bool>         g_mouseSawRawInput{ false };
static float                     g_mouseFilteredX = 0.0f;
static float                     g_mouseFilteredY = 0.0f;
static double                    g_mouseTargetX = 0.0;   // integrated mouse displacement (virtual cursor)
static double                    g_mouseTargetY = 0.0;
static double                    g_mouseFollowerX = 0.0; // virtual "stick-controlled" anchor
static double                    g_mouseFollowerY = 0.0;
static ULONGLONG                 g_mouseLastTickMs = 0;
static std::atomic<int>          g_mouseRawAccumDx{ 0 };
static std::atomic<int>          g_mouseRawAccumDy{ 0 };
static std::atomic<uint8_t>      g_mouseBindButtons[5]{};
static std::atomic<ULONGLONG>    g_mouseWheelPulseUpUntilMs{ 0 };
static std::atomic<ULONGLONG>    g_mouseWheelPulseDownUntilMs{ 0 };
static std::atomic<uint8_t>      g_mouseDbgEnabled{ 0 };
static std::atomic<uint8_t>      g_mouseDbgUsingRaw{ 0 };
static std::atomic<int>          g_mouseDbgTargetX10{ 0 };
static std::atomic<int>          g_mouseDbgTargetY10{ 0 };
static std::atomic<int>          g_mouseDbgFollowerX10{ 0 };
static std::atomic<int>          g_mouseDbgFollowerY10{ 0 };
static std::atomic<int>          g_mouseDbgOutX1000{ 0 };
static std::atomic<int>          g_mouseDbgOutY1000{ 0 };
static std::atomic<int>          g_mouseDbgRadius1000{ 1000 };

static const wchar_t* KeycodeModeName(int mode)
{
    switch ((WootingAnalog_KeycodeType)mode)
    {
    case WootingAnalog_KeycodeType_HID: return L"HID";
    case WootingAnalog_KeycodeType_ScanCode1: return L"ScanCode1";
    case WootingAnalog_KeycodeType_VirtualKey: return L"VirtualKey";
    case WootingAnalog_KeycodeType_VirtualKeyTranslate: return L"VirtualKeyTranslate";
    default: return L"Unknown";
    }
}

static WootingAnalog_KeycodeType NextKeycodeMode(WootingAnalog_KeycodeType mode)
{
    switch (mode)
    {
    case WootingAnalog_KeycodeType_HID: return WootingAnalog_KeycodeType_ScanCode1;
    case WootingAnalog_KeycodeType_ScanCode1: return WootingAnalog_KeycodeType_VirtualKey;
    case WootingAnalog_KeycodeType_VirtualKey: return WootingAnalog_KeycodeType_VirtualKeyTranslate;
    case WootingAnalog_KeycodeType_VirtualKeyTranslate: return WootingAnalog_KeycodeType_HID;
    default: return WootingAnalog_KeycodeType_HID;
    }
}

static bool SetKeycodeModeWithLog(WootingAnalog_KeycodeType mode, const wchar_t* reason, uint16_t hidHint)
{
    WootingAnalogResult r = wooting_analog_set_keycode_mode(mode);
    DebugLog_Write(
        L"[backend.mode] set mode=%s(%d) reason=%s hid_hint=%u ret=%d",
        KeycodeModeName((int)mode), (int)mode,
        reason ? reason : L"-",
        (unsigned)hidHint,
        (int)r);
    if (r >= 0)
    {
        g_keycodeMode.store((int)mode, std::memory_order_relaxed);
        g_lastKeycodeSwitchMs.store(GetTickCount64(), std::memory_order_relaxed);
        return true;
    }
    return false;
}

static void LogConnectedDevicesDetailed(const wchar_t* stage)
{
    WootingAnalog_DeviceInfo_FFI* devs[16]{};
    int n = wooting_analog_get_connected_devices_info(devs, (unsigned)_countof(devs));
    if (n < 0)
    {
        g_knownDeviceCount.store(0, std::memory_order_relaxed);
        DebugLog_Write(L"[backend.devices] %s get_devices_ret=%d", stage ? stage : L"-", n);
        return;
    }

    DebugLog_Write(L"[backend.devices] %s count=%d", stage ? stage : L"-", n);
    g_knownDeviceCount.store(0, std::memory_order_relaxed);
    int uniqueCount = 0;
    for (int i = 0; i < n && i < (int)_countof(devs); ++i)
    {
        const WootingAnalog_DeviceInfo_FFI* d = devs[i];
        if (!d) continue;
        const char* m = d->manufacturer_name ? d->manufacturer_name : "";
        const char* name = d->device_name ? d->device_name : "";
        DebugLog_Write(
            L"[backend.devices] #%d type=%d vid=0x%04X pid=0x%04X id=%llu mfr=%S name=%S",
            i,
            (int)d->device_type,
            (unsigned)d->vendor_id,
            (unsigned)d->product_id,
            (unsigned long long)d->device_id,
            m,
            name);

        bool dup = false;
        for (int k = 0; k < uniqueCount; ++k)
        {
            if (g_knownDeviceIds[k] == d->device_id)
            {
                dup = true;
                break;
            }
        }
        if (!dup && uniqueCount < (int)g_knownDeviceIds.size())
            g_knownDeviceIds[uniqueCount++] = d->device_id;
    }
    g_knownDeviceCount.store(uniqueCount, std::memory_order_relaxed);
    DebugLog_Write(L"[backend.devices] unique_ids=%d", uniqueCount);
}

static uint16_t HidFallbackToVk(uint16_t hid)
{
    if (hid >= 4 && hid <= 29)  return (uint16_t)('A' + (hid - 4)); // A..Z
    if (hid >= 30 && hid <= 38) return (uint16_t)('1' + (hid - 30)); // 1..9
    if (hid == 39) return (uint16_t)'0';
    switch (hid)
    {
    case 40: return VK_RETURN;
    case 41: return VK_ESCAPE;
    case 42: return VK_BACK;
    case 43: return VK_TAB;
    case 44: return VK_SPACE;
    case 45: return VK_OEM_MINUS;
    case 46: return VK_OEM_PLUS;
    case 47: return VK_OEM_4;
    case 48: return VK_OEM_6;
    case 49: return VK_OEM_5;
    case 51: return VK_OEM_1;
    case 52: return VK_OEM_7;
    case 54: return VK_OEM_COMMA;
    case 55: return VK_OEM_PERIOD;
    case 56: return VK_OEM_2;
    case 57: return VK_CAPITAL;
    case 58: return VK_F1;
    case 59: return VK_F2;
    case 60: return VK_F3;
    case 61: return VK_F4;
    case 62: return VK_F5;
    case 63: return VK_F6;
    case 64: return VK_F7;
    case 65: return VK_F8;
    case 66: return VK_F9;
    case 67: return VK_F10;
    case 68: return VK_F11;
    case 69: return VK_F12;
    case 73: return VK_INSERT;
    case 74: return VK_HOME;
    case 75: return VK_PRIOR;
    case 76: return VK_DELETE;
    case 77: return VK_END;
    case 78: return VK_NEXT;
    case 79: return VK_RIGHT;
    case 80: return VK_LEFT;
    case 81: return VK_DOWN;
    case 82: return VK_UP;
    case 83: return VK_NUMLOCK;
    case 84: return VK_DIVIDE;
    case 85: return VK_MULTIPLY;
    case 86: return VK_SUBTRACT;
    case 87: return VK_ADD;
    case 89: return VK_NUMPAD1;
    case 90: return VK_NUMPAD2;
    case 91: return VK_NUMPAD3;
    case 92: return VK_NUMPAD4;
    case 93: return VK_NUMPAD5;
    case 94: return VK_NUMPAD6;
    case 95: return VK_NUMPAD7;
    case 96: return VK_NUMPAD8;
    case 97: return VK_NUMPAD9;
    case 98: return VK_NUMPAD0;
    case 99: return VK_DECIMAL;
    case 224: return VK_LCONTROL;
    case 225: return VK_LSHIFT;
    case 226: return VK_LMENU;
    case 227: return VK_LWIN;
    case 228: return VK_RCONTROL;
    case 229: return VK_RSHIFT;
    case 230: return VK_RMENU;
    case 231: return VK_RWIN;
    default: return 0;
    }
}

static uint16_t HidToModeCode(uint16_t hid, WootingAnalog_KeycodeType mode)
{
    if (hid == 0) return 0;
    if (mode == WootingAnalog_KeycodeType_HID)
        return hid;

    if (hid < 256)
    {
        if (mode == WootingAnalog_KeycodeType_ScanCode1)
        {
            uint16_t sc = g_hidToScan[hid].load(std::memory_order_relaxed);
            return sc;
        }
        if (mode == WootingAnalog_KeycodeType_VirtualKey || mode == WootingAnalog_KeycodeType_VirtualKeyTranslate)
        {
            uint16_t vk = g_hidToVk[hid].load(std::memory_order_relaxed);
            if (vk != 0) return vk;
            return HidFallbackToVk(hid);
        }
    }
    return 0;
}

static float SafeReadAnalogByCode(uint16_t code)
{
    if (code == 0) return 0.0f;
    float v = wooting_analog_read_analog(code);
    if (!std::isfinite(v) || v < 0.0f) return 0.0f;
    return std::clamp(v, 0.0f, 1.0f);
}

static float ReadAnalogByCodeWithDeviceFallback(uint16_t code, uint16_t hidForLog)
{
    if (code == 0) return 0.0f;
    float base = wooting_analog_read_analog(code);
    float best = (std::isfinite(base) ? base : 0.0f);

    int n = std::clamp(g_knownDeviceCount.load(std::memory_order_relaxed), 0, (int)g_knownDeviceIds.size());
    for (int i = 0; i < n; ++i)
    {
        WootingAnalog_DeviceID id = g_knownDeviceIds[i];
        float dv = wooting_analog_read_analog_device(code, id);
        if (!std::isfinite(dv)) continue;
        if (dv > best)
            best = dv;
    }

    if (best > base + 0.0005f)
    {
        DebugLog_Write(
            L"[backend.analog] device_fallback improved hid=%u code=%u base=%.3f best=%.3f",
            (unsigned)hidForLog,
            (unsigned)code,
            base,
            best);
    }

    return best;
}

static bool ProbeKeycodeModeByFullBufferActivity(uint16_t hidHint)
{
    struct ModeProbe
    {
        WootingAnalog_KeycodeType mode;
        int setRet = -9999;
        int readRet = -9999;
        float maxV = 0.0f;
    };

    ModeProbe probes[] = {
        { WootingAnalog_KeycodeType_HID },
        { WootingAnalog_KeycodeType_ScanCode1 },
        { WootingAnalog_KeycodeType_VirtualKey },
        { WootingAnalog_KeycodeType_VirtualKeyTranslate },
    };

    int currentMode = g_keycodeMode.load(std::memory_order_relaxed);
    int bestIdx = -1;
    int bestReadRet = -1;
    float bestMax = 0.0f;

    for (int i = 0; i < (int)_countof(probes); ++i)
    {
        WootingAnalogResult sr = wooting_analog_set_keycode_mode(probes[i].mode);
        probes[i].setRet = (int)sr;
        if (sr < 0)
            continue;

        unsigned short codes[64]{};
        float vals[64]{};
        int rr = wooting_analog_read_full_buffer(codes, vals, (unsigned)_countof(codes));
        probes[i].readRet = rr;
        if (rr < 0)
            continue;

        int n = std::min(rr, (int)_countof(vals));
        float maxV = 0.0f;
        for (int j = 0; j < n; ++j)
        {
            float v = vals[j];
            if (std::isfinite(v) && v > maxV)
                maxV = v;
        }
        probes[i].maxV = maxV;

        // Prefer mode with highest max analog first, then by more reported keys.
        if (maxV > bestMax + 0.005f || (std::abs(maxV - bestMax) <= 0.005f && rr > bestReadRet))
        {
            bestMax = maxV;
            bestReadRet = rr;
            bestIdx = i;
        }
    }

    DebugLog_Write(
        L"[backend.mode] full_probe hid=%u HID(ret=%d,max=%.3f) SC(ret=%d,max=%.3f) VK(ret=%d,max=%.3f) VKT(ret=%d,max=%.3f)",
        (unsigned)hidHint,
        probes[0].readRet, probes[0].maxV,
        probes[1].readRet, probes[1].maxV,
        probes[2].readRet, probes[2].maxV,
        probes[3].readRet, probes[3].maxV);

    if (bestIdx >= 0 && (bestReadRet > 0 || bestMax >= 0.02f))
    {
        WootingAnalog_KeycodeType target = probes[bestIdx].mode;
        if ((int)target != currentMode)
            SetKeycodeModeWithLog(target, L"full_probe", hidHint);
        else
            wooting_analog_set_keycode_mode((WootingAnalog_KeycodeType)currentMode);
        return true;
    }

    // Restore current mode after temporary probing.
    wooting_analog_set_keycode_mode((WootingAnalog_KeycodeType)currentMode);
    return false;
}

static bool AutoProbeKeycodeModeFromEvent(uint16_t hidHint, uint16_t scanCode, uint16_t vkCode)
{
    if (hidHint == 0) return false;

    struct ProbeItem { WootingAnalog_KeycodeType mode; uint16_t code; float value; int setRet; };
    ProbeItem items[] = {
        { WootingAnalog_KeycodeType_HID, hidHint, 0.0f, 0 },
        { WootingAnalog_KeycodeType_ScanCode1, scanCode, 0.0f, 0 },
        { WootingAnalog_KeycodeType_VirtualKey, vkCode, 0.0f, 0 },
        { WootingAnalog_KeycodeType_VirtualKeyTranslate, vkCode, 0.0f, 0 },
    };

    int currentMode = g_keycodeMode.load(std::memory_order_relaxed);
    int bestIdx = -1;
    float bestVal = 0.0f;
    float currentVal = 0.0f;

    for (int i = 0; i < (int)_countof(items); ++i)
    {
        if (items[i].code == 0)
            continue;
        WootingAnalogResult sr = wooting_analog_set_keycode_mode(items[i].mode);
        items[i].setRet = (int)sr;
        if (sr < 0)
            continue;
        items[i].value = SafeReadAnalogByCode(items[i].code);
        if ((int)items[i].mode == currentMode)
            currentVal = items[i].value;
        if (items[i].value > bestVal)
        {
            bestVal = items[i].value;
            bestIdx = i;
        }
    }

    DebugLog_Write(
        L"[backend.mode] probe hid=%u scan=%u vk=%u values: HID=%.3f SC=%.3f VK=%.3f VKT=%.3f",
        (unsigned)hidHint, (unsigned)scanCode, (unsigned)vkCode,
        items[0].value, items[1].value, items[2].value, items[3].value);

    bool foundWorkingMode = (bestIdx >= 0 && bestVal >= 0.015f);
    if (foundWorkingMode)
    {
        WootingAnalog_KeycodeType targetMode = items[bestIdx].mode;
        // Switch only when the alternative mode is meaningfully better.
        if ((int)targetMode != currentMode && bestVal > currentVal + 0.01f)
            SetKeycodeModeWithLog(targetMode, L"auto_probe", hidHint);
        else
            wooting_analog_set_keycode_mode((WootingAnalog_KeycodeType)currentMode);
        return true;
    }

    // Restore current mode after temporary probe switching.
    wooting_analog_set_keycode_mode((WootingAnalog_KeycodeType)currentMode);
    return false;
}

static void LogFullBufferSnapshot(const wchar_t* stage)
{
    unsigned short codes[64]{};
    float vals[64]{};
    int ret = wooting_analog_read_full_buffer(codes, vals, (unsigned)_countof(codes));
    if (ret < 0)
    {
        g_tmFullBufferRet.store(ret, std::memory_order_relaxed);
        g_tmFullBufferMaxMilli.store(0, std::memory_order_relaxed);
        g_tmFullBufferDeviceBestRet.store(ret, std::memory_order_relaxed);
        g_tmFullBufferDeviceBestMaxMilli.store(0, std::memory_order_relaxed);
        DebugLog_Write(
            L"[backend.full] %s ret=%d mode=%s",
            stage ? stage : L"-",
            ret,
            KeycodeModeName(g_keycodeMode.load(std::memory_order_relaxed)));
        return;
    }

    int n = std::min(ret, (int)_countof(codes));
    float maxV = 0.0f;
    unsigned short maxCode = 0;
    for (int i = 0; i < n; ++i)
    {
        float v = vals[i];
        if (std::isfinite(v) && v > maxV)
        {
            maxV = v;
            maxCode = codes[i];
        }
    }

    DebugLog_Write(
        L"[backend.full] %s ret=%d max=%.3f code=%u mode=%s",
        stage ? stage : L"-",
        ret,
        maxV,
        (unsigned)maxCode,
        KeycodeModeName(g_keycodeMode.load(std::memory_order_relaxed)));
    g_tmFullBufferRet.store(ret, std::memory_order_relaxed);
    g_tmFullBufferMaxMilli.store((uint16_t)std::clamp((int)std::lround(maxV * 1000.0f), 0, 1000), std::memory_order_relaxed);

    int ndev = std::clamp(g_knownDeviceCount.load(std::memory_order_relaxed), 0, (int)g_knownDeviceIds.size());
    int bestDevRet = ret;
    uint16_t bestDevMilli = (uint16_t)std::clamp((int)std::lround(maxV * 1000.0f), 0, 1000);
    for (int di = 0; di < ndev; ++di)
    {
        WootingAnalog_DeviceID id = g_knownDeviceIds[di];
        unsigned short dcodes[64]{};
        float dvals[64]{};
        int dret = wooting_analog_read_full_buffer_device(dcodes, dvals, (unsigned)_countof(dcodes), id);
        if (dret < 0)
        {
            DebugLog_Write(
                L"[backend.full.dev] %s dev#%d id=%llu ret=%d",
                stage ? stage : L"-",
                di,
                (unsigned long long)id,
                dret);
            continue;
        }
        int dn = std::min(dret, (int)_countof(dcodes));
        float dmax = 0.0f;
        unsigned short dcode = 0;
        for (int i = 0; i < dn; ++i)
        {
            float v = dvals[i];
            if (std::isfinite(v) && v > dmax)
            {
                dmax = v;
                dcode = dcodes[i];
            }
        }
        DebugLog_Write(
            L"[backend.full.dev] %s dev#%d id=%llu ret=%d max=%.3f code=%u",
            stage ? stage : L"-",
            di,
            (unsigned long long)id,
            dret,
            dmax,
            (unsigned)dcode);

        uint16_t dm = (uint16_t)std::clamp((int)std::lround(dmax * 1000.0f), 0, 1000);
        if (dm > bestDevMilli)
        {
            bestDevMilli = dm;
            bestDevRet = dret;
        }
    }
    g_tmFullBufferDeviceBestRet.store(bestDevRet, std::memory_order_relaxed);
    g_tmFullBufferDeviceBestMaxMilli.store(bestDevMilli, std::memory_order_relaxed);
}

static void LogWootingStateSnapshot(const wchar_t* stage)
{
    WootingAnalog_DeviceInfo_FFI* devs[16]{};
    int devRet = wooting_analog_get_connected_devices_info(devs, (unsigned)_countof(devs));
    bool inited = wooting_analog_is_initialised();
    DebugLog_Write(
        L"[backend.wooting] %s init=%d get_devices_ret=%d keycode_mode=%d",
        stage ? stage : L"(null)",
        inited ? 1 : 0,
        devRet,
        g_keycodeMode.load(std::memory_order_relaxed));
}

// ------------------------------------------------------------
// Curve logic (shared with UI via CurveMath)
// ------------------------------------------------------------
struct CurveDef
{
    // Points in normalized space (0..1)
    float x0 = 0.0f, y0 = 0.0f; // Start
    float x1 = 0.0f, y1 = 0.0f; // CP1
    float x2 = 0.0f, y2 = 0.0f; // CP2
    float x3 = 1.0f, y3 = 1.0f; // End

    // CP weights in [0..1] (used only for Smooth mode)
    float w1 = 1.0f;
    float w2 = 1.0f;

    UINT mode = 0; // 0=Smooth (Rational Bezier), 1=Linear (Segments)
    bool invert = false;
};

static float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

static float ApplyCurve_LinearSegments(float x, const CurveDef& c)
{
    // piecewise interpolation between (x0,y0)->(x1,y1)->(x2,y2)->(x3,y3)
    float xa, ya, xb, yb;

    if (x <= c.x1) {
        xa = c.x0; ya = c.y0;
        xb = c.x1; yb = c.y1;
    }
    else if (x <= c.x2) {
        xa = c.x1; ya = c.y1;
        xb = c.x2; yb = c.y2;
    }
    else {
        xa = c.x2; ya = c.y2;
        xb = c.x3; yb = c.y3;
    }

    float denom = (xb - xa);
    if (std::fabs(denom) < 1e-6f) return Clamp01(yb);

    float t = (x - xa) / denom;
    t = std::clamp(t, 0.0f, 1.0f);
    float y = ya + (yb - ya) * t;
    return Clamp01(y);
}

static float ApplyCurve_SmoothRationalBezier(float x, const CurveDef& c)
{
    CurveMath::Curve01 cc{};
    cc.x0 = c.x0; cc.y0 = c.y0;
    cc.x1 = c.x1; cc.y1 = c.y1;
    cc.x2 = c.x2; cc.y2 = c.y2;
    cc.x3 = c.x3; cc.y3 = c.y3;
    cc.w1 = Clamp01(c.w1);
    cc.w2 = Clamp01(c.w2);

    // Solve x(t)=x, return y(t)
    return CurveMath::EvalRationalYForX(cc, x, 18);
}

static CurveDef BuildCurveForHid(uint16_t hid)
{
    CurveDef c{};
    KeyDeadzone ks = KeySettings_Get(hid);

    if (ks.useUnique)
    {
        c.invert = ks.invert;
        c.mode = (UINT)(ks.curveMode == 0 ? 0 : 1);

        c.x0 = ks.low;          c.y0 = ks.antiDeadzone;
        c.x1 = ks.cp1_x;        c.y1 = ks.cp1_y;
        c.x2 = ks.cp2_x;        c.y2 = ks.cp2_y;
        c.x3 = ks.high;         c.y3 = ks.outputCap;

        c.w1 = ks.cp1_w;
        c.w2 = ks.cp2_w;
    }
    else
    {
        c.invert = Settings_GetInputInvert();
        c.mode = Settings_GetInputCurveMode();

        c.x0 = Settings_GetInputDeadzoneLow();
        c.x3 = Settings_GetInputDeadzoneHigh();
        c.y0 = Settings_GetInputAntiDeadzone();
        c.y3 = Settings_GetInputOutputCap();

        c.x1 = Settings_GetInputBezierCp1X();
        c.y1 = Settings_GetInputBezierCp1Y();
        c.x2 = Settings_GetInputBezierCp2X();
        c.y2 = Settings_GetInputBezierCp2Y();

        c.w1 = Settings_GetInputBezierCp1W();
        c.w2 = Settings_GetInputBezierCp2W();
    }

    c.w1 = Clamp01(c.w1);
    c.w2 = Clamp01(c.w2);

    // Clamp Y to [0..1]
    c.y0 = Clamp01(c.y0); c.y1 = Clamp01(c.y1);
    c.y2 = Clamp01(c.y2); c.y3 = Clamp01(c.y3);

    // Enforce X range + monotonic constraints (needed for x(t) solve)
    c.x0 = Clamp01(c.x0);
    c.x3 = Clamp01(c.x3);
    if (c.x3 < c.x0 + 0.01f) c.x3 = std::clamp(c.x0 + 0.01f, 0.01f, 1.0f);

    float minGap = 0.001f;

    c.x1 = std::clamp(c.x1, c.x0, c.x3);
    c.x2 = std::clamp(c.x2, c.x0, c.x3);

    // enforce order slightly for safety
    c.x1 = std::clamp(c.x1, c.x0, c.x3 - minGap);
    c.x2 = std::clamp(c.x2, c.x1, c.x3);

    return c;
}

static float ApplyCurveByHid(uint16_t hid, float x01Raw)
{
    float x01 = Clamp01(x01Raw);

    CurveDef c = BuildCurveForHid(hid);

    if (c.invert) x01 = 1.0f - x01;

    // Range check against endpoints
    if (x01 < c.x0) return 0.0f;
    if (x01 > c.x3) return Clamp01(c.y3);

    if (c.mode == 1) return ApplyCurve_LinearSegments(x01, c);
    return ApplyCurve_SmoothRationalBezier(x01, c);
}

// ------------------------------------------------------------

static void Vigem_Destroy()
{
    if (g_client)
    {
        for (int i = 0; i < g_connectedPadCount; ++i)
        {
            if (g_pads[(size_t)i])
            {
                vigem_target_remove(g_client, g_pads[(size_t)i]);
                vigem_target_free(g_pads[(size_t)i]);
                g_pads[(size_t)i] = nullptr;
            }
        }
    }

    g_connectedPadCount = 0;
    for (int i = 0; i < kMaxVirtualPads; ++i)
        g_lastSentValid[(size_t)i] = 0;

    if (g_client)
    {
        vigem_disconnect(g_client);
        vigem_free(g_client);
        g_client = nullptr;
    }
}

static bool Vigem_Create(int padCount, VIGEM_ERROR* outErr)
{
    padCount = std::clamp(padCount, 1, kMaxVirtualPads);
    if (outErr) *outErr = VIGEM_ERROR_NONE;
    g_client = vigem_alloc();
    if (!g_client) { if (outErr) *outErr = VIGEM_ERROR_BUS_NOT_FOUND; return false; }
    VIGEM_ERROR err = vigem_connect(g_client);
    if (!VIGEM_SUCCESS(err)) { if (outErr) *outErr = err; vigem_free(g_client); g_client = nullptr; return false; }

    g_connectedPadCount = 0;
    for (int i = 0; i < padCount; ++i)
    {
        PVIGEM_TARGET pad = vigem_target_x360_alloc();
        if (!pad)
        {
            if (outErr) *outErr = VIGEM_ERROR_INVALID_TARGET;
            Vigem_Destroy();
            return false;
        }

        err = vigem_target_add(g_client, pad);
        if (!VIGEM_SUCCESS(err))
        {
            if (outErr) *outErr = err;
            vigem_target_free(pad);
            Vigem_Destroy();
            return false;
        }

        g_pads[(size_t)i] = pad;
        g_connectedPadCount = i + 1;
    }

    if (outErr) *outErr = VIGEM_ERROR_NONE;
    return true;
}

static bool Vigem_ReconnectThrottled(bool force = false)
{
    ULONGLONG now = GetTickCount64();
    if (!force && now - g_lastReconnectAttemptMs < 1000) return false;
    g_lastReconnectAttemptMs = now;
    g_vigemUpdateFailStreak = 0;

    // Reconnect itself emits device-change broadcasts. Suppress them briefly so
    // WM_DEVICECHANGE does not trigger reconnect loops.
    g_ignoreDeviceChangeUntilMs.store(now + 1500, std::memory_order_release);
    Vigem_Destroy();

    if (!g_virtualPadsEnabled.load(std::memory_order_acquire))
    {
        g_vigemOk.store(true, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
        return true;
    }

    VIGEM_ERROR err = VIGEM_ERROR_NONE;
    int wantedPads = std::clamp(g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads);
    bool ok = Vigem_Create(wantedPads, &err);
    g_vigemOk.store(ok, std::memory_order_release);
    g_vigemLastErr.store(ok ? VIGEM_ERROR_NONE : err, std::memory_order_release);
    return ok;
}

// Cache: for HID <= 255 read once per tick
struct HidCache
{
    std::array<float, 256> raw{};
    std::array<float, 256> filtered{};
    std::array<float, 256> fullRaw{};
    std::bitset<256> fullPresent{};
    std::bitset<256> hasRaw{};
    std::bitset<256> hasFiltered{};
    bool hasFullBuffer = false;
};

struct SimulatedKeyState
{
    bool down = false;
    float value = 0.0f;
    ULONGLONG lastUpdateMs = 0;
};

static std::array<SimulatedKeyState, 256> g_simulatedKeys{};

static bool IsHidDownViaAsyncState(uint16_t hidKeycode)
{
    if (hidKeycode == 0 || hidKeycode >= 256) return false;

    uint16_t vk = g_hidToVk[hidKeycode].load(std::memory_order_relaxed);
    if (vk == 0)
        vk = HidFallbackToVk(hidKeycode);
    if (vk == 0)
        return false;

    return (GetAsyncKeyState((int)vk) & 0x8000) != 0;
}

static float ReadDigitalFallback01(uint16_t hidKeycode)
{
    if (hidKeycode == 0 || hidKeycode >= 256)
        return 0.0f;

    SimulatedKeyState& s = g_simulatedKeys[hidKeycode];

    ULONGLONG now = GetTickCount64();
    ULONGLONG prev = s.lastUpdateMs;
    float dtMs = 1.0f;
    if (prev != 0 && now > prev)
    {
        dtMs = (float)(now - prev);
        dtMs = std::clamp(dtMs, 0.5f, 40.0f);
    }
    s.lastUpdateMs = now;

    const bool down = IsHidDownViaAsyncState(hidKeycode);
    s.down = down;

    // Two-stage press curve:
    // 0.00 -> 0.70 in ~50 ms, then 0.70 -> 1.00 in ~50 ms.
    // Release is slightly smoother to avoid harsh jitter on quick taps.
    if (down)
    {
        if (s.value < 0.70f)
            s.value += (0.70f / 50.0f) * dtMs;
        else
            s.value += (0.30f / 50.0f) * dtMs;
    }
    else
    {
        s.value -= (1.00f / 80.0f) * dtMs;
    }

    s.value = std::clamp(s.value, 0.0f, 1.0f);
    return s.value;
}

static float ReadMouseBindRaw01(uint16_t hidKeycode)
{
    switch (hidKeycode)
    {
    case kMouseBindHidLButton: return g_mouseBindButtons[0].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidRButton: return g_mouseBindButtons[1].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidMButton: return g_mouseBindButtons[2].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidX1: return g_mouseBindButtons[3].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidX2: return g_mouseBindButtons[4].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidWheelUp:
    {
        ULONGLONG now = GetTickCount64();
        ULONGLONG until = g_mouseWheelPulseUpUntilMs.load(std::memory_order_relaxed);
        return (now < until) ? 1.0f : 0.0f;
    }
    case kMouseBindHidWheelDown:
    {
        ULONGLONG now = GetTickCount64();
        ULONGLONG until = g_mouseWheelPulseDownUntilMs.load(std::memory_order_relaxed);
        return (now < until) ? 1.0f : 0.0f;
    }
    default:
        return 0.0f;
    }
}

static float ReadRaw01Cached(uint16_t hidKeycode, HidCache& cache)
{
    if (hidKeycode == 0) return 0.0f;
    if (MouseBind_IsPseudoHid(hidKeycode))
        return ReadMouseBindRaw01(hidKeycode);

    const bool allowFallback = Settings_GetDigitalFallbackInput();
    WootingAnalog_KeycodeType mode = (WootingAnalog_KeycodeType)g_keycodeMode.load(std::memory_order_relaxed);
    uint16_t modeCode = HidToModeCode(hidKeycode, mode);
    if (modeCode == 0) return 0.0f;

    if (hidKeycode < 256)
    {
        if (cache.hasRaw.test(hidKeycode))
            return cache.raw[hidKeycode];

        // Per-key read is the primary source. Some SDK/plugin builds can emit
        // noisy partial full-buffer snapshots that cause visible flicker.
        float v = ReadAnalogByCodeWithDeviceFallback(modeCode, hidKeycode);
        if (v < 0.0f)
        {
            int err = (int)std::lround(v);
            ULONGLONG now = GetTickCount64();
            int prev = g_lastAnalogErrorCode.load(std::memory_order_relaxed);
            ULONGLONG prevMs = g_lastAnalogErrorLogMs.load(std::memory_order_relaxed);
            if (err != prev || now - prevMs >= 5000)
            {
                DebugLog_Write(
                    L"[backend.analog] read_analog hid=%u code=%u mode=%s err=%d",
                    (unsigned)hidKeycode,
                    (unsigned)modeCode,
                    KeycodeModeName((int)mode),
                    err);
                g_lastAnalogErrorCode.store(err, std::memory_order_relaxed);
                g_lastAnalogErrorLogMs.store(now, std::memory_order_relaxed);
            }
        }

        // Use full-buffer only as a high-confidence assist in HID mode.
        if (kEnableFullBufferAssist &&
            cache.hasFullBuffer &&
            mode == WootingAnalog_KeycodeType_HID &&
            cache.fullPresent.test(hidKeycode))
        {
            float vf = cache.fullRaw[hidKeycode];
            if (std::isfinite(vf))
            {
                vf = Clamp01(vf);
                // Ignore low-amplitude full-buffer noise floor (~0.05-0.10).
                if (vf >= 0.20f && vf > v + 0.12f)
                    v = vf;
            }
        }

        if (!std::isfinite(v)) v = 0.0f;
        v = Clamp01(v);

        // If SDK path provides only zeros, fall back to digital key state emulation.
        // This keeps HallJoy usable on systems where analog stream is unavailable.
        if (allowFallback && v <= 0.001f)
        {
            float sim = ReadDigitalFallback01(hidKeycode);
            if (sim > v)
            {
                v = sim;
                if (v >= 0.05f)
                    g_digitalFallbackWarnPending.store(true, std::memory_order_release);
            }
        }

        cache.raw[hidKeycode] = v;
        cache.hasRaw.set(hidKeycode);
        return v;
    }

    // HID>=256: no caching needed in this project (UI tracks <256 anyway)
    float v = ReadAnalogByCodeWithDeviceFallback(modeCode, hidKeycode);
    if (v < 0.0f)
    {
        int err = (int)std::lround(v);
        ULONGLONG now = GetTickCount64();
        int prev = g_lastAnalogErrorCode.load(std::memory_order_relaxed);
        ULONGLONG prevMs = g_lastAnalogErrorLogMs.load(std::memory_order_relaxed);
        if (err != prev || now - prevMs >= 5000)
        {
            DebugLog_Write(
                L"[backend.analog] read_analog hid=%u code=%u mode=%s err=%d",
                (unsigned)hidKeycode,
                (unsigned)modeCode,
                KeycodeModeName((int)mode),
                err);
            g_lastAnalogErrorCode.store(err, std::memory_order_relaxed);
            g_lastAnalogErrorLogMs.store(now, std::memory_order_relaxed);
        }
    }
    if (!std::isfinite(v)) v = 0.0f;
    v = Clamp01(v);
    if (allowFallback && v <= 0.001f)
    {
        float sim = ReadDigitalFallback01(hidKeycode);
        if (sim > v)
        {
            v = sim;
            if (v >= 0.05f)
                g_digitalFallbackWarnPending.store(true, std::memory_order_release);
        }
    }
    return v;
}

static float ReadFiltered01Cached(uint16_t hidKeycode, HidCache& cache)
{
    if (hidKeycode == 0) return 0.0f;
    if (MouseBind_IsPseudoHid(hidKeycode))
        return ReadRaw01Cached(hidKeycode, cache);

    if (hidKeycode < 256)
    {
        if (cache.hasFiltered.test(hidKeycode))
            return cache.filtered[hidKeycode];

        float raw = ReadRaw01Cached(hidKeycode, cache);
        float filtered = ApplyCurveByHid(hidKeycode, raw);

        cache.filtered[hidKeycode] = filtered;
        cache.hasFiltered.set(hidKeycode);
        return filtered;
    }

    float raw = ReadRaw01Cached(hidKeycode, cache);
    return ApplyCurveByHid(hidKeycode, raw);
}

static SHORT StickFromMinus1Plus1(float x)
{
    x = std::clamp(x, -1.0f, 1.0f);
    return (SHORT)std::lround(x * 32767.0f);
}

static uint8_t TriggerByte01(float v01)
{
    v01 = std::clamp(v01, 0.0f, 1.0f);
    return (uint8_t)std::lround(v01 * 255.0f);
}

static bool Pressed(float v01)
{
    // Simple logic threshold after curve applied
    return v01 >= 0.10f;
}

// ---- Snappy Joystick (SOCD-like) state (backend thread) ----
// One state per axis (LX,LY,RX,RY)
static std::array<std::array<uint8_t, 4>, kMaxVirtualPads> g_snappyPrevMinusDown{};
static std::array<std::array<uint8_t, 4>, kMaxVirtualPads> g_snappyPrevPlusDown{};
static std::array<std::array<int8_t, 4>, kMaxVirtualPads>  g_snappyLastDir{}; // -1 = minus, +1 = plus, 0 = unknown
static std::array<std::array<float, 4>, kMaxVirtualPads>   g_snappyMinusValley{};
static std::array<std::array<float, 4>, kMaxVirtualPads>   g_snappyPlusValley{};

static int AxisIndexSafe(Axis a)
{
    switch (a)
    {
    case Axis::LX: return 0;
    case Axis::LY: return 1;
    case Axis::RX: return 2;
    case Axis::RY: return 3;
    default:       return -1;
    }
}

static float AxisValue_WithConflictModes(int padIndex, Axis a, float minusV, float plusV)
{
    const bool snapStick = Settings_GetSnappyJoystick();
    const bool lastKeyPriority = Settings_GetLastKeyPriority();
    if (!snapStick && !lastKeyPriority)
        return plusV - minusV;

    int idx = AxisIndexSafe(a);
    if (idx < 0 || idx >= 4)
        return plusV - minusV;

    // detect "press" edges using the same semantics as buttons (stable threshold)
    bool minusDown = Pressed(minusV);
    bool plusDown = Pressed(plusV);

    int p = std::clamp(padIndex, 0, kMaxVirtualPads - 1);
    bool prevMinus = (g_snappyPrevMinusDown[(size_t)p][idx] != 0);
    bool prevPlus = (g_snappyPrevPlusDown[(size_t)p][idx] != 0);

    if (minusDown && !prevMinus) g_snappyLastDir[(size_t)p][idx] = -1;
    if (plusDown && !prevPlus)  g_snappyLastDir[(size_t)p][idx] = +1;

    if (lastKeyPriority)
    {
        // Re-trigger threshold for analog "re-press" while key is still logically down.
        // Example: user slightly releases key and presses again without crossing Pressed() threshold.
        const float repDelta = std::clamp(Settings_GetLastKeyPrioritySensitivity(), 0.02f, 0.95f);

        if (!minusDown)
        {
            g_snappyMinusValley[(size_t)p][idx] = 1.0f;
        }
        else if (!prevMinus)
        {
            g_snappyMinusValley[(size_t)p][idx] = minusV;
        }
        else
        {
            float& valley = g_snappyMinusValley[(size_t)p][idx];
            valley = std::min(valley, minusV);
            if ((minusV - valley) >= repDelta)
            {
                g_snappyLastDir[(size_t)p][idx] = -1;
                valley = minusV;
            }
        }

        if (!plusDown)
        {
            g_snappyPlusValley[(size_t)p][idx] = 1.0f;
        }
        else if (!prevPlus)
        {
            g_snappyPlusValley[(size_t)p][idx] = plusV;
        }
        else
        {
            float& valley = g_snappyPlusValley[(size_t)p][idx];
            valley = std::min(valley, plusV);
            if ((plusV - valley) >= repDelta)
            {
                g_snappyLastDir[(size_t)p][idx] = +1;
                valley = plusV;
            }
        }
    }

    g_snappyPrevMinusDown[(size_t)p][idx] = minusDown ? 1u : 0u;
    g_snappyPrevPlusDown[(size_t)p][idx] = plusDown ? 1u : 0u;

    float maxV = std::max(minusV, plusV);
    if (maxV <= 0.0001f)
        return 0.0f;

    if (lastKeyPriority)
    {
        // While only one side is logically pressed, keep output fully bound to that side.
        // This prevents partial cancellation when the opposite side starts moving but
        // has not crossed the press threshold yet.
        if (minusDown && !plusDown) return -minusV;
        if (plusDown && !minusDown) return +plusV;
    }

    // Last Key Priority: when both directions are down, most recent press wins.
    if (lastKeyPriority && minusDown && plusDown)
    {
        int8_t dir = g_snappyLastDir[(size_t)p][idx];
        if (dir == 0)
            dir = (plusV >= minusV) ? +1 : -1;

        float mag = 0.0f;
        if (snapStick)
        {
            // Keep "snap" punch while still honoring last pressed direction.
            mag = maxV;
        }
        else
        {
            mag = (dir > 0) ? plusV : minusV;
        }
        return (dir > 0) ? +mag : -mag;
    }

    // Snap Stick behavior: stronger side wins; if equal, last direction wins.
    if (snapStick)
    {
        constexpr float EQ_EPS = 0.002f; // tolerant equality (float noise)
        float d = plusV - minusV;

        if (std::fabs(d) > EQ_EPS)
            return (d > 0.0f) ? +maxV : -maxV;

        if (g_snappyLastDir[(size_t)p][idx] > 0) return +maxV;
        if (g_snappyLastDir[(size_t)p][idx] < 0) return -maxV;
        return 0.0f;
    }

    return plusV - minusV;
}

static void SetBtn(XUSB_REPORT& report, WORD mask, bool down)
{
    if (down) report.wButtons |= mask;
    else      report.wButtons &= ~mask;
}

static float MouseErrorToAxis(double err, float radius, float aggressiveness)
{
    if (radius <= 0.0001f) return 0.0f;
    double n = err / (double)radius;
    n *= (double)std::clamp(aggressiveness, 0.2f, 3.0f);
    // tanh gives smooth response around center with soft saturation on large offsets.
    float out = (float)std::tanh(n);
    if (std::fabs(out) < 0.0025f)
        out = 0.0f;
    return std::clamp(out, -1.0f, 1.0f);
}

static void ApplyMouseCardinalAssist(float& x, float& y)
{
    float ax = std::fabs(x);
    float ay = std::fabs(y);
    float major = std::max(ax, ay);
    float minor = std::min(ax, ay);
    if (major < 0.22f || minor <= 0.0001f)
        return;

    // Keep diagonals available near center, but on strong flicks prefer
    // cardinal directions (X/Y) and suppress the weak orthogonal axis.
    float edge = std::clamp((major - 0.35f) / 0.65f, 0.0f, 1.0f);
    float dominance = std::clamp((major - minor) / (major + 0.0001f), 0.0f, 1.0f);
    float strength = edge * dominance;

    // Near full tilt with clearly dominant axis: aggressively kill minor axis.
    if (major > 0.90f && minor < 0.24f)
        strength = std::max(strength, 0.95f);

    float minorScale = std::clamp(1.0f - 0.92f * strength, 0.04f, 1.0f);
    if (ax >= ay)
        y *= minorScale;
    else
        x *= minorScale;
}

static void AlignMouseOutputDirection(float targetX, float targetY, float& outX, float& outY)
{
    float tMajor = std::max(std::fabs(targetX), std::fabs(targetY));
    float oMajor = std::max(std::fabs(outX), std::fabs(outY));
    if (tMajor < 0.0001f || oMajor < 0.0001f)
        return;

    float tx = targetX / tMajor;
    float ty = targetY / tMajor;
    float ox = outX / oMajor;
    float oy = outY / oMajor;

    float edge = std::clamp((tMajor - 0.30f) / 0.70f, 0.0f, 1.0f);
    if (edge <= 0.0f)
        return;

    // Bias filtered output direction toward live target direction on strong motion.
    float mix = 0.18f + 0.62f * edge;
    float nx = ox + (tx - ox) * mix;
    float ny = oy + (ty - oy) * mix;
    float nMajor = std::max(std::fabs(nx), std::fabs(ny));
    if (nMajor <= 0.0001f)
        return;

    outX = (nx / nMajor) * oMajor;
    outY = (ny / nMajor) * oMajor;
}

static bool ReadMouseStickSample(float& outX, float& outY)
{
    outX = 0.0f;
    outY = 0.0f;

    if (!Settings_GetMouseToStickEnabled())
    {
        g_mouseHasLastPos = false;
        g_mouseSawRawInput.store(false, std::memory_order_relaxed);
        g_mouseFilteredX = 0.0f;
        g_mouseFilteredY = 0.0f;
        g_mouseTargetX = 0.0;
        g_mouseTargetY = 0.0;
        g_mouseFollowerX = 0.0;
        g_mouseFollowerY = 0.0;
        g_mouseLastTickMs = 0;
        g_mouseRawAccumDx.store(0, std::memory_order_relaxed);
        g_mouseRawAccumDy.store(0, std::memory_order_relaxed);
        g_mouseDbgEnabled.store(0, std::memory_order_relaxed);
        g_mouseDbgUsingRaw.store(0, std::memory_order_relaxed);
        g_mouseDbgTargetX10.store(0, std::memory_order_relaxed);
        g_mouseDbgTargetY10.store(0, std::memory_order_relaxed);
        g_mouseDbgFollowerX10.store(0, std::memory_order_relaxed);
        g_mouseDbgFollowerY10.store(0, std::memory_order_relaxed);
        g_mouseDbgOutX1000.store(0, std::memory_order_relaxed);
        g_mouseDbgOutY1000.store(0, std::memory_order_relaxed);
        g_mouseDbgRadius1000.store(1000, std::memory_order_relaxed);
        return false;
    }

    ULONGLONG nowMs = GetTickCount64();

    if (g_mouseLastTickMs == 0)
        g_mouseLastTickMs = nowMs;

    ULONGLONG dtRaw = (g_mouseLastTickMs > 0 && nowMs > g_mouseLastTickMs) ? (nowMs - g_mouseLastTickMs) : 1ull;
    g_mouseLastTickMs = nowMs;
    float dtMs = (float)std::clamp<ULONGLONG>(dtRaw, 1, 25);

    int rawDx = g_mouseRawAccumDx.exchange(0, std::memory_order_acq_rel);
    int rawDy = g_mouseRawAccumDy.exchange(0, std::memory_order_acq_rel);
    if (rawDx != 0 || rawDy != 0)
        g_mouseSawRawInput.store(true, std::memory_order_relaxed);

    LONG dx = (LONG)rawDx;
    LONG dy = (LONG)rawDy;

    // Fallback to cursor delta only when raw input has not been seen yet.
    if (!g_mouseSawRawInput.load(std::memory_order_relaxed))
    {
        POINT pt{};
        if (GetCursorPos(&pt))
        {
            if (!g_mouseHasLastPos)
            {
                g_mouseLastPos = pt;
                g_mouseHasLastPos = true;
            }
            else
            {
                dx = (LONG)(pt.x - g_mouseLastPos.x);
                dy = (LONG)(pt.y - g_mouseLastPos.y);
                g_mouseLastPos = pt;
            }
        }
    }

    const float sens = std::clamp(Settings_GetMouseToStickSensitivity(), 0.1f, 8.0f);
    const float aggressiveness = std::clamp(Settings_GetMouseToStickAggressiveness(), 0.2f, 3.0f);
    const float maxOffsetMul = std::clamp(Settings_GetMouseToStickMaxOffset(), 0.0f, 6.0f);
    const float followSpeedMul = std::clamp(Settings_GetMouseToStickFollowSpeed(), 0.2f, 3.0f);

    // Base mouse-space unit: how many raw counts are needed for "1.0" of virtual range.
    const float baseRange = std::clamp(92.0f / sens, 10.0f, 260.0f);
    // Slider-controlled max allowed virtual displacement from center.
    const double offsetLimit = (double)baseRange * (double)maxOffsetMul;

    auto smoothAxis = [dtMs](float current, float target) -> float
    {
        // Output smoothing is independent from offset and follower speed.
        const float tauMs = 5.0f;
        float alpha = 1.0f - std::exp(-dtMs / std::max(0.5f, tauMs));
        float v = current + (target - current) * alpha;
        if (std::fabs(v) < 0.0006f && std::fabs(target) < 0.0006f) v = 0.0f;
        return std::clamp(v, -1.0f, 1.0f);
    };

    if (offsetLimit <= 0.0001)
    {
        // Max offset = 0 means disabled movement envelope.
        g_mouseTargetX = 0.0;
        g_mouseTargetY = 0.0;
        g_mouseFollowerX = 0.0;
        g_mouseFollowerY = 0.0;
        g_mouseFilteredX = 0.0f;
        g_mouseFilteredY = 0.0f;
        outX = 0.0f;
        outY = 0.0f;
        g_mouseDbgEnabled.store(1, std::memory_order_relaxed);
        g_mouseDbgUsingRaw.store(g_mouseSawRawInput.load(std::memory_order_relaxed) ? 1u : 0u, std::memory_order_relaxed);
        g_mouseDbgTargetX10.store(0, std::memory_order_relaxed);
        g_mouseDbgTargetY10.store(0, std::memory_order_relaxed);
        g_mouseDbgFollowerX10.store(0, std::memory_order_relaxed);
        g_mouseDbgFollowerY10.store(0, std::memory_order_relaxed);
        g_mouseDbgOutX1000.store(0, std::memory_order_relaxed);
        g_mouseDbgOutY1000.store(0, std::memory_order_relaxed);
        g_mouseDbgRadius1000.store((int)std::lround((double)baseRange * 1000.0), std::memory_order_relaxed);
        return false;
    }

    // In this model g_mouseTarget is the live offset between virtual cursor and
    // virtual center ("zero point"), not an absolute world position.
    // This prevents hard one-way lock at map borders.
    // First, let center catch up (offset decays toward zero)...
    const double invOffset = 1.0 / offsetLimit;
    double errNormX = std::clamp(g_mouseTargetX * invOffset, -1.0, 1.0);
    double errNormY = std::clamp(g_mouseTargetY * invOffset, -1.0, 1.0);

    // Follow speed in normalized units per millisecond.
    const float followNormPerMs = std::clamp(0.018f * followSpeedMul, 0.0015f, 0.12f);
    auto moveTowardNorm2D = [dtMs, followNormPerMs](double& cx, double& cy, double tx, double ty)
    {
        double dxv = tx - cx;
        double dyv = ty - cy;
        double dist = std::sqrt(dxv * dxv + dyv * dyv);
        double maxStep = (double)followNormPerMs * (double)dtMs;
        if (dist <= 0.000001 || maxStep <= 0.0)
            return;
        if (dist <= maxStep)
        {
            cx = tx;
            cy = ty;
            return;
        }
        double s = maxStep / dist;
        cx += dxv * s;
        cy += dyv * s;
    };
    moveTowardNorm2D(errNormX, errNormY, 0.0, 0.0);

    // ...then apply this tick mouse movement.
    errNormX += (double)dx * invOffset;
    errNormY += (double)(-dy) * invOffset; // Y up like stick

    // Limit only the offset (difference), not absolute motion space.
    errNormX = std::clamp(errNormX, -1.0, 1.0);
    errNormY = std::clamp(errNormY, -1.0, 1.0);
    g_mouseTargetX = errNormX * offsetLimit;
    g_mouseTargetY = errNormY * offsetLimit;

    // Follower stays at center in this representation; offset itself is the error.
    g_mouseFollowerX = 0.0;
    g_mouseFollowerY = 0.0;

    // Snap tiny residuals to zero when fully idle.
    if (rawDx == 0 && rawDy == 0 && std::fabs(errNormX) < 0.003 && std::fabs(errNormY) < 0.003)
    {
        g_mouseTargetX = 0.0;
        g_mouseTargetY = 0.0;
        errNormX = 0.0;
        errNormY = 0.0;
    }

    float targetX = MouseErrorToAxis(errNormX, 1.0f, aggressiveness);
    float targetY = MouseErrorToAxis(errNormY, 1.0f, aggressiveness);
    ApplyMouseCardinalAssist(targetX, targetY);

    // 4) Smooth output and align its direction toward the live target direction.
    g_mouseFilteredX = smoothAxis(g_mouseFilteredX, targetX);
    g_mouseFilteredY = smoothAxis(g_mouseFilteredY, targetY);
    ApplyMouseCardinalAssist(g_mouseFilteredX, g_mouseFilteredY);
    AlignMouseOutputDirection(targetX, targetY, g_mouseFilteredX, g_mouseFilteredY);

    // Square stick space (independent axes): do NOT renormalize to a circle.
    // This keeps full X/Y output even when the other axis is slightly non-zero.
    g_mouseFilteredX = std::clamp(g_mouseFilteredX, -1.0f, 1.0f);
    g_mouseFilteredY = std::clamp(g_mouseFilteredY, -1.0f, 1.0f);

    outX = g_mouseFilteredX;
    outY = g_mouseFilteredY;
    g_mouseDbgEnabled.store(1, std::memory_order_relaxed);
    g_mouseDbgUsingRaw.store(g_mouseSawRawInput.load(std::memory_order_relaxed) ? 1u : 0u, std::memory_order_relaxed);
    g_mouseDbgTargetX10.store((int)std::lround(std::clamp(g_mouseTargetX, -200000.0, 200000.0) * 10.0), std::memory_order_relaxed);
    g_mouseDbgTargetY10.store((int)std::lround(std::clamp(g_mouseTargetY, -200000.0, 200000.0) * 10.0), std::memory_order_relaxed);
    g_mouseDbgFollowerX10.store((int)std::lround(std::clamp(g_mouseFollowerX, -200000.0, 200000.0) * 10.0), std::memory_order_relaxed);
    g_mouseDbgFollowerY10.store((int)std::lround(std::clamp(g_mouseFollowerY, -200000.0, 200000.0) * 10.0), std::memory_order_relaxed);
    g_mouseDbgOutX1000.store((int)std::lround(std::clamp((double)outX, -1.0, 1.0) * 1000.0), std::memory_order_relaxed);
    g_mouseDbgOutY1000.store((int)std::lround(std::clamp((double)outY, -1.0, 1.0) * 1000.0), std::memory_order_relaxed);
    g_mouseDbgRadius1000.store((int)std::lround((double)offsetLimit * 1000.0), std::memory_order_relaxed);
    return (std::fabs(outX) > 0.0001f || std::fabs(outY) > 0.0001f);
}

static SHORT MergeStickAxis(SHORT baseAxis, float mouseAxis01)
{
    SHORT mouse = StickFromMinus1Plus1(mouseAxis01);
    if (mouse == 0)
        return baseAxis;
    // Mouse should feel immediate, but keep keyboard if stronger on this tick.
    return (std::abs((int)mouse) >= std::abs((int)baseAxis)) ? mouse : baseAxis;
}

static bool BtnPressedFromMask(int padIndex, GameButton b, HidCache& cache)
{
    for (int chunk = 0; chunk < 4; ++chunk)
    {
        uint64_t bits = Bindings_GetButtonMaskChunkForPad(padIndex, b, chunk);
        if (!bits) continue;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        while (bits) {
            unsigned long idx = 0;
            _BitScanForward64(&idx, bits);
            bits &= (bits - 1);
            uint16_t hid = (uint16_t)(chunk * 64 + (int)idx);
            float v01 = ReadFiltered01Cached(hid, cache);
            if (Pressed(v01)) return true;
        }
#else
        for (int bit = 0; bit < 64; ++bit) {
            if (bits & (1ULL << bit)) {
                uint16_t hid = (uint16_t)(chunk * 64 + bit);
                float v01 = ReadFiltered01Cached(hid, cache);
                if (Pressed(v01)) return true;
            }
        }
#endif
    }
    return false;
}

static XUSB_REPORT BuildReportForPad(int padIndex, HidCache& cache)
{
    XUSB_REPORT report{};
    report.wButtons = 0;

    auto applyAxis = [&](Axis a, SHORT& out) {
        AxisBinding b = Bindings_GetAxisForPad(padIndex, a);
        float minusV = ReadFiltered01Cached(b.minusHid, cache);
        float plusV = ReadFiltered01Cached(b.plusHid, cache);
        out = StickFromMinus1Plus1(AxisValue_WithConflictModes(padIndex, a, minusV, plusV));
        };

    applyAxis(Axis::LX, report.sThumbLX);
    applyAxis(Axis::LY, report.sThumbLY);
    applyAxis(Axis::RX, report.sThumbRX);
    applyAxis(Axis::RY, report.sThumbRY);

    if (padIndex == 0 && Settings_GetMouseToStickEnabled())
    {
        float mx = 0.0f, my = 0.0f;
        if (ReadMouseStickSample(mx, my))
        {
            if (Settings_GetMouseToStickTarget() == 0)
            {
                report.sThumbLX = MergeStickAxis(report.sThumbLX, mx);
                report.sThumbLY = MergeStickAxis(report.sThumbLY, my);
            }
            else
            {
                report.sThumbRX = MergeStickAxis(report.sThumbRX, mx);
                report.sThumbRY = MergeStickAxis(report.sThumbRY, my);
            }
        }
    }

    report.bLeftTrigger = TriggerByte01(ReadFiltered01Cached(Bindings_GetTriggerForPad(padIndex, Trigger::LT), cache));
    report.bRightTrigger = TriggerByte01(ReadFiltered01Cached(Bindings_GetTriggerForPad(padIndex, Trigger::RT), cache));

    SetBtn(report, XUSB_GAMEPAD_A, BtnPressedFromMask(padIndex, GameButton::A, cache));
    SetBtn(report, XUSB_GAMEPAD_B, BtnPressedFromMask(padIndex, GameButton::B, cache));
    SetBtn(report, XUSB_GAMEPAD_X, BtnPressedFromMask(padIndex, GameButton::X, cache));
    SetBtn(report, XUSB_GAMEPAD_Y, BtnPressedFromMask(padIndex, GameButton::Y, cache));
    SetBtn(report, XUSB_GAMEPAD_LEFT_SHOULDER, BtnPressedFromMask(padIndex, GameButton::LB, cache));
    SetBtn(report, XUSB_GAMEPAD_RIGHT_SHOULDER, BtnPressedFromMask(padIndex, GameButton::RB, cache));
    SetBtn(report, XUSB_GAMEPAD_BACK, BtnPressedFromMask(padIndex, GameButton::Back, cache));
    SetBtn(report, XUSB_GAMEPAD_START, BtnPressedFromMask(padIndex, GameButton::Start, cache));
    SetBtn(report, XUSB_GAMEPAD_GUIDE, BtnPressedFromMask(padIndex, GameButton::Guide, cache));
    SetBtn(report, XUSB_GAMEPAD_LEFT_THUMB, BtnPressedFromMask(padIndex, GameButton::LS, cache));
    SetBtn(report, XUSB_GAMEPAD_RIGHT_THUMB, BtnPressedFromMask(padIndex, GameButton::RS, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_UP, BtnPressedFromMask(padIndex, GameButton::DpadUp, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_DOWN, BtnPressedFromMask(padIndex, GameButton::DpadDown, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_LEFT, BtnPressedFromMask(padIndex, GameButton::DpadLeft, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_RIGHT, BtnPressedFromMask(padIndex, GameButton::DpadRight, cache));

    return report;
}

static bool IsReportSignificantlyDifferent(const XUSB_REPORT& a, const XUSB_REPORT& b)
{
    if (a.wButtons != b.wButtons) return true;
    if (std::abs((int)a.bLeftTrigger - (int)b.bLeftTrigger) >= 2) return true;
    if (std::abs((int)a.bRightTrigger - (int)b.bRightTrigger) >= 2) return true;

    if (std::abs((int)a.sThumbLX - (int)b.sThumbLX) >= 256) return true;
    if (std::abs((int)a.sThumbLY - (int)b.sThumbLY) >= 256) return true;
    if (std::abs((int)a.sThumbRX - (int)b.sThumbRX) >= 256) return true;
    if (std::abs((int)a.sThumbRY - (int)b.sThumbRY) >= 256) return true;

    return false;
}

bool Backend_Init()
{
    DebugLog_Write(L"[backend.init] begin");
    g_tmTrackedMaxRawMilli.store(0, std::memory_order_relaxed);
    g_tmTrackedMaxOutMilli.store(0, std::memory_order_relaxed);
    g_tmFullBufferRet.store(0, std::memory_order_relaxed);
    g_tmFullBufferMaxMilli.store(0, std::memory_order_relaxed);
    g_tmFullBufferDeviceBestRet.store(0, std::memory_order_relaxed);
    g_tmFullBufferDeviceBestMaxMilli.store(0, std::memory_order_relaxed);
    g_virtualPadCount.store(std::clamp(Settings_GetVirtualGamepadCount(), 1, kMaxVirtualPads), std::memory_order_release);
    g_virtualPadsEnabled.store(Settings_GetVirtualGamepadsEnabled(), std::memory_order_release);
    g_lastInitIssues.store(BackendInitIssue_None, std::memory_order_release);
    g_reconnectRequested.store(false, std::memory_order_release);
    g_deviceChangeReconnectRequested.store(false, std::memory_order_release);
    g_ignoreDeviceChangeUntilMs.store(0, std::memory_order_release);
    g_vigemUpdateFailStreak = 0;
    g_zeroProbeStreak.store(0, std::memory_order_relaxed);
    g_autoRecoverTried.store(false, std::memory_order_relaxed);
    g_keycodeModeLocked.store(false, std::memory_order_relaxed);
    g_mouseHasLastPos = false;
    g_mouseSawRawInput.store(false, std::memory_order_relaxed);
    g_mouseFilteredX = 0.0f;
    g_mouseFilteredY = 0.0f;
    g_mouseTargetX = 0.0;
    g_mouseTargetY = 0.0;
    g_mouseFollowerX = 0.0;
    g_mouseFollowerY = 0.0;
    g_mouseLastTickMs = 0;
    g_mouseRawAccumDx.store(0, std::memory_order_relaxed);
    g_mouseRawAccumDy.store(0, std::memory_order_relaxed);
    g_mouseDbgEnabled.store(0, std::memory_order_relaxed);
    g_mouseDbgUsingRaw.store(0, std::memory_order_relaxed);
    g_mouseDbgTargetX10.store(0, std::memory_order_relaxed);
    g_mouseDbgTargetY10.store(0, std::memory_order_relaxed);
    g_mouseDbgFollowerX10.store(0, std::memory_order_relaxed);
    g_mouseDbgFollowerY10.store(0, std::memory_order_relaxed);
    g_mouseDbgOutX1000.store(0, std::memory_order_relaxed);
    g_mouseDbgOutY1000.store(0, std::memory_order_relaxed);
    g_mouseDbgRadius1000.store(1000, std::memory_order_relaxed);
    for (auto& b : g_mouseBindButtons) b.store(0, std::memory_order_relaxed);
    g_mouseWheelPulseUpUntilMs.store(0, std::memory_order_relaxed);
    g_mouseWheelPulseDownUntilMs.store(0, std::memory_order_relaxed);

    uint32_t initIssues = BackendInitIssue_None;
    int wootingInit = wooting_analog_initialise();
    DebugLog_Write(L"[backend.init] wooting_analog_initialise ret=%d", wootingInit);
    if (wootingInit >= 0)
    {
        SetKeycodeModeWithLog(WootingAnalog_KeycodeType_HID, L"init", 0);
    }
    LogWootingStateSnapshot(L"after_init_call");
    LogConnectedDevicesDetailed(L"after_init_call");
    if (wootingInit < 0)
    {
        switch ((WootingAnalogResult)wootingInit)
        {
        case WootingAnalogResult_DLLNotFound:
        case WootingAnalogResult_FunctionNotFound:
            initIssues |= BackendInitIssue_WootingSdkMissing;
            break;
        case WootingAnalogResult_NoPlugins:
            initIssues |= BackendInitIssue_WootingNoPlugins;
            break;
        case WootingAnalogResult_IncompatibleVersion:
            initIssues |= BackendInitIssue_WootingIncompatible;
            break;
        default:
            initIssues |= BackendInitIssue_Unknown;
            break;
        }
    }

    if (g_virtualPadsEnabled.load(std::memory_order_acquire))
    {
        // Initial virtual pad creation may also broadcast device changes.
        g_ignoreDeviceChangeUntilMs.store(GetTickCount64() + 1500, std::memory_order_release);
        VIGEM_ERROR err = VIGEM_ERROR_NONE;
        if (!Vigem_Create(g_virtualPadCount.load(std::memory_order_acquire), &err)) {
            DebugLog_Write(L"[backend.init] Vigem_Create failed err=%d", (int)err);
            g_vigemOk.store(false, std::memory_order_release);
            g_vigemLastErr.store(err, std::memory_order_release);
            if (err == VIGEM_ERROR_BUS_NOT_FOUND)
                initIssues |= BackendInitIssue_VigemBusMissing;
            else
                initIssues |= BackendInitIssue_Unknown;
        }
        else
        {
            DebugLog_Write(L"[backend.init] Vigem_Create ok pads=%d", g_virtualPadCount.load(std::memory_order_acquire));
            g_vigemOk.store(true, std::memory_order_release);
            g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
        }
    }
    else
    {
        g_vigemOk.store(true, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
    }

    if (initIssues != BackendInitIssue_None)
    {
        DebugLog_Write(L"[backend.init] fail issues=0x%08X", initIssues);
        g_lastInitIssues.store(initIssues, std::memory_order_release);
        Vigem_Destroy();
        wooting_analog_uninitialise();
        return false;
    }

    for (auto& a : g_uiAnalogM) a.store(0, std::memory_order_relaxed);
    for (auto& a : g_uiRawM)    a.store(0, std::memory_order_relaxed);
    for (auto& d : g_uiDirty)   d.store(0, std::memory_order_relaxed);
    for (int i = 0; i < kMaxVirtualPads; ++i)
    {
        g_lastSentValid[(size_t)i] = 0;
        g_lastSentTicks[(size_t)i] = 0;
        g_lastSentReports[(size_t)i] = XUSB_REPORT{};
    }

    DebugLog_Write(L"[backend.init] success");
    return true;
}

void Backend_Shutdown()
{
    DebugLog_Write(L"[backend] shutdown");
    g_knownDeviceCount.store(0, std::memory_order_relaxed);
    g_mouseHasLastPos = false;
    g_mouseSawRawInput.store(false, std::memory_order_relaxed);
    g_mouseFilteredX = 0.0f;
    g_mouseFilteredY = 0.0f;
    g_mouseTargetX = 0.0;
    g_mouseTargetY = 0.0;
    g_mouseFollowerX = 0.0;
    g_mouseFollowerY = 0.0;
    g_mouseLastTickMs = 0;
    g_mouseRawAccumDx.store(0, std::memory_order_relaxed);
    g_mouseRawAccumDy.store(0, std::memory_order_relaxed);
    g_mouseDbgEnabled.store(0, std::memory_order_relaxed);
    g_mouseDbgUsingRaw.store(0, std::memory_order_relaxed);
    g_mouseDbgTargetX10.store(0, std::memory_order_relaxed);
    g_mouseDbgTargetY10.store(0, std::memory_order_relaxed);
    g_mouseDbgFollowerX10.store(0, std::memory_order_relaxed);
    g_mouseDbgFollowerY10.store(0, std::memory_order_relaxed);
    g_mouseDbgOutX1000.store(0, std::memory_order_relaxed);
    g_mouseDbgOutY1000.store(0, std::memory_order_relaxed);
    g_mouseDbgRadius1000.store(1000, std::memory_order_relaxed);
    for (auto& b : g_mouseBindButtons) b.store(0, std::memory_order_relaxed);
    g_mouseWheelPulseUpUntilMs.store(0, std::memory_order_relaxed);
    g_mouseWheelPulseDownUntilMs.store(0, std::memory_order_relaxed);
    g_reconnectRequested.store(false, std::memory_order_release);
    g_deviceChangeReconnectRequested.store(false, std::memory_order_release);
    g_keycodeModeLocked.store(false, std::memory_order_relaxed);
    g_vigemUpdateFailStreak = 0;
    Vigem_Destroy();
    wooting_analog_uninitialise();
}

void Backend_Tick()
{
    ULONGLONG nowMs = GetTickCount64();
    ULONGLONG lastStateLog = g_lastWootingStateLogMs.load(std::memory_order_relaxed);
    if (nowMs - lastStateLog >= 10000)
    {
        g_lastWootingStateLogMs.store(nowMs, std::memory_order_relaxed);
        LogWootingStateSnapshot(L"tick_heartbeat");
    }

    if (g_reconnectRequested.exchange(false, std::memory_order_acq_rel))
    {
        DebugLog_Write(L"[backend.tick] reconnect requested (force)");
        g_deviceChangeReconnectRequested.store(false, std::memory_order_release);
        Vigem_ReconnectThrottled(true);
    }
    else if (g_deviceChangeReconnectRequested.exchange(false, std::memory_order_acq_rel))
    {
        DebugLog_Write(L"[backend.tick] reconnect requested (device change)");
        Vigem_ReconnectThrottled(false);
    }

    HidCache cache;
    static uint32_t s_lastHandledKeyEventSeq = 0;

    // Build per-tick raw map from full buffer (most robust on newer SDK/plugin stacks).
    // Works directly for HID mode because returned keycodes are HID identifiers.
    if (kEnableFullBufferAssist)
    {
        WootingAnalog_KeycodeType mode = (WootingAnalog_KeycodeType)g_keycodeMode.load(std::memory_order_relaxed);
        if (mode == WootingAnalog_KeycodeType_HID)
        {
            unsigned short codes[128]{};
            float vals[128]{};
            int ret = wooting_analog_read_full_buffer(codes, vals, (unsigned)_countof(codes));
            if (ret >= 0)
            {
                int n = std::min(ret, (int)_countof(codes));
                cache.hasFullBuffer = true;
                for (int i = 0; i < n; ++i)
                {
                    unsigned short code = codes[i];
                    if (code >= 256) continue;
                    float v = vals[i];
                    if (!std::isfinite(v)) continue;
                    v = Clamp01(v);
                    cache.fullPresent.set(code);
                    if (v > cache.fullRaw[code])
                        cache.fullRaw[code] = v;
                }

                // Merge per-device buffers too, taking max value per HID.
                int ndev = std::clamp(g_knownDeviceCount.load(std::memory_order_relaxed), 0, (int)g_knownDeviceIds.size());
                for (int di = 0; di < ndev; ++di)
                {
                    unsigned short dcodes[128]{};
                    float dvals[128]{};
                    int dret = wooting_analog_read_full_buffer_device(dcodes, dvals, (unsigned)_countof(dcodes), g_knownDeviceIds[di]);
                    if (dret < 0) continue;
                    int dn = std::min(dret, (int)_countof(dcodes));
                    for (int i = 0; i < dn; ++i)
                    {
                        unsigned short code = dcodes[i];
                        if (code >= 256) continue;
                        float v = dvals[i];
                        if (!std::isfinite(v)) continue;
                        v = Clamp01(v);
                        cache.fullPresent.set(code);
                        if (v > cache.fullRaw[code])
                            cache.fullRaw[code] = v;
                    }
                }
            }
        }
    }

    int cnt = g_trackedCount.load(std::memory_order_acquire);
    cnt = std::clamp(cnt, 0, 256);
    uint16_t maxRawM = 0;
    uint16_t maxOutM = 0;
    uint16_t maxRawHid = 0;
    uint16_t maxOutHid = 0;

    // UI snapshot update
    for (int i = 0; i < cnt; ++i)
    {
        uint16_t hid = g_trackedList[i];
        if (hid == 0 || hid >= 256) continue;

        float raw = ReadRaw01Cached(hid, cache);
        float filtered = ReadFiltered01Cached(hid, cache);

        int rawM = (int)std::lround(raw * 1000.0f);
        rawM = std::clamp(rawM, 0, 1000);
        g_uiRawM[hid].store((uint16_t)rawM, std::memory_order_relaxed);
        if ((uint16_t)rawM >= maxRawM)
        {
            maxRawM = (uint16_t)rawM;
            maxRawHid = hid;
        }

        int outM = (int)std::lround(filtered * 1000.0f);
        outM = std::clamp(outM, 0, 1000);
        if ((uint16_t)outM >= maxOutM)
        {
            maxOutM = (uint16_t)outM;
            maxOutHid = hid;
        }

        uint16_t newV = (uint16_t)outM;
        uint16_t oldV = g_uiAnalogM[hid].load(std::memory_order_relaxed);
        if (oldV != newV) {
            int diff = std::abs((int)newV - (int)oldV);
            bool edge = (oldV == 0 || newV == 0 || oldV == 1000 || newV == 1000);
            if (diff >= 2 || edge)
            {
                g_uiAnalogM[hid].store(newV, std::memory_order_relaxed);
                int chunk = hid / 64;
                int bit = hid % 64;
                g_uiDirty[chunk].fetch_or(1ULL << bit, std::memory_order_relaxed);
            }
        }
    }
    g_tmTrackedMaxRawMilli.store(maxRawM, std::memory_order_relaxed);
    g_tmTrackedMaxOutMilli.store(maxOutM, std::memory_order_relaxed);
    ULONGLONG lastInputLog = g_lastInputStateLogMs.load(std::memory_order_relaxed);
    if (nowMs - lastInputLog >= 2000)
    {
        g_lastInputStateLogMs.store(nowMs, std::memory_order_relaxed);
        DebugLog_Write(
            L"[backend.input] tracked=%d max_raw=%u(hid=%u) max_out=%u(hid=%u)",
            cnt,
            (unsigned)maxRawM, (unsigned)maxRawHid,
            (unsigned)maxOutM, (unsigned)maxOutHid);
    }
    ULONGLONG lastFullLog = g_lastFullBufferLogMs.load(std::memory_order_relaxed);
    if (nowMs - lastFullLog >= 2000)
    {
        g_lastFullBufferLogMs.store(nowMs, std::memory_order_relaxed);
        LogFullBufferSnapshot(L"periodic");
    }

    // Probe keycode mode only via per-key event reads; full-buffer based probing
    // is intentionally avoided because some SDK/plugin versions expose noisy,
    // non-key-specific full-buffer activity.
    uint32_t keySeq = g_keyboardEventSeq.load(std::memory_order_acquire);
    if (kEnableAdaptiveKeycodeModeProbe && keySeq != s_lastHandledKeyEventSeq)
    {
        s_lastHandledKeyEventSeq = keySeq;
        uint16_t hidHint = g_keyboardEventHid.load(std::memory_order_relaxed);
        uint16_t scanHint = g_keyboardEventScan.load(std::memory_order_relaxed);
        uint16_t vkHint = g_keyboardEventVk.load(std::memory_order_relaxed);
        float probe = 0.0f;
        if (hidHint != 0)
            probe = ReadRaw01Cached(hidHint, cache);

        DebugLog_Write(
            L"[backend.mode] key_event seq=%u hid=%u scan=%u vk=%u probe=%.3f mode=%s",
            (unsigned)keySeq,
            (unsigned)hidHint,
            (unsigned)scanHint,
            (unsigned)vkHint,
            probe,
            KeycodeModeName(g_keycodeMode.load(std::memory_order_relaxed)));

        ULONGLONG now = GetTickCount64();
        ULONGLONG lastSwitch = g_lastKeycodeSwitchMs.load(std::memory_order_relaxed);
        if (hidHint != 0 && probe > 0.02f)
        {
            g_zeroProbeStreak.store(0, std::memory_order_relaxed);
            g_keycodeModeLocked.store(false, std::memory_order_relaxed);
        }
        else if (hidHint != 0 && probe <= 0.001f && now - lastSwitch >= 120)
        {
            g_zeroProbeStreak.fetch_add(1, std::memory_order_relaxed);
            bool found = AutoProbeKeycodeModeFromEvent(hidHint, scanHint, vkHint);
            if (found)
                LogFullBufferSnapshot(L"after_probe_found");
        }
    }

    // Bind capture: scan all HID 1..255 and capture first edge above threshold.
    if (g_bindCaptureEnabled.load(std::memory_order_acquire))
    {
        uint16_t bestHid = 0;
        int bestRawM = 0;
        for (uint16_t hid = 1; hid < 256; ++hid)
        {
            float raw = ReadRaw01Cached(hid, cache);
            int rawM = (int)std::lround(raw * 1000.0f);
            if (rawM > bestRawM)
            {
                bestRawM = rawM;
                bestHid = hid;
            }
        }

        bool down = (bestRawM >= 120);
        bool hadDown = g_bindHadDown.load(std::memory_order_relaxed);
        if (down && !hadDown && bestHid != 0)
        {
            uint32_t packed = (uint32_t)(bestHid & 0xFFFFu) | ((uint32_t)(bestRawM & 0xFFFFu) << 16);
            g_bindCapturedPacked.store(packed, std::memory_order_release);
        }
        g_bindHadDown.store(down, std::memory_order_relaxed);
    }
    else
    {
        g_bindHadDown.store(false, std::memory_order_relaxed);
    }

    int logicalPads = std::clamp(g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads);
    for (int pad = 0; pad < logicalPads; ++pad)
    {
        XUSB_REPORT report = BuildReportForPad(pad, cache);
        g_reports[(size_t)pad] = report;

        g_lastRX[(size_t)pad].store(report.sThumbRX, std::memory_order_release);
        g_lastSeq[(size_t)pad].fetch_add(1, std::memory_order_acq_rel);
        g_lastReport[(size_t)pad] = report;
        g_lastSeq[(size_t)pad].fetch_add(1, std::memory_order_release);
    }
    for (int pad = logicalPads; pad < kMaxVirtualPads; ++pad)
    {
        XUSB_REPORT report{};
        g_reports[(size_t)pad] = report;

        g_lastRX[(size_t)pad].store(0, std::memory_order_release);
        g_lastSeq[(size_t)pad].fetch_add(1, std::memory_order_acq_rel);
        g_lastReport[(size_t)pad] = report;
        g_lastSeq[(size_t)pad].fetch_add(1, std::memory_order_release);
    }

    if (g_virtualPadsEnabled.load(std::memory_order_acquire))
    {
        if (g_client && g_connectedPadCount > 0) {
            VIGEM_ERROR err = VIGEM_ERROR_NONE;
            bool allOk = true;
            DWORD now = GetTickCount();
            constexpr DWORD kMinSendIntervalMs = 4;
            constexpr DWORD kKeepAliveMs = 250;

            for (int i = 0; i < g_connectedPadCount; ++i)
            {
                PVIGEM_TARGET pad = g_pads[(size_t)i];
                if (!pad) continue;

                int idx = std::clamp(i, 0, kMaxVirtualPads - 1);
                const XUSB_REPORT& report = g_reports[(size_t)idx];

                bool valid = g_lastSentValid[(size_t)idx] != 0;
                bool changed = !valid || IsReportSignificantlyDifferent(report, g_lastSentReports[(size_t)idx]);
                DWORD elapsed = now - g_lastSentTicks[(size_t)idx];

                if (!changed && elapsed < kKeepAliveMs)
                    continue;
                if (changed && elapsed < kMinSendIntervalMs)
                    continue;

                err = vigem_target_x360_update(g_client, pad, report);
                if (!VIGEM_SUCCESS(err))
                {
                    allOk = false;
                    break;
                }

                g_lastSentReports[(size_t)idx] = report;
                g_lastSentTicks[(size_t)idx] = now;
                g_lastSentValid[(size_t)idx] = 1;
            }

            if (!allOk) {
                DebugLog_Write(L"[backend.tick] vigem update failed err=%d streak=%d", (int)err, g_vigemUpdateFailStreak + 1);
                g_vigemOk.store(false, std::memory_order_release);
                g_vigemLastErr.store(err, std::memory_order_release);
                ++g_vigemUpdateFailStreak;
                if (g_vigemUpdateFailStreak >= 3)
                {
                    g_vigemUpdateFailStreak = 0;
                    Vigem_ReconnectThrottled();
                }
            }
            else {
                g_vigemUpdateFailStreak = 0;
                g_vigemOk.store(true, std::memory_order_release);
                g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
            }
        }
        else {
            DebugLog_Write(L"[backend.tick] no vigem client/targets, reconnect");
            g_vigemUpdateFailStreak = 0;
            g_vigemOk.store(false, std::memory_order_release);
            g_vigemLastErr.store(VIGEM_ERROR_BUS_NOT_FOUND, std::memory_order_release);
            Vigem_ReconnectThrottled();
        }
    }
    else {
        g_vigemUpdateFailStreak = 0;
        if (g_client || g_connectedPadCount > 0)
            Vigem_Destroy();
        g_vigemOk.store(true, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
    }
}

SHORT Backend_GetLastRX() { return g_lastRX[0].load(std::memory_order_acquire); }

XUSB_REPORT Backend_GetLastReport()
{
    return Backend_GetLastReportForPad(0);
}

XUSB_REPORT Backend_GetLastReportForPad(int padIndex)
{
    int p = std::clamp(padIndex, 0, kMaxVirtualPads - 1);
    XUSB_REPORT r{};
    for (;;) {
        uint32_t s1 = g_lastSeq[(size_t)p].load(std::memory_order_acquire);
        if (s1 & 1u) continue;
        r = g_lastReport[(size_t)p];
        uint32_t s2 = g_lastSeq[(size_t)p].load(std::memory_order_acquire);
        if (s1 == s2) return r;
    }
}

void BackendUI_SetTrackedHids(const uint16_t* hids, int count)
{
    if (!hids || count <= 0) { BackendUI_ClearTrackedHids(); return; }
    count = std::clamp(count, 0, 256);

    g_trackedCount.store(0, std::memory_order_release);

    int outN = 0;
    for (int i = 0; i < count && outN < 256; ++i) {
        uint16_t hid = hids[i];
        if (hid == 0 || hid >= 256) continue;
        g_trackedList[outN++] = hid;
    }

    g_trackedCount.store(outN, std::memory_order_release);
    wchar_t sample[256]{};
    size_t used = 0;
    int sampleN = std::min(outN, 12);
    for (int i = 0; i < sampleN; ++i)
    {
        wchar_t t[16]{};
        _snwprintf_s(t, _countof(t), _TRUNCATE, (i == 0) ? L"%u" : L",%u", (unsigned)g_trackedList[i]);
        size_t left = _countof(sample) - 1 - used;
        if (left == 0) break;
        wcsncat_s(sample, _countof(sample), t, _TRUNCATE);
        used = wcslen(sample);
    }
    DebugLog_Write(L"[backend.ui] tracked set count=%d sample=%s", outN, sample[0] ? sample : L"-");
}

void BackendUI_ClearTrackedHids()
{
    g_trackedCount.store(0, std::memory_order_release);
    DebugLog_Write(L"[backend.ui] tracked cleared");
}

uint16_t BackendUI_GetAnalogMilli(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return 0;
    return g_uiAnalogM[hid].load(std::memory_order_relaxed);
}

uint16_t BackendUI_GetRawMilli(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return 0;
    return g_uiRawM[hid].load(std::memory_order_relaxed);
}

void BackendUI_SetBindCapture(bool enable)
{
    g_bindCaptureEnabled.store(enable, std::memory_order_release);
    if (!enable)
    {
        g_bindCapturedPacked.store(0, std::memory_order_release);
        g_bindHadDown.store(false, std::memory_order_relaxed);
    }
}

bool BackendUI_ConsumeBindCapture(uint16_t* outHid, uint16_t* outRawMilli)
{
    uint32_t p = g_bindCapturedPacked.exchange(0, std::memory_order_acq_rel);
    if (!p) return false;
    if (outHid) *outHid = (uint16_t)(p & 0xFFFFu);
    if (outRawMilli) *outRawMilli = (uint16_t)((p >> 16) & 0xFFFFu);
    return true;
}

uint64_t BackendUI_ConsumeDirtyChunk(int chunk)
{
    if (chunk < 0 || chunk >= 4) return 0;
    return g_uiDirty[chunk].exchange(0, std::memory_order_acq_rel);
}

BackendStatus Backend_GetStatus()
{
    BackendStatus s;
    s.vigemOk = g_vigemOk.load(std::memory_order_acquire);
    s.lastVigemError = g_vigemLastErr.load(std::memory_order_acquire);
    return s;
}

void Backend_GetAnalogTelemetry(BackendAnalogTelemetry* out)
{
    if (!out) return;
    BackendAnalogTelemetry t{};
    t.sdkInitialised = wooting_analog_is_initialised();
    t.deviceCount = std::clamp(g_knownDeviceCount.load(std::memory_order_relaxed), 0, (int)g_knownDeviceIds.size());
    t.keycodeMode = g_keycodeMode.load(std::memory_order_relaxed);
    t.keyboardEventSeq = g_keyboardEventSeq.load(std::memory_order_acquire);
    t.trackedMaxRawMilli = g_tmTrackedMaxRawMilli.load(std::memory_order_relaxed);
    t.trackedMaxOutMilli = g_tmTrackedMaxOutMilli.load(std::memory_order_relaxed);
    t.fullBufferRet = g_tmFullBufferRet.load(std::memory_order_relaxed);
    t.fullBufferMaxMilli = g_tmFullBufferMaxMilli.load(std::memory_order_relaxed);
    t.fullBufferDeviceBestRet = g_tmFullBufferDeviceBestRet.load(std::memory_order_relaxed);
    t.fullBufferDeviceBestMaxMilli = g_tmFullBufferDeviceBestMaxMilli.load(std::memory_order_relaxed);
    t.lastAnalogError = g_lastAnalogErrorCode.load(std::memory_order_relaxed);
    *out = t;
}

void Backend_GetMouseStickDebug(BackendMouseStickDebug* out)
{
    if (!out) return;
    BackendMouseStickDebug d{};
    d.enabled = (g_mouseDbgEnabled.load(std::memory_order_relaxed) != 0);
    d.usingRawInput = (g_mouseDbgUsingRaw.load(std::memory_order_relaxed) != 0);
    d.targetX = (float)g_mouseDbgTargetX10.load(std::memory_order_relaxed) / 10.0f;
    d.targetY = (float)g_mouseDbgTargetY10.load(std::memory_order_relaxed) / 10.0f;
    d.followerX = (float)g_mouseDbgFollowerX10.load(std::memory_order_relaxed) / 10.0f;
    d.followerY = (float)g_mouseDbgFollowerY10.load(std::memory_order_relaxed) / 10.0f;
    d.outputX = (float)g_mouseDbgOutX1000.load(std::memory_order_relaxed) / 1000.0f;
    d.outputY = (float)g_mouseDbgOutY1000.load(std::memory_order_relaxed) / 1000.0f;
    d.radius = std::max(0.001f, (float)g_mouseDbgRadius1000.load(std::memory_order_relaxed) / 1000.0f);
    *out = d;
}

bool Backend_ConsumeDigitalFallbackWarning()
{
    if (!Settings_GetDigitalFallbackInput())
    {
        g_digitalFallbackWarnPending.store(false, std::memory_order_release);
        return false;
    }
    return g_digitalFallbackWarnPending.exchange(false, std::memory_order_acq_rel);
}

void Backend_NotifyDeviceChange()
{
    if (!g_virtualPadsEnabled.load(std::memory_order_acquire))
        return;

    // Ignore generic device-change noise while ViGEm is healthy.
    // Realtime tick already detects real update failures and reconnects.
    if (g_vigemOk.load(std::memory_order_acquire))
        return;

    ULONGLONG now = GetTickCount64();
    ULONGLONG ignoreUntil = g_ignoreDeviceChangeUntilMs.load(std::memory_order_acquire);
    if (now < ignoreUntil)
        return;

    g_deviceChangeReconnectRequested.store(true, std::memory_order_release);
}

void Backend_NotifyKeyboardEvent(
    uint16_t hidHint,
    uint16_t scanCode,
    uint16_t vkCode,
    bool isKeyDown,
    bool isInjected)
{
    if (hidHint == 0 || isInjected) return;

    if (hidHint < 256)
    {
        if (scanCode != 0) g_hidToScan[hidHint].store(scanCode, std::memory_order_relaxed);
        if (vkCode != 0) g_hidToVk[hidHint].store(vkCode, std::memory_order_relaxed);
    }

    if (!isKeyDown) return;

    g_keyboardEventHid.store(hidHint, std::memory_order_relaxed);
    g_keyboardEventScan.store(scanCode, std::memory_order_relaxed);
    g_keyboardEventVk.store(vkCode, std::memory_order_relaxed);
    g_keyboardEventSeq.fetch_add(1u, std::memory_order_release);
}

void Backend_AddMouseDelta(int dx, int dy)
{
    if (dx != 0)
    {
        int old = g_mouseRawAccumDx.load(std::memory_order_relaxed);
        for (;;)
        {
            int nxt = std::clamp(old + dx, -32768, 32767);
            if (g_mouseRawAccumDx.compare_exchange_weak(old, nxt, std::memory_order_release, std::memory_order_relaxed))
                break;
        }
    }
    if (dy != 0)
    {
        int old = g_mouseRawAccumDy.load(std::memory_order_relaxed);
        for (;;)
        {
            int nxt = std::clamp(old + dy, -32768, 32767);
            if (g_mouseRawAccumDy.compare_exchange_weak(old, nxt, std::memory_order_release, std::memory_order_relaxed))
                break;
        }
    }
}

void Backend_SetMouseBindButtonState(uint16_t mouseBindHid, bool down)
{
    switch (mouseBindHid)
    {
    case kMouseBindHidLButton: g_mouseBindButtons[0].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    case kMouseBindHidRButton: g_mouseBindButtons[1].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    case kMouseBindHidMButton: g_mouseBindButtons[2].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    case kMouseBindHidX1: g_mouseBindButtons[3].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    case kMouseBindHidX2: g_mouseBindButtons[4].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    default: break;
    }
}

void Backend_PulseMouseBindWheel(uint16_t mouseBindHid)
{
    // Keep wheel as a short digital pulse.
    constexpr ULONGLONG kPulseMs = 42;
    ULONGLONG until = GetTickCount64() + kPulseMs;
    if (mouseBindHid == kMouseBindHidWheelUp)
        g_mouseWheelPulseUpUntilMs.store(until, std::memory_order_relaxed);
    else if (mouseBindHid == kMouseBindHidWheelDown)
        g_mouseWheelPulseDownUntilMs.store(until, std::memory_order_relaxed);
}

void Backend_SetVirtualGamepadCount(int count)
{
    count = std::clamp(count, 1, kMaxVirtualPads);
    int old = g_virtualPadCount.exchange(count, std::memory_order_acq_rel);
    if (old != count)
        g_reconnectRequested.store(true, std::memory_order_release);
}

int Backend_GetVirtualGamepadCount()
{
    return g_virtualPadCount.load(std::memory_order_acquire);
}

void Backend_SetVirtualGamepadsEnabled(bool on)
{
    bool old = g_virtualPadsEnabled.exchange(on, std::memory_order_acq_rel);
    if (old != on)
        g_reconnectRequested.store(true, std::memory_order_release);
}

bool Backend_GetVirtualGamepadsEnabled()
{
    return g_virtualPadsEnabled.load(std::memory_order_acquire);
}

uint32_t Backend_GetLastInitIssues()
{
    return g_lastInitIssues.load(std::memory_order_acquire);
}
