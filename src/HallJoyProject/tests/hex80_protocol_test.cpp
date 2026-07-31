#include "../HallJoy/hex80_protocol.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{
std::array<std::uint8_t, hex80::kPayloadBytes> MakeTravelInfo(std::uint16_t maximum)
{
    std::array<std::uint8_t, hex80::kPayloadBytes> response{};
    response[0] = hex80::kGetValue;
    response[1] = hex80::kCustomCommand;
    response[2] = hex80::kTravelInfo;
    response[3] = static_cast<std::uint8_t>(maximum >> 8);
    response[4] = static_cast<std::uint8_t>(maximum & 0xFFu);
    return response;
}

std::array<std::uint8_t, hex80::kPayloadBytes> MakeChunk(
    std::uint16_t offset,
    std::uint8_t count,
    const std::array<std::uint16_t, hex80::kChunkSize>& travel)
{
    std::array<std::uint8_t, hex80::kPayloadBytes> response{};
    response[0] = hex80::kGetValue;
    response[1] = hex80::kCustomCommand;
    response[2] = hex80::kTravelBuffer;
    response[5] = static_cast<std::uint8_t>(offset >> 8);
    response[6] = static_cast<std::uint8_t>(offset & 0xFFu);
    response[7] = count;
    std::size_t cursor = 8;
    for (std::size_t index = 0; index < count; ++index, cursor += 5)
    {
        const std::uint16_t adc = static_cast<std::uint16_t>(0x1200u + index);
        response[cursor] = static_cast<std::uint8_t>(adc >> 8);
        response[cursor + 1] = static_cast<std::uint8_t>(adc & 0xFFu);
        response[cursor + 2] = static_cast<std::uint8_t>(travel[index] >> 8);
        response[cursor + 3] = static_cast<std::uint8_t>(travel[index] & 0xFFu);
        response[cursor + 4] = static_cast<std::uint8_t>(0x80u + index);
    }
    return response;
}
}

int main()
{
    using namespace hex80;

    static_assert(kTotalSlots == 104);
    static_assert(kChunkSize == 4);
    static_assert(kSlotToHid.size() == kTotalSlots);
    static_assert(MappedKeyCount() == 82);

    assert(IsKnownProductId(0x1176));
    assert(IsKnownProductId(0x1177));
    assert(IsKnownProductId(0x1250));
    assert(!IsKnownProductId(0x1109));

    assert(kSlotToHid[36] == 0x1A); // W
    assert(kSlotToHid[52] == 0x04); // A
    assert(kSlotToHid[53] == 0x16); // S
    assert(kSlotToHid[54] == 0x07); // D
    assert(kSlotToHid[90] == 0x2C); // Space
    assert(kSlotToHid[95] == 0);    // vendor Fn 0x409 is intentionally omitted
    assert(kSlotToHid[102] == 0 && kSlotToHid[103] == 0);

    const auto finish = BuildCalibrationFinishPayload();
    assert(finish[0] == kSetValue && finish[1] == kCustomCommand && finish[2] == kCalibrationFinish);
    const auto infoRequest = BuildTravelInfoPayload();
    assert(infoRequest[0] == kGetValue && infoRequest[1] == kCustomCommand && infoRequest[2] == kTravelInfo);
    const auto chunkRequest = BuildTravelBufferPayload(100, 4);
    assert(chunkRequest[0] == kGetValue && chunkRequest[1] == kCustomCommand && chunkRequest[2] == kTravelBuffer);
    assert(chunkRequest[5] == 0 && chunkRequest[6] == 100 && chunkRequest[7] == 4);

    assert(NormalizeTravelToMilli(0, 3300) == 0);
    assert(NormalizeTravelToMilli(8, 3300) == 0);
    assert(NormalizeTravelToMilli(3300, 3300) == 1000);
    assert(NormalizeTravelToMilli(4000, 3300) == 1000);
    assert(NormalizeTravelToMilli(1654, 3300) == 500);
    assert(NormalizeTravelToMilli(100, 8) == 0);

    auto info = MakeTravelInfo(3300);
    std::uint16_t travelMax = 0;
    assert(DecodeTravelInfo(info.data(), info.size(), travelMax));
    assert(travelMax == 3300);

    std::array<std::uint8_t, kPayloadBytes + 1> prefixed{};
    std::copy(info.begin(), info.end(), prefixed.begin() + 1);
    assert(DecodeTravelInfo(prefixed.data(), prefixed.size(), travelMax));
    assert(travelMax == 3300);

    info[3] = 0;
    info[4] = 64;
    assert(!DecodeTravelInfo(info.data(), info.size(), travelMax));

    const std::array<std::uint16_t, kChunkSize> values{{ 0, 8, 1654, 3300 }};
    auto chunk = MakeChunk(52, 4, values);
    std::array<TravelEntry, kChunkSize> entries{};
    std::size_t count = 0;
    assert(DecodeTravelChunk(chunk.data(), chunk.size(), 52, 4, 3300, entries, count));
    assert(count == 4);
    assert(entries[0].slot == 52 && entries[0].hid == 0x04 && entries[0].milli == 0);
    assert(entries[1].slot == 53 && entries[1].hid == 0x16 && entries[1].milli == 0);
    assert(entries[2].slot == 54 && entries[2].hid == 0x07 && entries[2].milli == 500);
    assert(entries[3].slot == 55 && entries[3].hid == 0x09 && entries[3].milli == 1000);
    assert(entries[2].adc == 0x1202 && entries[2].status == 0x82);

    assert(!DecodeTravelChunk(chunk.data(), chunk.size(), 48, 4, 3300, entries, count));
    chunk[7] = 3;
    assert(!DecodeTravelChunk(chunk.data(), chunk.size(), 52, 4, 3300, entries, count));
    chunk = MakeChunk(52, 4, values);
    assert(!DecodeTravelChunk(chunk.data(), 20, 52, 4, 3300, entries, count));
    chunk[8 + 2] = 0xFF;
    chunk[8 + 3] = 0xFF;
    assert(!DecodeTravelChunk(chunk.data(), chunk.size(), 52, 4, 3300, entries, count));

    std::cout << "hex80 protocol tests passed: 104 slots, 82 mapped HID keys\n";
    return 0;
}
