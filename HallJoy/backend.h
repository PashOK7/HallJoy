// backend.h
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

#include <ViGEm/Client.h>

enum BackendInitIssue : uint32_t
{
    BackendInitIssue_None = 0,
    BackendInitIssue_WootingSdkMissing = 1u << 0,
    BackendInitIssue_WootingNoPlugins = 1u << 1,
    BackendInitIssue_WootingIncompatible = 1u << 2,
    BackendInitIssue_VigemBusMissing = 1u << 3,
    BackendInitIssue_Unknown = 1u << 31,
};

bool Backend_Init();
void Backend_Shutdown();
void Backend_Tick();
uint32_t Backend_GetLastInitIssues();

// Virtual X360 gamepad count in ViGEm (1..4). Can be changed at runtime.
void Backend_SetVirtualGamepadCount(int count);
int Backend_GetVirtualGamepadCount();
void Backend_SetVirtualGamepadsEnabled(bool on);
bool Backend_GetVirtualGamepadsEnabled();

SHORT Backend_GetLastRX();
XUSB_REPORT Backend_GetLastReport();
XUSB_REPORT Backend_GetLastReportForPad(int padIndex);

// ---- UI snapshot API (HID < 256) ----

// UI tells backend which HID codes are present on the Main page (so backend doesn't depend on UI/layout)
void BackendUI_SetTrackedHids(const uint16_t* hids, int count);
void BackendUI_ClearTrackedHids();

// last analog value after curve/deadzones, milli-units [0..1000]
uint16_t BackendUI_GetAnalogMilli(uint16_t hid);

// NEW: raw analog value as reported by device (before invert/curve), milli-units [0..1000]
uint16_t BackendUI_GetRawMilli(uint16_t hid);

// Bind-capture helpers for layout editor:
// - Enable capture mode
// - Consume first newly-pressed HID (edge-triggered) and its raw milli value
void BackendUI_SetBindCapture(bool enable);
bool BackendUI_ConsumeBindCapture(uint16_t* outHid, uint16_t* outRawMilli);

// dirty bits: which HID values changed since last consume.
// chunk: 0..3 for HID ranges [0..63], [64..127], [128..191], [192..255]
uint64_t BackendUI_ConsumeDirtyChunk(int chunk);

// ---- Status / hotplug ----
struct BackendStatus
{
    bool vigemOk = false;
    VIGEM_ERROR lastVigemError = VIGEM_ERROR_NONE;
};

BackendStatus Backend_GetStatus();

struct BackendAnalogTelemetry
{
    bool sdkInitialised = false;
    int deviceCount = 0;                 // unique SDK device ids
    int keycodeMode = 0;                 // WootingAnalog_KeycodeType
    uint32_t keyboardEventSeq = 0;       // increments on physical key-down events
    uint16_t trackedMaxRawMilli = 0;     // last tracked-page max raw [0..1000]
    uint16_t trackedMaxOutMilli = 0;     // last tracked-page max filtered [0..1000]
    int fullBufferRet = 0;               // last read_full_buffer return
    uint16_t fullBufferMaxMilli = 0;     // last max value from read_full_buffer
    int fullBufferDeviceBestRet = 0;     // best return among read_full_buffer_device
    uint16_t fullBufferDeviceBestMaxMilli = 0; // best max among device buffers
    int lastAnalogError = 0;             // last negative read_analog code (if any)
};

void Backend_GetAnalogTelemetry(BackendAnalogTelemetry* out);
bool Backend_ConsumeDigitalFallbackWarning();

struct BackendMouseStickDebug
{
    bool enabled = false;
    bool usingRawInput = false;
    float targetX = 0.0f;   // virtual mouse target (mouse-space units)
    float targetY = 0.0f;   // Y up
    float followerX = 0.0f; // virtual anchor (mouse-space units)
    float followerY = 0.0f; // Y up
    float outputX = 0.0f;   // final stick output [-1..1]
    float outputY = 0.0f;   // final stick output [-1..1]
    float radius = 1.0f;    // current max offset range used for debug visualization
};

void Backend_GetMouseStickDebug(BackendMouseStickDebug* out);

// request reconnect attempt on next tick (e.g. on WM_DEVICECHANGE)
void Backend_NotifyDeviceChange();

// Optional hint from low-level keyboard hook for adaptive diagnostics/autofix.
void Backend_NotifyKeyboardEvent(
    uint16_t hidHint,
    uint16_t scanCode,
    uint16_t vkCode,
    bool isKeyDown,
    bool isInjected);

// Feed raw mouse delta (from WM_INPUT) for Mouse->Stick path.
void Backend_AddMouseDelta(int dx, int dy);

// Feed mouse button/wheel input for mouse pseudo-bindings.
void Backend_SetMouseBindButtonState(uint16_t mouseBindHid, bool down);
void Backend_PulseMouseBindWheel(uint16_t mouseBindHid);
