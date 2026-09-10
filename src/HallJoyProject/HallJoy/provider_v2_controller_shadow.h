#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>

#include "analog_key_codes.h"
#include "provider_v2_snapshot_broker.h"
#include "uap_parent_snapshot.h"
#include "virtual_controller_frame.h"

namespace halljoy::provider_v2_shadow
{
// Provider V2 owns key identities, including zero-valued samples. Keeping
// ownership separate from values is essential: zero is a valid released key,
// not evidence that the provider failed to report it.
struct RawInputMapV1 final
{
    std::array<float, keycode::kCount> values{};
    std::bitset<keycode::kCount> owned{};
    std::uint32_t mappedSampleCount = 0;
    std::uint32_t ignoredSampleCount = 0;
};

enum class ProjectionError : std::uint32_t
{
    None = 0,
    Unavailable,
    NotAuthoritative,
    CapacityMismatch,
    InvalidSnapshot,
    InvalidSample,
};

// A source state makes the distinction between unavailable, owned-zero and a
// usable non-zero sample explicit. Values are normalized only at this boundary.
struct AnalogSourceStateV1 final
{
    bool available = false;
    bool owned = false;
    bool fresh = false;
    float value = 0.0f;
};

enum ArbitrationSourceMask : std::uint32_t
{
    ArbitrationSource_None = 0,
    ArbitrationSource_Native = 1u << 0,
    ArbitrationSource_Provider = 1u << 1,
    ArbitrationSource_DigitalFallback = 1u << 2,
};

struct ArbitrationInputV1 final
{
    std::uint16_t keyCode = 0;
    AnalogSourceStateV1 native{};
    AnalogSourceStateV1 provider{};
    AnalogSourceStateV1 digitalFallback{};
    bool allowDigitalFallback = false;
};

struct ArbitrationResultV1 final
{
    float value = 0.0f;
    std::uint32_t sourceMask = ArbitrationSource_None;
};

// HallJoy's characterized policy: standard keyboard usages may take the max
// of native and provider sources; a native extended key remains authoritative.
// An owned native zero is still ownership and blocks a digital fallback.
ArbitrationResultV1 Arbitrate(const ArbitrationInputV1& input) noexcept;

// Projects only identities that HallJoy can bind today:
// - USB HID keyboard-page usages 01..FF;
// - non-byte UAP extended identities that fit the versioned HallJoy key space.
// Consumer and future semantic namespaces remain explicit and unbound instead
// of being aliased to unrelated keyboard keys.
bool MatchesCapturedPublication(
    const provider_v2_snapshot_broker::SnapshotMetadataV1& provider,
    const uap_parent_snapshot::SnapshotV1& dense) noexcept;

ProjectionError ProjectCapturedSnapshot(
    const analog_provider_v2::AnalogSnapshotHeaderV2& header,
    const analog_provider_v2::AnalogDeviceV2* devices,
    std::size_t deviceStorageCount,
    const analog_provider_v2::AnalogSampleV2* samples,
    std::size_t sampleStorageCount,
    RawInputMapV1* output) noexcept;

// Applies HallJoy's existing multi-source ownership policy to one bindable key.
// It intentionally has no digital-input argument.
float MergeWithNative(std::uint16_t keyCode,
    bool nativeOwned, float nativeValue,
    bool providerOwned, float providerValue) noexcept;

enum FrameMismatch : std::uint32_t
{
    FrameMismatch_None = 0,
    FrameMismatch_Buttons = 1u << 0,
    FrameMismatch_LeftTrigger = 1u << 1,
    FrameMismatch_RightTrigger = 1u << 2,
    FrameMismatch_LeftStickX = 1u << 3,
    FrameMismatch_LeftStickY = 1u << 4,
    FrameMismatch_RightStickX = 1u << 5,
    FrameMismatch_RightStickY = 1u << 6,
};

std::uint32_t CompareFrames(
    const controller::VirtualControllerFrameV1& qualified,
    const controller::VirtualControllerFrameV1& shadow) noexcept;
}
