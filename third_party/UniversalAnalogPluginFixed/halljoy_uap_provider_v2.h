#pragma once

#include <cstdint>
#include "halljoy_analog_provider_v2_contract.h"
#include "halljoy_plugin_telemetry.h"

namespace HallJoyUapProviderV2
{
constexpr std::uint64_t kProviderId = 0x3256504155594A48ull; // "HJY UAPV2"
constexpr std::uint32_t kMaxDevices = HallJoyPluginTelemetry::kMaxDevices;
constexpr std::uint32_t kMaxSamples = 2048;

constexpr halljoy::analog_provider_v2::KeyIdentityV1 IdentityFromLegacyCode(
    std::uint16_t code) noexcept
{
    using namespace halljoy::analog_provider_v2;
    if (code == 0)
        return {};
    if ((code & 0xF00u) == 0x300u)
        return UsbHidKey(0x0Cu, code & 0xFFu);
    if ((code & 0xF00u) == 0x400u)
        return UapExtendedKey(code);
    if (code <= 0xFFu)
        return UsbHidKey(0x07u, code);
    return {};
}
}
