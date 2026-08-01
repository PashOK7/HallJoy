#include "aula_win60he_protocol.h"
#include "hex80_protocol.h"
#include "mad68pr_protocol.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace
{
std::uint64_t g_state = 0x6a09e667f3bcc909ull;

std::uint32_t Next() noexcept
{
    g_state ^= g_state << 13;
    g_state ^= g_state >> 7;
    g_state ^= g_state << 17;
    return static_cast<std::uint32_t>(g_state ^ (g_state >> 32));
}

std::size_t MakeAulaSeed(
    std::array<std::uint8_t, 320>& bytes,
    std::uint8_t& command) noexcept
{
    bytes.fill(0);
    command = aula_win60he::kCommandApi;
    constexpr std::array<std::uint8_t, 7> payload{{
        0, aula_win60he::kOrderPrecisionStroke, 10, 10, 0, 0x48, 0x0D,
    }};
    bytes[0] = aula_win60he::kFrameHead;
    bytes[1] = static_cast<std::uint8_t>(payload.size());
    bytes[2] = aula_win60he::ResponseCommand(command);
    bytes[3] = aula_win60he::ComputeChecksum(
        bytes[1], bytes[2], payload.data());
    std::copy(payload.begin(), payload.end(), bytes.begin() + 4u);
    return aula_win60he::kWireReportBytes;
}

std::size_t MakeHexSeed(std::array<std::uint8_t, 320>& bytes) noexcept
{
    bytes.fill(0);
    bytes[0] = hex80::kGetValue;
    bytes[1] = hex80::kCustomCommand;
    bytes[2] = hex80::kTravelBuffer;
    bytes[6] = 52;
    bytes[7] = 4;
    for (std::size_t index = 0; index < hex80::kChunkSize; ++index)
    {
        const std::size_t at = 8u + index * 5u;
        bytes[at] = 0x12;
        bytes[at + 1u] = static_cast<std::uint8_t>(index);
        const auto travel = static_cast<std::uint16_t>(index * 1000u);
        bytes[at + 2u] = static_cast<std::uint8_t>(travel >> 8u);
        bytes[at + 3u] = static_cast<std::uint8_t>(travel);
        bytes[at + 4u] = static_cast<std::uint8_t>(0x80u + index);
    }
    return hex80::kPayloadBytes;
}

std::size_t MakeMadSeed(std::array<std::uint8_t, 320>& bytes) noexcept
{
    bytes.fill(0);
    const auto& descriptor = mad68pr::kKeyDescriptors[0];
    bytes[0] = mad68pr::kStreamHeader;
    std::copy(descriptor.bytes.begin(), descriptor.bytes.end(), bytes.begin() + 1u);
    bytes[4] = 0x03;
    bytes[5] = 0x20;
    bytes[14] = 0x01;
    bytes[15] = 0x5E;
    return mad68pr::kPayloadBytes;
}
}

