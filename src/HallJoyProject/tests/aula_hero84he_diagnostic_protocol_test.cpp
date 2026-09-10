#include "../HallJoy/aula_hero84he_diagnostic_protocol.h"

#include <array>
#include <cassert>

namespace
{
using namespace aula_hero84he_diagnostic;

void SetChecksum(Report* report)
{
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i + 1 < report->size(); ++i) sum += (*report)[i];
    report->back() = static_cast<std::uint8_t>(0xffu - (sum & 0xffu));
}

Report Response(std::uint8_t command, std::uint8_t subcommand,
    const std::uint8_t* data, std::size_t length)
{
    Report report{};
    report[0] = kReportId; report[1] = command; report[2] = subcommand;
    report[4] = 1; report[6] = static_cast<std::uint8_t>(length);
    for (std::size_t i = 0; i < length; ++i) report[7 + i] = data[i];
    SetChecksum(&report);
    return report;
}
}

int main()
{
    Report identity{};
    assert(BuildIdentityRead(&identity));
    assert(identity[0] == 0x09 && identity[1] == 0x82 && identity[2] == 0x01);
    assert(identity[4] == 1 && identity[6] == 6 && HasValidChecksum(identity));

    const std::array<std::uint16_t, 4> keys{{30, 43, 44, 45}};
    Report assignment{}, direct{};
    assert(BuildAssignmentRead(0, keys.data(), keys.size(), &assignment));
    assert(BuildDirectRead(keys.data(), keys.size(), &direct));
    assert(assignment[1] == 0x83 && assignment[6] == 8 && HasValidChecksum(assignment));
    assert(direct[1] == 0x94 && direct[2] == 0x02 && direct[6] == 8 && HasValidChecksum(direct));
    const std::array<std::uint16_t, 9> nineKeys{{1,2,3,4,5,6,7,8,9}};
    assert(BuildDirectRead(nineKeys.data(), nineKeys.size(), &direct));
    assert(direct[6] == 18 && HasValidChecksum(direct));
    assert(!BuildDirectRead(keys.data(), 0, &direct));
    assert(!BuildDirectRead(keys.data(), 10, &direct));
    const std::array<std::uint16_t, 2> duplicate{{30, 30}};
    assert(!BuildDirectRead(duplicate.data(), duplicate.size(), &direct));

    const std::array<std::uint8_t, 6> uuidData{{1, 2, 3, 4, 5, 6}};
    auto identityReply = Response(0x82, 0x01, uuidData.data(), uuidData.size());
    std::array<std::uint8_t, 6> uuid{};
    assert(ParseIdentityResponse(identityReply, &uuid) && uuid == uuidData);
    identityReply[20] ^= 1;
    assert(!ParseIdentityResponse(identityReply, &uuid));

    const std::array<std::uint8_t, 12> directData{{
        0x00,0x1E,0x80,0x02,0x01,0x00,
        0x00,0x2B,0x00,0x03,0x01,0x01}};
    auto directReply = Response(0x94, 0x02, directData.data(), directData.size());
    std::array<DirectSample, kMaxPositions> samples{};
    assert(ParseDirectResponse(directReply, keys.data(), 2, &samples));
    assert(samples[0].scannerLock && samples[0].current == 2 && samples[0].minimum == 0x100);
    assert(!ParseDirectResponse(directReply, keys.data() + 1, 2, &samples));
    directReply[7] = 0x01; SetChecksum(&directReply);
    assert(!ParseDirectResponse(directReply, keys.data(), 2, &samples));
    return 0;
}
