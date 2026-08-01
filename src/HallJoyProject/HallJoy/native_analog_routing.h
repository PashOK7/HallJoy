#pragma once

#include <cstdint>

enum class NativeAnalogProtocol : std::uint8_t
{
    Mad68A0 = 1,
    Hex80 = 2,
    Addressed09402 = 3,
    SparkLink = 4,
    SayoDepth = 5,
    Simulator = 250,
};

// Central startup-time ownership registry for native analogue protocols.
// A concrete HID interface path enters the registry only after the corresponding
// backend has completed its protocol-specific capability proof. Compact exact
// path fingerprints are published before the isolated UAP enumerates HID paths.
void NativeAnalogRouting_Reset();
bool NativeAnalogRouting_Claim(
    std::uint16_t vendorId,
    std::uint16_t productId,
    const wchar_t* interfacePath,
    NativeAnalogProtocol protocol);
bool NativeAnalogRouting_IsClaimed(const wchar_t* interfacePath);
bool NativeAnalogRouting_IsClaimedBy(
    const wchar_t* interfacePath,
    NativeAnalogProtocol protocol);
bool NativeAnalogRouting_ProtocolHasClaims(NativeAnalogProtocol protocol);
