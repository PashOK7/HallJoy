#pragma once

#include <cstdint>
#include <type_traits>

namespace HallJoyPluginTelemetry
{
    constexpr std::uint32_t kVersion = 1;
    constexpr std::uint32_t kMaxDevices = 8;

    enum DeviceFlags : std::uint32_t
    {
        DeviceFlag_None = 0,
        DeviceFlag_Connected = 1u << 0,
        DeviceFlag_PolledTransport = 1u << 1,
        DeviceFlag_SynchronousHallJoyPoll = 1u << 2,
        DeviceFlag_StreamTransport = 1u << 3,
        DeviceFlag_UnthrottledWorker = 1u << 4,
        DeviceFlag_DuplicateSafeId = 1u << 5,
    };

    // Optional ABI exported by UniversalAnalogPluginFixed and consumed only by
    // HallJoy's isolated analog host. Values describe what is actually observed
    // by the plugin. Unknown topology/resolution fields are zero.
    struct alignas(8) DeviceV1
    {
        std::uint32_t version = kVersion;
        std::uint32_t structSize = 0;
        std::uint64_t deviceId = 0;

        std::uint16_t vendorId = 0;
        std::uint16_t productId = 0;
        std::uint16_t usagePage = 0;
        std::uint16_t usage = 0;

        std::uint32_t flags = DeviceFlag_None;
        std::uint32_t rows = 0;
        std::uint32_t columns = 0;
        std::uint32_t layoutKeySlots = 0;
        std::uint32_t nominalRawLevels = 0;
        std::uint32_t inputReportBytes = 0;
        std::uint32_t outputReportBytes = 0;
        std::uint32_t featureReportBytes = 0;
        std::uint32_t bluetooth = 0;
        std::uint32_t observedDistinctLevels = 0;
        std::uint32_t observedKeys = 0;
        std::uint32_t observedLevelsPerKeyMin = 0;
        std::uint32_t observedLevelsPerKeyMax = 0;
        std::uint32_t observedLevelsPerKeyAverage10 = 0;
        std::uint32_t activeKeys = 0;

        std::uint32_t updateHz10 = 0;
        std::uint32_t averageUpdateIntervalUs = 0;
        std::uint32_t maximumUpdateIntervalUs = 0;
        std::uint32_t lastUpdateAgeMs = 0;
        std::uint64_t updateCount = 0;

        char manufacturer[48]{};
        char name[80]{};
    };

    static_assert(std::is_standard_layout_v<DeviceV1>, "telemetry ABI must remain standard-layout");
    static_assert(std::is_trivially_copyable_v<DeviceV1>, "telemetry ABI must remain trivially copyable");
}
