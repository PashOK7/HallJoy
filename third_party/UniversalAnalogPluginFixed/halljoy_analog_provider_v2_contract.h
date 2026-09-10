#pragma once

#include <cstdint>
#include <type_traits>

namespace halljoy::analog_provider_v2
{
constexpr std::uint16_t kKeyIdentityVersion = 1;
constexpr std::uint32_t kSnapshotSchemaVersion = 2;
constexpr std::uint32_t kDeviceSchemaVersion = 2;
constexpr std::uint32_t kSampleSchemaVersion = 2;

enum class KeyNamespace : std::uint16_t
{
    Invalid = 0,
    UsbHidUsage = 1,
    UapExtended = 2,
    HallJoySemantic = 3,
};

struct KeyIdentityV1
{
    std::uint16_t schemaVersion = kKeyIdentityVersion;
    KeyNamespace keyNamespace = KeyNamespace::Invalid;
    std::uint32_t usagePage = 0;
    std::uint32_t usage = 0;

    friend constexpr bool operator==(const KeyIdentityV1&, const KeyIdentityV1&) = default;
};

constexpr KeyIdentityV1 UsbHidKey(std::uint32_t usagePage, std::uint32_t usage) noexcept
{
    return { kKeyIdentityVersion, KeyNamespace::UsbHidUsage, usagePage, usage };
}

constexpr KeyIdentityV1 UapExtendedKey(std::uint32_t code) noexcept
{
    return { kKeyIdentityVersion, KeyNamespace::UapExtended, 0, code };
}

constexpr KeyIdentityV1 HallJoySemanticKey(
    std::uint32_t controlSet, std::uint32_t control) noexcept
{
    return { kKeyIdentityVersion, KeyNamespace::HallJoySemantic,
        controlSet, control };
}

constexpr bool IsValid(const KeyIdentityV1& key) noexcept
{
    if (key.schemaVersion != kKeyIdentityVersion || key.usage == 0)
        return false;
    switch (key.keyNamespace)
    {
    case KeyNamespace::UsbHidUsage:
        return key.usagePage != 0;
    case KeyNamespace::UapExtended:
        return key.usagePage == 0;
    case KeyNamespace::HallJoySemantic:
        return key.usagePage != 0;
    default:
        return false;
    }
}

enum AnalogValueFlags : std::uint32_t
{
    AnalogValueFlag_None = 0,
    AnalogValueFlag_RawValid = 1u << 0,
    AnalogValueFlag_LegacyQuantized = 1u << 1,
};

struct AnalogValueV2
{
    float normalized = 0.0f;
    std::uint32_t rawNumerator = 0;
    std::uint32_t rawDomain = 0;
    std::uint32_t flags = AnalogValueFlag_None;
};

enum AnalogDeviceFlags : std::uint32_t
{
    AnalogDeviceFlag_None = 0,
    AnalogDeviceFlag_Connected = 1u << 0,
    AnalogDeviceFlag_StableIdentity = 1u << 1,
    AnalogDeviceFlag_ExactInterfaceAssigned = 1u << 2,
    AnalogDeviceFlag_ProtocolProven = 1u << 3,
    AnalogDeviceFlag_LayoutProven = 1u << 4,
};

struct AnalogDeviceV2
{
    std::uint32_t schemaVersion = kDeviceSchemaVersion;
    std::uint32_t structSize = sizeof(AnalogDeviceV2);
    std::uint64_t deviceId = 0;
    std::uint64_t exactInterfaceId = 0;
    std::uint32_t flags = AnalogDeviceFlag_None;
    std::uint32_t protocolId = 0;
    std::uint32_t layoutId = 0;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint16_t usagePage = 0;
    std::uint16_t usage = 0;
};

enum AnalogSampleFlags : std::uint32_t
{
    AnalogSampleFlag_None = 0,
    AnalogSampleFlag_Owned = 1u << 0,
    AnalogSampleFlag_ValueValid = 1u << 1,
    AnalogSampleFlag_Fresh = 1u << 2,
};

struct AnalogSampleV2
{
    std::uint32_t schemaVersion = kSampleSchemaVersion;
    std::uint32_t structSize = sizeof(AnalogSampleV2);
    KeyIdentityV1 key{};
    std::uint32_t deviceIndex = 0;
    std::uint32_t flags = AnalogSampleFlag_None;
    AnalogValueV2 value{};
};

enum AnalogSnapshotFlags : std::uint32_t
{
    AnalogSnapshotFlag_None = 0,
    AnalogSnapshotFlag_Complete = 1u << 0,
    AnalogSnapshotFlag_Truncated = 1u << 1,
};

struct AnalogSnapshotHeaderV2
{
    std::uint32_t schemaVersion = kSnapshotSchemaVersion;
    std::uint32_t structSize = sizeof(AnalogSnapshotHeaderV2);
    std::uint64_t providerId = 0;
    std::uint64_t providerGeneration = 0;
    std::uint64_t sampleGeneration = 0;
    std::uint64_t valueGeneration = 0;
    std::uint64_t ownershipGeneration = 0;
    std::uint64_t sampleTimestampUs = 0;
    std::uint64_t valueTimestampUs = 0;
    std::uint32_t flags = AnalogSnapshotFlag_None;
    std::uint32_t deviceCount = 0;
    std::uint32_t deviceCapacity = 0;
    std::uint32_t requiredDeviceCount = 0;
    std::uint32_t sampleCount = 0;
    std::uint32_t sampleCapacity = 0;
    std::uint32_t requiredSampleCount = 0;
};

static_assert(sizeof(float) == 4, "AnalogProviderV2 requires 32-bit float");
static_assert(std::is_standard_layout_v<KeyIdentityV1> &&
    std::is_trivially_copyable_v<KeyIdentityV1>);
static_assert(std::is_standard_layout_v<AnalogValueV2> &&
    std::is_trivially_copyable_v<AnalogValueV2>);
static_assert(std::is_standard_layout_v<AnalogDeviceV2> &&
    std::is_trivially_copyable_v<AnalogDeviceV2>);
static_assert(std::is_standard_layout_v<AnalogSampleV2> &&
    std::is_trivially_copyable_v<AnalogSampleV2>);
static_assert(std::is_standard_layout_v<AnalogSnapshotHeaderV2> &&
    std::is_trivially_copyable_v<AnalogSnapshotHeaderV2>);
}
