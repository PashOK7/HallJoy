// backend.h
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstddef>

#include "native_analog_backend.h"

#include <ViGEm/Client.h>

enum BackendInitIssue : uint32_t
{
    BackendInitIssue_None = 0,
    BackendInitIssue_PrivateUapUnavailable = 1u << 0,
    BackendInitIssue_PrivateUapNoDevices = 1u << 1,
    BackendInitIssue_PrivateUapIncompatible = 1u << 2,
    BackendInitIssue_VigemBusMissing = 1u << 3,
    BackendInitIssue_Unknown = 1u << 31,
};

bool Backend_Init();
[[nodiscard]] bool Backend_Shutdown();
// Allocation-free fail-safe publication used only when the realtime worker
// crosses its top-level exception boundary.
void Backend_ResetPublishedStateAfterRealtimeFault() noexcept;
void Backend_Tick();
// Realtime-thread deadline for the newest coalesced ViGEm report, in QPC ticks.
// Zero means no output is pending.
LONGLONG Backend_GetNextOutputDeadlineQpc();
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



static constexpr int kBackendMaxNativeProtocols = 16;

struct BackendNativeProtocolTelemetry
{
    bool present = false;
    bool connected = false;
    std::uint16_t protocol = 0;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint16_t usagePage = 0;
    std::uint16_t usage = 0;
    std::uint32_t flags = 0;
    std::uint32_t mappedKeys = 0;
    std::uint32_t activeKeys = 0;
    std::uint32_t nominalRawLevels = 0;
    std::uint32_t inputReportBytes = 0;
    std::uint32_t outputReportBytes = 0;
    std::uint32_t updateHz10 = 0;
    std::uint32_t averageIntervalUs = 0;
    std::uint32_t maximumIntervalUs = 0;
    std::uint32_t lastUpdateAgeMs = 0;
    std::uint64_t successfulUpdates = 0;
    std::uint64_t failedUpdates = 0;
    char id[32]{};
    wchar_t name[64]{};
    wchar_t status[kNativeAnalogBackendStatusChars]{};
};

static constexpr int kBackendMaxAnalogDevices = 8;

enum BackendAnalogDeviceFlags : uint32_t
{
    BackendAnalogDeviceFlag_None = 0,
    BackendAnalogDeviceFlag_Connected = 1u << 0,
    BackendAnalogDeviceFlag_PolledTransport = 1u << 1,
    BackendAnalogDeviceFlag_SynchronousHallJoyPoll = 1u << 2,
    BackendAnalogDeviceFlag_StreamTransport = 1u << 3,
    BackendAnalogDeviceFlag_UnthrottledWorker = 1u << 4,
    BackendAnalogDeviceFlag_DuplicateSafeId = 1u << 5,
};

struct BackendAnalogDeviceTelemetry
{
    bool present = false;
    uint64_t deviceId = 0;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint16_t usagePage = 0;
    uint16_t usage = 0;
    uint32_t flags = 0;
    uint32_t rows = 0;
    uint32_t columns = 0;
    uint32_t layoutKeySlots = 0;
    uint32_t nominalRawLevels = 0;
    uint32_t inputReportBytes = 0;
    uint32_t outputReportBytes = 0;
    uint32_t featureReportBytes = 0;
    bool bluetooth = false;
    uint32_t observedDistinctLevels = 0;
    uint32_t observedKeys = 0;
    uint32_t observedLevelsPerKeyMin = 0;
    uint32_t observedLevelsPerKeyMax = 0;
    uint32_t observedLevelsPerKeyAverage10 = 0;
    uint32_t activeKeys = 0;
    uint32_t updateHz10 = 0;
    uint32_t averageUpdateIntervalUs = 0;
    uint32_t maximumUpdateIntervalUs = 0;
    uint32_t lastUpdateAgeMs = 0;
    uint64_t updateCount = 0;
    char manufacturer[48]{};
    char name[80]{};
};

