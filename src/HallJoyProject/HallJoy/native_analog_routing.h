#pragma once

#include <cstdint>

enum class NativeAnalogProtocol : std::uint8_t
{
    Mad68A0 = 1,
    Hex80 = 2,
    Addressed09402 = 3,
    SparkLink = 4,
    SayoDepth = 5,
};

// Central startup-time ownership registry for native analogue protocols.
// A VID/PID pair enters the registry only after the corresponding backend has
// completed its protocol-specific capability proof. The complete exact-ID set
// is published to the isolated UAP child before it enumerates HID paths.
void NativeAnalogRouting_Reset();
bool NativeAnalogRouting_Claim(
    std::uint16_t vendorId,
    std::uint16_t productId,
    NativeAnalogProtocol protocol);
bool NativeAnalogRouting_IsClaimed(std::uint16_t vendorId, std::uint16_t productId);
bool NativeAnalogRouting_IsClaimedBy(
    std::uint16_t vendorId,
    std::uint16_t productId,
    NativeAnalogProtocol protocol);
