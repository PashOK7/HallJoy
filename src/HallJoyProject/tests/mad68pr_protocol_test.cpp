#include "../HallJoy/mad68pr_protocol.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <set>

namespace
{
std::array<std::uint8_t, mad68pr::kPayloadBytes> MakeNormalAck(
    std::uint8_t opcode,
    std::uint8_t xorKey,
    std::initializer_list<std::uint8_t> payload)
{
    std::array<std::uint8_t, mad68pr::kPayloadBytes> packet{};
    packet[0] = mad68pr::kNormalResponseHeader;
    packet[1] = opcode;
    packet[2] = xorKey;
    packet[4] = static_cast<std::uint8_t>(payload.size());
    std::size_t at = 8;
    for (std::uint8_t byte : payload) packet[at++] = byte;

    std::uint32_t sum = 0;
    for (std::size_t i = 4; i < 8 + payload.size(); ++i) sum += packet[i];
    packet[3] = static_cast<std::uint8_t>(sum & 0xFFu);
    if (xorKey != 0)
    {
        for (std::size_t i = 3; i < 8 + payload.size(); ++i) packet[i] ^= xorKey;
    }
    return packet;
}
}

int main()
{
    using namespace mad68pr;

    static_assert(kKeyDescriptors.size() == 68);
    std::set<std::uint32_t> descriptors;
    std::set<std::uint16_t> publishedHids;
    std::size_t fnCount = 0;
    for (std::size_t key = 0; key < kKeyDescriptors.size(); ++key)
    {
        const auto& d = kKeyDescriptors[key];
        const std::uint32_t packed =
            (static_cast<std::uint32_t>(d.bytes[0]) << 16) |
            (static_cast<std::uint32_t>(d.bytes[1]) << 8) |
            d.bytes[2];
        assert(descriptors.insert(packed).second);
        assert(KeyIndexFromDescriptor(d.bytes.data()) == static_cast<int>(key));
        assert(std::string(KeyName(key)) == d.name);

        if (d.hid == 0)
        {
            ++fnCount;
            assert(std::string(d.name) == "Fn");
        }
        else
        {
            assert(publishedHids.insert(d.hid).second);
            assert(KeyIndexFromHid(d.hid) == static_cast<int>(key));
            assert(IsPublishedHid(d.hid));
        }

        std::array<std::uint8_t, kPayloadBytes> sample{};
        sample[0] = kStreamHeader;
        sample[1] = d.bytes[0];
        sample[2] = d.bytes[1];
        sample[3] = d.bytes[2];
        sample[4] = 0x03;
        sample[5] = 0x20; // 800
        sample[10] = 0x7F;
        sample[14] = 0x01;
        sample[15] = 0x5E; // 350
        sample[18] = 0x09;
        sample[19] = 0x73;
        KeySample decoded{};
        assert(DecodeKeySample(sample.data(), sample.size(), decoded));
        assert(decoded.keyIndex == key);
        assert(decoded.scannerSlot == d.scannerSlot);
        assert(decoded.internalId == d.internalId);
        assert(decoded.hid == d.hid);
        assert(decoded.raw == 800 && decoded.milli == 500);
        assert(decoded.threshold == 350 && decoded.baseline == 0x0973);
        assert(decoded.state == 0x7F);

        sample[4] = 0x06;
        sample[5] = 0x40;
        assert(DecodeKeySample(sample.data(), sample.size(), decoded));
        assert(decoded.raw == 1600 && decoded.milli == 1000);
        sample[5] = 0x41;
        assert(!DecodeKeySample(sample.data(), sample.size(), decoded));
    }
    assert(fnCount == 1);
    assert(publishedHids.size() == 67);
    assert(!IsPublishedHid(0));

    assert(IsWasdHid(0x1A));
    assert(IsWasdHid(0x04));
    assert(IsWasdHid(0x16));
    assert(IsWasdHid(0x07));
    assert(!IsWasdHid(0x08));

    // Idle Hall noise (2..5 in the hardware log) must not be accepted as a
    // digital press. A meaningful post-edge update or threshold crossing must.
    assert(!AnalogTransitionMatchesDigital(true, 350, 5, 5));
    assert(!AnalogTransitionMatchesDigital(true, 350, 5, 12));
    assert(AnalogTransitionMatchesDigital(true, 350, 5, 13));
    assert(AnalogTransitionMatchesDigital(true, 350, 5, 350));
    assert(!AnalogTransitionMatchesDigital(false, 350, 800, 800));
    assert(AnalogTransitionMatchesDigital(false, 350, 800, 100));
    assert(AnalogTransitionMatchesDigital(false, 350, 800, 790));

    // Steady-state fallback proof rejects idle noise and same-side motion, but
    // accepts a meaningful threshold crossing in either direction.
    assert(!IsPostSweepAnalogProof(5, 12, 350));
    assert(!IsPostSweepAnalogProof(165, 285, 350));
    assert(IsPostSweepAnalogProof(285, 474, 350));
    assert(IsPostSweepAnalogProof(800, 100, 350));
    assert(!IsPostSweepAnalogProof(340, 355, 350)); // crossing, but too small/noisy
    assert(!IsPostSweepAnalogProof(0, 1600, 0));

    const auto normal = MakeZeroPayloadRequest(kArmOpcode);
    assert(normal[0] == 0x55 && normal[1] == 0xA8);
    for (std::size_t i = 2; i < normal.size(); ++i) assert(normal[i] == 0);

    const auto encrypted = MakeZeroPayloadRequest(kArmOpcode, kNormalRequestHeader, 0x5A);
    assert(encrypted[0] == 0x55 && encrypted[1] == 0xA8 && encrypted[2] == 0x5A);
    for (std::size_t i = 3; i <= 7; ++i) assert(encrypted[i] == 0x5A);

    const auto rawRequest = MakeZeroPayloadRequest(kRestoreInputOpcode, kRawRequestHeader);
    assert(rawRequest[0] == 0x5F && rawRequest[1] == 0xA9);

    std::array<std::uint8_t, kPayloadBytes> unknown{};
    unknown[0] = kStreamHeader;
    unknown[1] = 0xDE;
    unknown[2] = 0xAD;
    unknown[3] = 0xBE;
    KeySample decoded{};
    assert(!DecodeKeySample(unknown.data(), unknown.size(), decoded));

    auto ack = MakeNormalAck(kArmOpcode, 0x00, {});
    auto response = DecodeControlResponse(ack.data(), ack.size(), kNormalRequestHeader, kArmOpcode);
    assert(response.kind == ControlResponseKind::Valid);

    ack = MakeNormalAck(kArmOpcode, 0x5A, {0x11, 0x22, 0x33});
    response = DecodeControlResponse(ack.data(), ack.size(), kNormalRequestHeader, kArmOpcode);
    assert(response.kind == ControlResponseKind::Valid && response.xorKey == 0x5A);
    assert(response.length == 3);

    ack[1] = kRestoreInputOpcode;
    response = DecodeControlResponse(ack.data(), ack.size(), kNormalRequestHeader, kArmOpcode);
    assert(response.kind == ControlResponseKind::Invalid);

    ack = MakeNormalAck(kArmOpcode, 0x00, {0x44});
    ack[3] ^= 1;
    response = DecodeControlResponse(ack.data(), ack.size(), kNormalRequestHeader, kArmOpcode);
    assert(response.kind == ControlResponseKind::Invalid);

    ack.fill(0);
    ack[0] = kChecksumErrorHeader;
    ack[1] = kArmOpcode;
    response = DecodeControlResponse(ack.data(), ack.size(), kNormalRequestHeader, kArmOpcode);
    assert(response.kind == ControlResponseKind::ChecksumError);

    ack.fill(0);
    ack[0] = kRawRequestHeader;
    ack[1] = kRestoreInputOpcode;
    response = DecodeControlResponse(ack.data(), ack.size(), kRawRequestHeader, kRestoreInputOpcode);
    assert(response.kind == ControlResponseKind::Valid);
    response = DecodeControlResponse(ack.data(), ack.size(), kRawRequestHeader, kArmOpcode);
    assert(response.kind == ControlResponseKind::Invalid);

    std::cout << "mad68pr full protocol tests passed: 68 descriptors, 67 HID keys\n";
    return 0;
}