struct BackendAnalogTelemetry
{
    int nativeProtocolCount = 0;
    BackendNativeProtocolTelemetry nativeProtocols[kBackendMaxNativeProtocols]{};
    bool sdkInitialised = false;         // true when Wooting SDK or any native HID path is active
    int deviceCount = 0;                 // connected analog sources (SDK devices + native HID paths)
    bool mad68Present = false;           // MAD68 Pro R vendor interface detected
    bool mad68Connected = false;         // validated native A0 stream is publishing
    bool mad68Full = false;              // complete 68/68 snapshot, 67 HID keys published
    bool mad68EmergencyWasd = false;     // degraded but real W/A/S/D analog publication
    uint16_t mad68ProductId = 0;        // routed MADLIONS PID
    uint16_t mad68FirmwareVersion = 0;   // USB bcdDevice
    uint32_t mad68Coverage = 0;          // observed descriptors out of 68
    uint32_t mad68PublishedKeys = 0;     // 0, 4, or 67
    bool hex80Present = false;            // ATK x QK Hex80 vendor interface detected
    bool hex80Connected = false;          // validated native 0x96 matrix polling active
    uint16_t hex80VendorId = 0;
    uint16_t hex80ProductId = 0;
    uint16_t hex80FirmwareVersion = 0;
    uint16_t hex80TravelMax = 0;
    uint32_t hex80MappedKeys = 0;
    uint32_t hex80ActiveKeys = 0;
    uint32_t hex80ObservedKeys = 0;
    uint32_t hex80InputReportBytes = 0;
    uint32_t hex80OutputReportBytes = 0;
    uint32_t hex80ChunkHz10 = 0;
    uint32_t hex80MatrixHz10 = 0;
    uint32_t hex80AvgTransactionUs = 0;
    uint32_t hex80MaxTransactionUs = 0;
    uint32_t hex80AvgMatrixIntervalUs = 0;
    uint32_t hex80MaxMatrixIntervalUs = 0;
    uint32_t hex80LastPacketAgeMs = 0;
    uint64_t hex80PollAttempts = 0;
    uint64_t hex80PollSuccess = 0;
    uint64_t hex80PollFail = 0;
    uint64_t hex80MatrixCycles = 0;
    bool addressedPresent = false;        // capability-validated 09/94/02 interface reserved from UAP
    bool addressedConnected = false;      // addressed polling is publishing fresh values
    uint16_t addressedVendorId = 0;
    uint16_t addressedProductId = 0;
    uint32_t addressedMappedKeys = 0;
    uint32_t addressedActiveKeys = 0;
    uint32_t addressedInputReportBytes = 0;
    uint32_t addressedOutputReportBytes = 0;
    uint32_t addressedLastResponseAgeMs = 0;
    uint64_t addressedPollAttempts = 0;
    uint64_t addressedPollSuccess = 0;
    uint64_t addressedPollFail = 0;
    bool sayoConnected = false;          // native SayoDevice HID path connected
    uint16_t sayoVendorId = 0;           // current SayoDevice VID when connected
    uint16_t sayoProductId = 0;          // current SayoDevice PID when connected
    int sayoReaders = 0;                 // active SayoDevice HID interfaces
    uint32_t sayoDepthHz10 = 0;          // Sayo depth response frequency * 10
    uint32_t sayoAvgDepthIntervalUs = 0; // average Sayo depth response interval
    uint32_t sayoMaxDepthIntervalUs = 0; // max Sayo depth response interval since connect
    uint32_t sayoDepthRawLevels = 0;     // source raw depth levels when known
    uint32_t sayoLastDepthAgeMs = 0;
    uint32_t sayoLastPacketAgeMs = 0;
    uint64_t sayoPollAttempts = 0;
    uint64_t sayoPollSuccess = 0;
    uint64_t sayoPollFail = 0;
    uint64_t sayoDepthPackets = 0;
    uint32_t sayoMappedKeys = 0;
    uint32_t sayoInputReportBytes = 0;
    uint32_t sayoOutputReportBytes = 0;
    uint32_t sayoWriteCapableReaders = 0;
    uint32_t sayoObservedKeys = 0;
    uint32_t sayoObservedPositionsMin = 0;
    uint32_t sayoObservedPositionsMax = 0;
    uint32_t sayoObservedPositionsAverage10 = 0;
    bool sparkConnected = false;         // native SparkLink HID path connected
    uint16_t sparkVendorId = 0;          // current SparkLink VID when connected
    uint16_t sparkProductId = 0;         // current SparkLink PID when connected
    int sparkRows = 0;                   // discovered SparkLink matrix rows
    int sparkActiveRows = 0;             // active/mapped rows that are polled
    uint32_t sparkRouteQueries = 0;      // cumulative route-row queries
    uint32_t sparkRouteOk = 0;           // cumulative successful route-row queries
    uint32_t sparkRouteFail = 0;         // cumulative failed route-row queries
    uint32_t sparkRouteHz10 = 0;         // route-row packet frequency * 10
    uint32_t sparkMatrixHz10 = 0;        // full-matrix refresh estimate * 10
    uint32_t sparkAvgIntervalUs = 0;     // average successful route-row interval
    uint32_t sparkMaxIntervalUs = 0;     // max successful route-row interval since connect
    uint32_t sparkAvgRouteTxUs = 0;      // average route-row USB request/response time
    uint32_t sparkMaxRouteTxUs = 0;      // max route-row USB request/response time
    uint32_t sparkMappedAnalogKeys = 0;  // mapped SparkLink HID keys
    uint32_t sparkObservedRawMin = 0;     // observed raw full-scale min across mapped keys
    uint32_t sparkObservedRawMax = 0;     // observed raw full-scale max across mapped keys
    uint32_t analogOutputLevels = 1001;   // HallJoy normalized output levels [0..1000]
    uint32_t sparkLastRouteAgeMs = 0;    // age of last route-row response
    uint32_t sparkMaxRowAgeMs = 0;       // max age among active rows
    uint8_t sparkLastRouteRow = 0xFF;    // last queried row
    bool sparkLastRouteOk = false;       // last queried row success
    uint32_t sparkPollMode = 0;           // SettingsSparkPollMode
    uint32_t sparkRowLimit = 0;           // 0 auto, 1..8 experimental cap
    bool pluginHostAvailable = false;
    bool pluginHostReady = false;
    int pluginHostStatus = 0;
    int pluginHostLastError = 0;
    int pluginHostTransportError = 0;
    int pluginHostRestartCount = 0;
    int pluginHostInvalidSnapshots = 0;
    int pluginHostActiveKeys = 0;
    int pluginHostDenseDeviceCount = 0;
    uint64_t pluginHostSnapshotGeneration = 0;
    uint64_t pluginHostSnapshotTimestampUs = 0;
    uint32_t pluginHostPollHz10 = 0;
    uint32_t pluginHostSuccessfulPollHz10 = 0;
    uint32_t pluginHostLastPublishAgeMs = 0;
    uint64_t pluginHostTotalPolls = 0;
    uint64_t pluginHostSuccessfulPolls = 0;
    int pluginDeviceCount = 0;
    BackendAnalogDeviceTelemetry pluginDevices[kBackendMaxAnalogDevices]{};
    uint32_t sdkPollHz10 = 0;             // configured HallJoy polling target * 10
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

// Integrated SparkLink/Sayo implementations expose the same descriptor contract
// as standalone native protocol modules. New protocols should normally live in
// their own *_backend.cpp and export one descriptor getter.
const NativeAnalogBackendDescriptor& BackendNative_GetSparkDescriptor();
const NativeAnalogBackendDescriptor& BackendNative_GetSayoDescriptor();
