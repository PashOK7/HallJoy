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

// request reconnect attempt on next tick (e.g. on WM_DEVICECHANGE)
void Backend_NotifyDeviceChange();
