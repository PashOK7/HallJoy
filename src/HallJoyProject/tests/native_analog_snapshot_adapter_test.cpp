#include "native_analog_snapshot_adapter.h"

#include <array>
#include <cassert>
#include <cstdio>

int main()
{
    using namespace halljoy;
    using namespace analog_provider_v2;
    using namespace native_analog_snapshot;

    std::array<std::uint8_t, 256> ownedA{};
    std::array<std::uint16_t, 256> milliA{};
    std::array<std::uint8_t, 256> ownedB{};
    std::array<std::uint16_t, 256> milliB{};
    ownedA[0x04] = 1;
    milliA[0x04] = 250;
    ownedB[0x04] = 1;
    milliB[0x04] = 750;
    const std::uint64_t provider = StableProviderId("native-registry-v2");
    assert(provider != 0);
    const LegacyMilliSourceV1 pair[] = {
        { 0x1111, 0x1111, 0x2222, 0xFFB0, 1, 4, true, true, true, true,
          ownedA.data(), milliA.data() },
        { 0x2222, 0x1111, 0x2222, 0xFFB0, 1, 4, true, true, true, true,
          ownedB.data(), milliB.data() },
    };
    std::array<AnalogDeviceV2, 2> devices{};
    std::array<AnalogSampleV2, 2> samples{};
    AnalogSnapshotHeaderV2 header{};
    OutputV1 output{ &header, devices.data(), devices.size(), samples.data(), samples.size() };
    assert(PublishLegacyMilli(provider, 1, 9, 1000, pair, std::size(pair), output));
    assert((header.flags & AnalogSnapshotFlag_Complete) != 0);
    assert(header.deviceCount == 2 && header.sampleCount == 2);
    assert(devices[0].deviceId != devices[1].deviceId);
    assert(samples[0].key == samples[1].key);
    assert((samples[0].value.flags & AnalogValueFlag_LegacyQuantized) != 0);

    const LegacyMilliSourceV1 surviving[] = { pair[1] };
    assert(PublishLegacyMilli(provider, 1, 10, 2000, surviving, std::size(surviving), output));
    assert(header.deviceCount == 1 && header.sampleCount == 1);
    assert(devices[0].deviceId == StableDeviceId(provider, 0x2222, 0x1111, 0x2222));
    assert(samples[0].value.rawNumerator == 750 && samples[0].value.rawDomain == 1000);
    std::printf("NATIVE_ANALOG_SNAPSHOT_ADAPTER_TEST=PASS distinct_identity=1 disconnect_preserves_other=1 legacy_milli=1\n");
    return 0;
}
