// backend.h
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

#include <ViGEm/Client.h>

bool Backend_Init();
void Backend_Shutdown();
void Backend_Tick();

SHORT Backend_GetLastRX();
XUSB_REPORT Backend_GetLastReport();

// ---- UI snapshot API (HID < 256) ----

// UI tells backend which HID codes are present on the Main page (so backend doesn't depend on UI/layout)
void BackendUI_SetTrackedHids(const uint16_t* hids, int count);
void BackendUI_ClearTrackedHids();

// last analog value after curve/deadzones, milli-units [0..1000]
uint16_t BackendUI_GetAnalogMilli(uint16_t hid);

// NEW: raw analog value as reported by device (before invert/curve), milli-units [0..1000]
uint16_t BackendUI_GetRawMilli(uint16_t hid);

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