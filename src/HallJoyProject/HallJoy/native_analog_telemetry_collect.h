#pragma once
#include "backend.h"
#include "native_analog_backend_registry.h"
#include <algorithm>
#include <cstring>
#include <iterator>

// Callbacks may be sampled at different instants. Preserve observations and
// availability; never invent an atomic device epoch across unrelated backends.
template<class Descriptor, class Telemetry, class Lifecycle>
void CollectNativeAnalogTelemetry(BackendAnalogTelemetry& t, bool diagnostic,
    std::size_t count, Descriptor descriptorAt, Telemetry telemetryAt, Lifecycle lifecycleAt)
{
    t.nativeCatalogCount = static_cast<std::uint32_t>(count);
    const auto limit = (std::min)(count, std::size(t.nativeProtocols));
    for (std::size_t i=0; i<limit; ++i) {
        ++t.nativeVisitedCount;
        const auto* d=descriptorAt(i);
        NativeAnalogBackendTelemetry n{};
        const bool available=d && telemetryAt(i, &n);
        if (!available) ++t.nativeTelemetryFailures;
        if (available && n.connected) ++t.nativeConnectedCount;
        if (!diagnostic && (!available || (!n.present && !n.connected))) continue;
        auto& dst=t.nativeProtocols[t.nativeProtocolCount++];
        dst.catalogIndex=static_cast<std::uint32_t>(i);
        dst.telemetryAvailable=available;
        if (d) {
            dst.protocol=static_cast<std::uint16_t>(d->protocol);
            dst.flags=d->flags;
            strncpy_s(dst.id,d->id,_TRUNCATE);
            wcsncpy_s(dst.name,n.deviceName[0]?n.deviceName:d->displayName,_TRUNCATE);
        }
        dst.verifiedLayoutToken=n.verifiedLayoutToken;
        dst.present=n.present; dst.connected=n.connected;
        dst.genericProtocol=n.connected && n.genericProtocol;
        dst.vendorId=n.vendorId; dst.productId=n.productId;
        dst.usagePage=n.usagePage; dst.usage=n.usage;
        dst.mappedKeys=n.mappedKeys; dst.activeKeys=n.activeKeys;
        dst.nominalRawLevels=n.nominalRawLevels;
        dst.inputReportBytes=n.inputReportBytes; dst.outputReportBytes=n.outputReportBytes;
        dst.updateHz10=n.updateHz10; dst.averageIntervalUs=n.averageIntervalUs;
        dst.maximumIntervalUs=n.maximumIntervalUs; dst.lastUpdateAgeMs=n.lastUpdateAgeMs;
        dst.successfulUpdates=n.successfulUpdates; dst.failedUpdates=n.failedUpdates;
        wcsncpy_s(dst.status,n.status,_TRUNCATE);
        if (diagnostic) {
            NativeAnalogBackendLifecycleSnapshot life{};
            dst.lifecycleAvailable=lifecycleAt(i,&life);
            if (dst.lifecycleAvailable) {
                dst.lifecycleState=static_cast<std::uint32_t>(life.state);
                dst.generation=life.generation.Value();
                dst.lifecycleError=static_cast<std::uint32_t>(life.lastError.code);
                dst.lifecycleOperation=static_cast<std::uint32_t>(life.lastError.operation);
                dst.nativeError=life.lastError.native_error;
            }
        }
    }
    t.nativeTelemetryComplete=t.nativeVisitedCount==count && t.nativeTelemetryFailures==0;
}
