#include "../HallJoy/uap_parent_snapshot.h"
#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_uap_provider_v2_projection.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace halljoy::analog_provider_v2;
using namespace halljoy::uap_parent_snapshot;
using namespace HallJoyUapProviderV2;

namespace
{
AnalogDeviceV2 MakeDevice(std::uint64_t id)
{
    AnalogDeviceV2 device{};
    device.deviceId = id;
    device.flags = AnalogDeviceFlag_Connected | AnalogDeviceFlag_StableIdentity;
    device.vendorId = 0x352d;
    device.productId = static_cast<std::uint16_t>(0x2380u + id);
    device.usagePage = 0xff00;
    device.usage = 1;
    return device;
}

HallJoyDenseSnapshot::DeviceV1 MakeDense(const AnalogDeviceV2& provider,
    std::uint64_t generation, std::uint64_t timestamp)
{
    HallJoyDenseSnapshot::DeviceV1 dense{};
    dense.structSize = sizeof(dense);
    dense.version = HallJoyDenseSnapshot::kVersion;
    dense.deviceId = provider.deviceId;
    dense.generation = generation;
    dense.timestampUs = timestamp;
    dense.flags = HallJoyDenseSnapshot::DeviceFlag_Connected;
    dense.vendorId = provider.vendorId;
    dense.productId = provider.productId;
    dense.usagePage = provider.usagePage;
    dense.usage = provider.usage;
    return dense;
}
}

int main()
{
    constexpr std::array<ProjectionKey, 4> keys{
        ProjectionKey{ IdentityFromLegacyCode(0x04), 0 },
        ProjectionKey{ IdentityFromLegacyCode(0x1a), 1 },
        ProjectionKey{ IdentityFromLegacyCode(0x65), 2 },
        ProjectionKey{ IdentityFromLegacyCode(0x409), 3 },
    };
    std::array<float, 4> first{ 0.2f, 0.8f, 0.7f, 0.4f };
    std::array<float, 4> second{ 0.6f, 0.3f, 0.1f, 0.0f };
    const auto firstDevice = MakeDevice(1);
    const auto secondDevice = MakeDevice(2);
    std::array<ProjectionDevice, 2> inputs{
        ProjectionDevice{ firstDevice, first.data(), first.size(), 1001 },
        ProjectionDevice{ secondDevice, second.data(), second.size(), 1002 },
    };

    SnapshotV1 snapshot{};
    snapshot.publicationGeneration = 17;
    snapshot.publicationTimestampUs = 1017;
    AnalogSnapshotHeaderV2 providerHeader{};
    std::array<AnalogDeviceV2, HallJoyUapProviderV2::kMaxDevices>
        providerDevices{};
    std::array<AnalogSampleV2, HallJoyUapProviderV2::kMaxSamples>
        providerSamples{};
    const ProjectionGeneration generation{ 9, 17, 17, 4, 1000, 1017 };
    assert(BuildSnapshot(keys.data(), keys.size(), inputs.data(), inputs.size(),
        inputs.size(), generation, &providerHeader,
        providerDevices.data(), providerDevices.size(),
        providerSamples.data(), providerSamples.size()) ==
        ProjectionError::None);

    snapshot.denseDeviceCount = 2;
    snapshot.denseDevices[0] = MakeDense(firstDevice, 17, 1017);
    snapshot.denseDevices[1] = MakeDense(secondDevice, 17, 1017);
    for (std::uint32_t di = 0; di < snapshot.denseDeviceCount; ++di)
    {
        auto& dense = snapshot.denseDevices[di];
        assert(ProjectCapturedDeviceOrdinaryUsbHidDense(providerHeader,
            providerSamples.data(), di, dense.values,
            HallJoyDenseSnapshot::kKeyCount));
        for (const float value : dense.values)
            dense.activeKeyCount += value > 0.0f ? 1u : 0u;
        for (std::size_t code = 0; code < snapshot.denseValues.size(); ++code)
            snapshot.denseValues[code] = (std::max)(snapshot.denseValues[code],
                dense.values[code]);
    }
    for (const float value : snapshot.denseValues)
        snapshot.denseActiveKeyCount += value > 0.0f ? 1u : 0u;

    assert(Validate(snapshot) == ValidationError::None);
    assert(snapshot.denseValues[0x04] == 0.6f);
    assert(snapshot.denseValues[0x1a] == 0.8f);
    assert(snapshot.denseValues[0x65] == 0.7f);
    assert(ValidateDualViews(providerHeader, providerDevices.data(),
        providerDevices.size(), providerSamples.data(), providerSamples.size(),
        snapshot.denseDevices.data(), snapshot.denseDeviceCount) ==
        ValidationError::None);

    auto badAggregate = snapshot;
    badAggregate.denseValues[0x04] = 0.5f;
    assert(Validate(badAggregate) == ValidationError::DenseAggregateMismatch);

    auto badDual = snapshot;
    badDual.denseDevices[0].values[0x04] = 0.1f;
    assert(ValidateDualViews(providerHeader, providerDevices.data(),
        providerDevices.size(), providerSamples.data(), providerSamples.size(),
        badDual.denseDevices.data(), badDual.denseDeviceCount) ==
        ValidationError::DualViewMismatch);

    auto denseFallback = snapshot;
    assert(Validate(denseFallback) == ValidationError::None);

    auto badValue = snapshot;
    badValue.denseValues[0x04] = std::numeric_limits<float>::quiet_NaN();
    assert(Validate(badValue) == ValidationError::InvalidDenseValue);

    std::cout << "UAP_PARENT_SNAPSHOT_TEST=PASS one_publication=1 "
                 "dense_fallback=1 aggregate_checked=1 dual_checked=1\n";
    return 0;
}