int main()
{
    constexpr std::size_t kIterations = 250000;
    std::array<std::uint8_t, 320> bytes{};
    std::uint64_t accepted = 0;

    for (std::size_t iteration = 0; iteration < kIterations; ++iteration)
    {
        for (auto& byte : bytes)
            byte = static_cast<std::uint8_t>(Next());
        std::size_t size = Next() % (bytes.size() + 1u);
        std::uint8_t command = static_cast<std::uint8_t>(Next());

        // Half of the corpus starts from a known-good packet and applies a
        // small deterministic mutation. This drives both successful decoders
        // and their near-valid rejection paths; pure random bytes almost never
        // have a correct header, command and checksum at the same time.
        const std::uint32_t seedKind = Next() % 6u;
        if (seedKind < 3u)
        {
            if (seedKind == 0u)
                size = MakeAulaSeed(bytes, command);
            else if (seedKind == 1u)
                size = MakeHexSeed(bytes);
            else
                size = MakeMadSeed(bytes);

            const std::size_t mutations = (iteration % 32u == 0u)
                ? 0u
                : 1u + Next() % 6u;
            for (std::size_t mutation = 0; mutation < mutations; ++mutation)
                bytes[Next() % size] ^= static_cast<std::uint8_t>(1u << (Next() % 8u));
            if (mutations != 0u && (Next() & 3u) == 0u)
                size = Next() % (size + 1u);
        }

        aula_win60he::FrameView frame{};
        if (aula_win60he::ParseResponseFrame(bytes.data(), size, command, &frame))
        {
            const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
            const auto end = begin + size;
            const auto payload = reinterpret_cast<std::uintptr_t>(frame.payload);
            if (payload < begin || payload > end || frame.payloadBytes > end - payload)
                return 1;
            ++accepted;

            aula_win60he::SyncInfo sync{};
            aula_win60he::PrecisionStroke precision{};
            aula_win60he::KeyMap keyMap{};
            aula_win60he::KeyQuery query{};
            aula_win60he::KeyFunctionBatch functions{};
            aula_win60he::TravelHalf travel{};
            aula_win60he::StatusMatrix status{};
            (void)aula_win60he::DecodeSyncInfo(frame, &sync);
            (void)aula_win60he::DecodePrecisionStroke(frame, &precision);
            (void)aula_win60he::DecodeDefaultKeyRows(frame, 0, 1, &keyMap);
            (void)aula_win60he::DecodeKeyFunctionReadResponse(frame, query, 0, &functions);
            (void)aula_win60he::DecodeTravelHalf(frame, &travel);
            (void)aula_win60he::DecodeStatusMatrix(frame, &status);
        }

        aula_win60he::Report report{};
        (void)aula_win60he::DecodeHidReport(bytes.data(), size, &report);
        aula_win60he::ResponseStream flattened{};
        std::size_t flattenedBytes = 0;
        (void)aula_win60he::FlattenHidReports(
            bytes.data(), Next() % 5u, Next() % 70u, &flattened, &flattenedBytes);

        std::uint16_t travelMax = 0;
        (void)hex80::DecodeTravelInfo(bytes.data(), size, travelMax);
        std::array<hex80::TravelEntry, hex80::kChunkSize> entries{};
        std::size_t entryCount = 0;
        (void)hex80::DecodeTravelChunk(
            bytes.data(), size,
            static_cast<std::uint16_t>(Next()),
            static_cast<std::uint8_t>(Next()),
            static_cast<std::uint16_t>(Next()),
            entries, entryCount);
        if (entryCount > entries.size())
            return 2;
        const auto* found = hex80::FindPayload(
            bytes.data(), size,
            static_cast<std::uint8_t>(Next()),
            static_cast<std::uint8_t>(Next()));
        accepted += found != nullptr;
        if (hex80::NormalizeTravelToMilli(
                static_cast<std::uint16_t>(Next()),
                static_cast<std::uint16_t>(Next())) > 1000u)
            return 3;

        mad68pr::KeySample sample{};
        accepted += mad68pr::DecodeKeySample(bytes.data(), size, sample);
        const auto control = mad68pr::DecodeControlResponse(
            bytes.data(), size,
            static_cast<std::uint8_t>(Next()),
            static_cast<std::uint8_t>(Next()));
        accepted += control.kind == mad68pr::ControlResponseKind::Valid;
        (void)mad68pr::KeyIndexFromDescriptor(bytes.data());
        (void)mad68pr::AnalogTransitionMatchesDigital(
            (Next() & 1u) != 0,
            static_cast<std::uint16_t>(Next()),
            static_cast<std::uint16_t>(Next()),
            static_cast<std::uint16_t>(Next()));
        (void)mad68pr::IsPostSweepAnalogProof(
            static_cast<std::uint16_t>(Next()),
            static_cast<std::uint16_t>(Next()),
            static_cast<std::uint16_t>(Next()));
    }

    std::cout << "PROTOCOL_PARSER_FUZZ_SMOKE=PASS iterations=" << kIterations
              << " accepted=" << accepted << '\n';
    return accepted == 0u ? 4 : 0;
}
