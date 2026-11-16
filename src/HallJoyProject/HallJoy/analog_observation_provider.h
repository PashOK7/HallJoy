#pragma once
#include <algorithm>
#include <cmath>
#include "analog_observation_snapshot.h"
#include "analog_provider_v2.h"
#include "halljoy_dense_snapshot.h"

namespace halljoy::observation {
// Caller supplies a validated immutable capture. Standard and extended source
// namespaces remain explicit; no merged dense value substitutes for a device.
template<class Put> void VisitProviderObservations(
    const analog_provider_v2::AnalogSnapshotHeaderV2& header,
    const analog_provider_v2::AnalogDeviceV2* devices,
    const analog_provider_v2::AnalogSampleV2* samples,Put put) {
    using namespace analog_provider_v2;
    for(std::size_t i=0;i<header.deviceCount;++i) {
        const auto& device=devices[i];if(!(device.flags & AnalogDeviceFlag_Connected)) continue;
        SnapshotValues values{};SnapshotValidity valid;
        for(std::size_t j=0;j<header.sampleCount;++j) {
            const auto& sample=samples[j];if(sample.deviceIndex!=i) continue;
            const auto& key=sample.key;const auto code=key.usage;
            const bool bindable=(key.keyNamespace==KeyNamespace::UsbHidUsage && key.usagePage==7 && code>0 && code<256) ||
                (key.keyNamespace==KeyNamespace::UapExtended && code>=256 && code<keycode::kCount);
            if(!bindable) continue;
            const bool available=(sample.flags & (AnalogSampleFlag_Owned|AnalogSampleFlag_ValueValid|AnalogSampleFlag_Fresh)) ==
                (AnalogSampleFlag_Owned|AnalogSampleFlag_ValueValid|AnalogSampleFlag_Fresh);
            valid.set(code,available);
            if(available) values[code]=static_cast<std::uint16_t>(std::clamp(std::lround(sample.value.normalized*1000.0f),0l,1000l));
        }
        put(device.deviceId,values,header.sampleTimestampUs,0,&valid);
    }
}
template<class Put> void VisitDenseObservations(const HallJoyDenseSnapshot::DeviceV1* devices,
                                                unsigned count,std::uint64_t timestamp,Put put) {
    for(unsigned i=0;i<count;++i) {
        const auto& device=devices[i];if(!(device.flags & HallJoyDenseSnapshot::DeviceFlag_Connected)) continue;
        SnapshotValues values{};SnapshotValidity valid;
        for(unsigned hid=1;hid<256;++hid) {
            valid.set(hid);values[hid]=static_cast<std::uint16_t>(std::clamp(std::lround(device.values[hid]*1000.0f),0l,1000l));
        }
        const unsigned instance=(device.flags & HallJoyDenseSnapshot::DeviceFlag_DuplicateSafeId) ? i+1 : 0;
        put(device.deviceId,values,device.timestampUs ? device.timestampUs : timestamp,instance,&valid);
    }
}
}
