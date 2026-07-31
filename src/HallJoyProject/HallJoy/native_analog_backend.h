#pragma once

#include <cstddef>
#include <cstdint>

#include "native_analog_routing.h"

// Internal ABI for HallJoy native analogue protocol modules.
// A new protocol implementation owns its USB/HID transport and publishes only
// normalized [0..1000] values. The common backend owns curves, bindings, SOCD,
// UI snapshots and ViGEm scheduling.
static constexpr std::uint32_t kNativeAnalogBackendAbiVersion = 1;
static constexpr std::size_t kNativeAnalogBackendStatusChars = 160;

enum class NativeAnalogStartPhase : std::uint8_t
{
    BeforeUap = 1,       // capability proof/start must happen before Soup/UAP opens HID paths
    AfterRealtime = 2,   // worker may wake the common realtime loop immediately
    AfterRawInput = 3,   // protocol needs target-scoped Raw Input to be registered first
};

enum NativeAnalogBackendFlags : std::uint32_t
{
    NativeAnalogBackendFlag_None = 0,
    NativeAnalogBackendFlag_PolledTransport = 1u << 0,
    NativeAnalogBackendFlag_StreamTransport = 1u << 1,
    NativeAnalogBackendFlag_ReadOnlyProbe = 1u << 2,
    NativeAnalogBackendFlag_ReversibleControlProbe = 1u << 3,
    NativeAnalogBackendFlag_DynamicVidPid = 1u << 4,
    NativeAnalogBackendFlag_RequiresRawInput = 1u << 5,
};

struct NativeAnalogBackendTelemetry
{
    bool present = false;
    bool connected = false;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint16_t usagePage = 0;
    std::uint16_t usage = 0;
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
    wchar_t status[kNativeAnalogBackendStatusChars]{};
};

struct NativeAnalogBackendDescriptor
{
    std::uint32_t abiVersion = kNativeAnalogBackendAbiVersion;
    std::uint32_t structSize = 0;
    const char* id = nullptr;                 // stable ASCII id, e.g. "hex80-0x96"
    const wchar_t* displayName = nullptr;     // UI/debug label
    NativeAnalogProtocol protocol = NativeAnalogProtocol::Mad68A0;
    NativeAnalogStartPhase startPhase = NativeAnalogStartPhase::AfterRealtime;
    std::uint32_t flags = NativeAnalogBackendFlag_None;

    // prepareRouting must perform only the protocol's documented safe capability
    // proof. It claims exact VID/PID ownership through NativeAnalogRouting_Claim
    // only after the response semantics are validated.
    bool (*prepareRouting)() = nullptr;
    bool (*start)() = nullptr;
    bool (*stop)() = nullptr;
    void (*notifyDeviceChange)() = nullptr;
    bool (*isProtocolDevicePresent)() = nullptr;
    bool (*isConnected)() = nullptr;
    bool (*ownsHid)(std::uint16_t hidUsage) = nullptr;
    std::uint16_t (*getMilli)(std::uint16_t hidUsage) = nullptr;
    void (*getTelemetry)(NativeAnalogBackendTelemetry* out) = nullptr;
};

inline bool NativeAnalogBackendDescriptor_IsValid(const NativeAnalogBackendDescriptor& d)
{
    const bool validPhase = d.startPhase == NativeAnalogStartPhase::BeforeUap ||
        d.startPhase == NativeAnalogStartPhase::AfterRealtime ||
        d.startPhase == NativeAnalogStartPhase::AfterRawInput;
    const std::uint32_t knownFlags = NativeAnalogBackendFlag_PolledTransport |
        NativeAnalogBackendFlag_StreamTransport |
        NativeAnalogBackendFlag_ReadOnlyProbe |
        NativeAnalogBackendFlag_ReversibleControlProbe |
        NativeAnalogBackendFlag_DynamicVidPid |
        NativeAnalogBackendFlag_RequiresRawInput;
    return d.abiVersion == kNativeAnalogBackendAbiVersion &&
        d.structSize >= sizeof(NativeAnalogBackendDescriptor) &&
        d.id && *d.id && d.displayName && *d.displayName &&
        static_cast<std::uint8_t>(d.protocol) != 0 && validPhase &&
        (d.flags & ~knownFlags) == 0 &&
        d.start && d.stop && d.isProtocolDevicePresent && d.isConnected &&
        d.ownsHid && d.getMilli && d.getTelemetry;
}
