#include "../HallJoy/aula_w669_protocol.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

int main()
{
    using namespace aula_w669;
    const auto deviceRequest = BuildDeviceInfoRequest();
    assert(deviceRequest[0] == 1 && deviceRequest[1] == 0x0d);

    const auto deviceInfoReport = [](const char* product) {
        Report report{};
        const std::string csv = std::string("board,chip,variant,region,") +
            product + ",V3_17_08";
        assert(csv.size() + 6u <= report.size());
        report[0] = 1;
        report[1] = 0x0d;
        report[2] = 0;
        report[4] = 0;
        report[5] = static_cast<std::uint8_t>(5u + csv.size());
        std::memcpy(report.data() + 6u, csv.data(), csv.size());
        return report;
    };
    auto identityReport = deviceInfoReport("SI2828KZHEARGB");
    DeviceInfo deviceInfo{};
    assert(DecodeDeviceInfo(identityReport.data(), identityReport.size(),
        &deviceInfo));
    assert(std::strcmp(deviceInfo.product.data(), "SI2828KZHEARGB") == 0);
    assert(FactoryProfileForProduct(deviceInfo.product.data()) ==
        FactoryLayoutProfile::Si2828Win68);
    assert(FactoryProfileForProduct(" si2825kr-aheargb ") ==
        FactoryLayoutProfile::Si2825Win60);
    assert(FactoryProfileForProduct("SI2851UKKZHEARGB") ==
        FactoryLayoutProfile::Si2851KpTe153Uk);
    assert(FactoryProfileForProduct("UNKNOWN-W669") ==
        FactoryLayoutProfile::Unknown);
    identityReport[2] = 1;
    assert(!DecodeDeviceInfo(identityReport.data(), identityReport.size(),
        &deviceInfo));

    const auto infoRequest = BuildTravelInfoRequest();
    assert(infoRequest[0] == 1 && infoRequest[1] == 0x21 && infoRequest[6] == 4);

    Report info{};
    info[0] = 1; info[1] = 0x21; info[5] = 6; info[6] = 4;
    info[7] = 0x54; info[8] = 1; info[9] = 1; info[10] = 1; info[11] = 8;
    TravelInfo decoded{};
    assert(DecodeTravelInfo(info.data(), info.size(), &decoded));
    assert(decoded.maximum == 340 && decoded.unitCode == 1 && decoded.formatCode == 8);
    assert(ToMilli(170, 340) == 500);

    Report live{};
    live[0] = 1; live[1] = 0x21; live[5] = 5; live[6] = 1;
    live[7] = 5; live[8] = 21; live[9] = 0x34; live[10] = 0x01;
    LiveEvent event{};
    assert(DecodeLiveEvent(live.data(), live.size(), &event));
    assert(event.row == 5 && event.column == 21 && event.travel == 0x134);
    live[6] = 5;
    assert(!DecodeLiveEvent(live.data(), live.size(), &event));

    PositionToHid map = Win60FactoryMap();
    assert(MappedKeyCount(map) == 61);
    assert(map[22] == 0x29 && map[45] == 0x14 && map[68] == 0x04);
    assert(map[116] == 0x2c && map[122] == 0xfa);

    const auto win68 = Win68FactoryMap();
    assert(MappedKeyCount(win68) == 68);
    assert(win68[38] == 0x49 && win68[60] == 0x4c);
    assert(win68[103] == 0x52 && win68[124] == 0x50 &&
        win68[125] == 0x51 && win68[126] == 0x4f);
    assert(win68[121] == 0xfa && win68[122] == 0xe4);

    const auto kpTe153 = KpTe153UkFactoryMap();
    assert(MappedKeyCount(kpTe153) == 69);
    assert(kpTe153[38] == 0x4a && kpTe153[58] == 0x28);
    assert(kpTe153[79] == 0x32 && kpTe153[89] == 0x64 &&
        kpTe153[99] == 0x87);
    assert(FactoryMap(FactoryLayoutProfile::Unknown) == PositionToHid{});
    std::array<bool, 10> fragments{};
    Report mapPacket{};
    mapPacket[0] = 1; mapPacket[1] = 0x18; mapPacket[2] = 0x80;
    mapPacket[4] = 9; mapPacket[5] = 24;
    mapPacket[6] = 1; mapPacket[7] = 4;
    assert(DecodeKeyMapFragment(mapPacket.data(), mapPacket.size(), &map, &fragments));
    assert(map[126] == 4 && fragments[9]);

    // The physical log from an unmodified WIN60 contains all-zero default
    // records and one explicit 01 FA record for Fn.  Zeros must preserve the
    // factory map instead of erasing all 60 ordinary keys.
    map = Win60FactoryMap(); fragments.fill(false);
    for (std::uint8_t fragment = 0; fragment < 10; ++fragment)
    {
        Report physicalPacket{};
        physicalPacket[0] = 1; physicalPacket[1] = 0x18;
        physicalPacket[2] = 0x80; physicalPacket[4] = fragment;
        physicalPacket[5] = fragment < 9 ? 56 : 24;
        if (fragment == 8)
        {
            physicalPacket[46] = 1;
            physicalPacket[47] = 0xfa;
        }
        assert(DecodeKeyMapFragment(physicalPacket.data(), physicalPacket.size(),
            &map, &fragments));
    }
    assert(std::all_of(fragments.begin(), fragments.end(), [](bool v) { return v; }));
    assert(MappedKeyCount(map) == 61);
    assert(map[22] == 0x29 && map[122] == 0xfa);

    std::array<std::uint8_t, kColumns> mask{};
    mask[3] = 0x15;
    const auto subscribe = BuildSubscriptionRequest(mask);
    assert(subscribe[5] == 0x18 && subscribe[6] == 2 && subscribe[10] == 0x15);
    Report poll{}; poll[0] = 1; poll[1] = 0x21; poll[5] = 2; poll[6] = 9; poll[7] = 8;
    std::uint8_t pollCode = 0; std::uint16_t pollHz = 0;
    assert(DecodePollRate(poll.data(), poll.size(), &pollCode, &pollHz));
    assert(pollCode == 8 && pollHz == 8000);
    poll[7] = 0; pollCode = 0xff; pollHz = 0xffff;
    assert(DecodePollRate(poll.data(), poll.size(), &pollCode, &pollHz));
    assert(pollCode == 0 && pollHz == 0);

    // Every decoder must reject truncated/arbitrary reports without touching
    // memory outside the supplied span. Sanitizer builds execute this loop.
    std::uint32_t state = 0x66931708u;
    for (std::size_t length = 0; length <= 64; ++length)
    {
        Report fuzz{};
        for (auto& byte : fuzz)
        {
            state = state * 1664525u + 1013904223u;
            byte = static_cast<std::uint8_t>(state >> 24);
        }
        TravelInfo fuzzInfo{}; LiveEvent fuzzLive{};
        DeviceInfo fuzzDevice{};
        PositionToHid fuzzMap{}; std::array<bool, 10> fuzzFragments{};
        std::array<std::uint16_t, kColumns> fuzzRow{};
        std::uint8_t fuzzCode = 0; std::uint16_t fuzzHz = 0;
        (void)DecodeDeviceInfo(fuzz.data(), length, &fuzzDevice);
        (void)DecodeTravelInfo(fuzz.data(), length, &fuzzInfo);
        (void)DecodeLiveEvent(fuzz.data(), length, &fuzzLive);
        (void)DecodeKeyMapFragment(fuzz.data(), length, &fuzzMap, &fuzzFragments);
        (void)DecodeSnapshotPacket(fuzz.data(), length, &fuzzRow);
        (void)DecodePollRate(fuzz.data(), length, &fuzzCode, &fuzzHz);
    }
    return 0;
}
