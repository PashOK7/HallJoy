#pragma once

#include <cstddef>

namespace halljoy::raw_input
{
// GetRawInputData returns the actual size of the type-specific packet. On x64
// a keyboard packet is smaller than sizeof(RAWINPUT), because RAWINPUT's union
// is sized for RAWMOUSE. Validate the common header and the selected payload
// instead of requiring the largest union member.
constexpr bool ContainsTypedPayload(std::size_t copiedBytes,
    std::size_t declaredBytes, std::size_t payloadOffset,
    std::size_t payloadBytes) noexcept
{
    if (declaredBytes > copiedBytes || payloadOffset > copiedBytes)
        return false;
    if (payloadBytes > copiedBytes - payloadOffset)
        return false;
    return declaredBytes >= payloadOffset + payloadBytes;
}
}
