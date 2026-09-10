#include "../HallJoy/irok_nd75_protocol.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
    using namespace irok_nd75;

    const auto identityRequest = BuildIdentityRequest();
    assert(identityRequest[0] == 1 && identityRequest[1] == 0x0d);
    assert(std::count(identityRequest.begin() + 2, identityRequest.end(), 0) == 62);

    const auto capabilityRequest = BuildCapabilityRequest();
    assert(capabilityRequest[0] == 1 && capabilityRequest[1] == 0x29);
    assert(capabilityRequest[5] == 0x18 && capabilityRequest[6] == 0x04);

    const auto unsubscribe = BuildUnsubscribeRequest();
    assert(unsubscribe[1] == 0x29 && unsubscribe[5] == 0x18 &&
        unsubscribe[6] == 0x03);

    Report identity{};
    const char csv[] = "M484,01,KB,ABT,X86HERGB,V1.00.09";
    identity[0] = 1;
    identity[1] = 0x0d;
    identity[2] = 0;
    identity[4] = 0;
    std::memcpy(identity.data() + 6, csv, sizeof(csv));
    identity[5] = static_cast<std::uint8_t>(5 + std::strlen(csv));
    DeviceInfo info{};
    assert(DecodeDeviceInfo(identity.data(), identity.size(), &info));
    assert(std::strcmp(info.controller.data(), "M484") == 0);
    assert(std::strcmp(info.product.data(), "X86HERGB") == 0);
    assert(std::strcmp(info.firmware.data(), "V1.00.09") == 0);
    assert(irok_nd75::IsFirmwareWithinKnownFamily(info));
    assert(IsExpectedDevice(info));

    auto wrong = info;
    wrong.controller.fill(0);
    std::copy_n("M483", 4, wrong.controller.begin());
    assert(!IsExpectedDevice(wrong));
    wrong = info;
    wrong.product.fill(0);
    std::copy_n("X82HERGB", 8, wrong.product.begin());
    assert(!IsExpectedDevice(wrong));
    wrong = info;
    wrong.firmware.fill(0);
    std::copy_n("V2.00.00", 9, wrong.firmware.data());
    assert(!irok_nd75::IsFirmwareWithinKnownFamily(wrong));
    identity[2] = 1;
    assert(!DecodeDeviceInfo(identity.data(), identity.size(), &info));

    Report capability{};
    capability[0] = 1;
    capability[1] = 0x21;
    capability[5] = 2;
    capability[6] = 4;
    capability[7] = 20;
    CapabilityInfo decodedCapability{};
    assert(DecodeCapabilityInfo(capability.data(), capability.size(),
        &decodedCapability));
    assert(decodedCapability.sensitivity == 20);
    capability[7] = 41;
    assert(!DecodeCapabilityInfo(capability.data(), capability.size(),
        &decodedCapability));

    Report live{};
    live[0] = 1;
    live[1] = 0x21;
    live[5] = 4;
    live[6] = 1;
    live[7] = 5;
    live[8] = 15;
    live[9] = 40;
    LiveEvent event{};
    assert(DecodeLiveEvent(live.data(), live.size(), &event));
    assert(event.row == 5 && event.column == 15 && event.travel == 40);
    assert(ToMilli(0) == 0 && ToMilli(20) == 500 && ToMilli(40) == 1000);
    assert(ToMilli(255) == 1000);
    assert(ClassifyReadFailure(false, true, false, 0) ==
        ReadFailureAction::Continue);
    assert(ClassifyReadFailure(false, false, false, 1) ==
        ReadFailureAction::Continue);
    assert(ClassifyReadFailure(false, false, false, 2) ==
        ReadFailureAction::Continue);
    assert(ClassifyReadFailure(false, false, false, 3) ==
        ReadFailureAction::EndSession);
    assert(ClassifyReadFailure(false, false, true, 1) ==
        ReadFailureAction::EndSession);
    assert(ClassifyReadFailure(true, true, false, 0) ==
        ReadFailureAction::EndSession);
    live[7] = 6;
    assert(!DecodeLiveEvent(live.data(), live.size(), &event));
    live[7] = 0;
    live[8] = 22;
    assert(!DecodeLiveEvent(live.data(), live.size(), &event));

    const auto map = FactoryMap();
    assert(map.size() == 132);
    assert(MappedKeyCount(map) == 81);
    assert(map[0] == 0x29 && map[2] == 0x3a && map[15] == 0x49);
    assert(map[22] == 0x35 && map[36] == 0x2a && map[37] == 0x4c);
    assert(map[58] == 0x31 && map[59] == 0x4b);
    assert(map[80] == 0x28 && map[81] == 0x4e);
    assert(map[102] == 0x52 && map[110] == 0xe0 && map[120] == 0xfa);
    assert(map[123] == 0x50 && map[124] == 0x51 && map[125] == 0x4f);

    std::uint64_t hash = 1469598103934665603ull;
    for (const auto value : map)
    {
        hash ^= value;
        hash *= 1099511628211ull;
    }
    assert(hash == 0x4bef9fcf48e36c37ull);

    const auto mask = SubscriptionMask(map);
    const std::array<std::uint8_t, kColumns> expectedMask{
        0x3f, 0x26, 0x3f, 0x1f, 0x1f, 0x1f, 0x3e, 0x1f, 0x1f, 0x3f, 0x3f,
        0x1f, 0x3f, 0x21, 0x3f, 0x2f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    assert(mask == expectedMask);
    const auto subscribe = BuildSubscriptionRequest(mask);
    assert(subscribe[1] == 0x29 && subscribe[5] == 0x18 &&
        subscribe[6] == 0x02);
    assert(std::equal(mask.begin(), mask.end(), subscribe.begin() + 7));

    std::uint32_t state = 0x04167372u;
    for (std::size_t length = 0; length <= kReportBytes; ++length)
    {
        Report fuzz{};
        for (auto& byte : fuzz)
        {
            state = state * 1664525u + 1013904223u;
            byte = static_cast<std::uint8_t>(state >> 24);
        }
        DeviceInfo fuzzIdentity{};
        CapabilityInfo fuzzCapability{};
        LiveEvent fuzzEvent{};
        (void)DecodeDeviceInfo(fuzz.data(), length, &fuzzIdentity);
        (void)DecodeCapabilityInfo(fuzz.data(), length, &fuzzCapability);
        (void)DecodeLiveEvent(fuzz.data(), length, &fuzzEvent);
    }
    return 0;
}
