#pragma once

#include <cstdint>
#include <type_traits>
#include "halljoy_plugin_telemetry.h"

// Optional HallJoy-private ABI exported by the bundled Universal Analog Plugin.
// It exposes a coherent 256-key value table per physical device. The ordinary
// Wooting/plugin ABI remains available for third-party callers.
namespace HallJoyDenseSnapshot
{
    constexpr std::uint32_t kVersion = 1;
    constexpr std::uint32_t kKeyCount = 256;
    constexpr std::uint32_t kMaxDevices = HallJoyPluginTelemetry::kMaxDevices;

    enum DeviceFlags : std::uint32_t
    {
        DeviceFlag_None = 0,
        DeviceFlag_Connected = 1u << 0,
        DeviceFlag_DuplicateSafeId = 1u << 1,
        DeviceFlag_PolledTransport = 1u << 2,
        DeviceFlag_StreamTransport = 1u << 3,
    };

    struct DeviceV1
    {
        std::uint32_t structSize;
        std::uint32_t version;
        std::uint64_t deviceId;
        std::uint64_t generation;
        std::uint64_t timestampUs;
        std::uint32_t activeKeyCount;
        std::uint32_t flags;
        std::uint16_t vendorId;
        std::uint16_t productId;
        std::uint16_t usagePage;
        std::uint16_t usage;
        float values[kKeyCount];
    };

    static_assert(sizeof(float) == 4, "Dense snapshot requires 32-bit float");
    static_assert(std::is_standard_layout_v<DeviceV1>, "Dense snapshot ABI must be standard-layout");
    static_assert(std::is_trivially_copyable_v<DeviceV1>, "Dense snapshot ABI must be trivially copyable");
}
