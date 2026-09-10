#pragma once

#include <array>
#include <cstdint>
#include "analog_provider_v2.h"
#include "halljoy_plugin_telemetry.h"
#include "halljoy_uap_provider_v2.h"
#include "provider_v2_snapshot_broker.h"
#include "uap_parent_snapshot.h"
#include "wooting-analog-sdk.h"

// Starts and supervises the crash-isolated analog host. All plugin/Soup/HID code
// runs in a child process; these functions never load the SDK into HallJoy.
int AnalogHostClient_Initialise();
bool AnalogHostClient_IsInitialised();
// Coalesces a real Windows device-change event into a controlled restart of
// the isolated UAP child. This never performs enumeration or waits on the
// caller's thread; the supervisor owns termination, reap and replacement.
bool AnalogHostClient_RequestDeviceRefresh() noexcept;
WootingAnalogResult AnalogHostClient_Uninitialise();
WootingAnalogResult AnalogHostClient_SetKeycodeMode(WootingAnalog_KeycodeType mode);
int AnalogHostClient_GetConnectedDevicesInfo(WootingAnalog_DeviceInfo_FFI** buffer, unsigned int len);
float AnalogHostClient_ReadAnalog(unsigned short code);
float AnalogHostClient_ReadAnalogDevice(unsigned short code, WootingAnalog_DeviceID deviceId);
int AnalogHostClient_ReadFullBuffer(unsigned short* codeBuffer, float* analogBuffer, unsigned int len);
int AnalogHostClient_ReadFullBufferDevice(unsigned short* codeBuffer, float* analogBuffer, unsigned int len, WootingAnalog_DeviceID deviceId);


struct AnalogHostTelemetry
{
    bool available = false;
    bool ready = false;
    int status = 0;
    int initResult = 0;
    int lastError = 0;
    int transportError = 0;
    int restartCount = 0;
    int invalidSnapshotCount = 0;
    int activeKeyCount = 0;
    int denseDeviceCount = 0;
    bool providerV2PlaneDualCoherent = false;
    int providerV2DualFailureCount = 0;
    bool providerV2PlaneAvailable = false;
    bool providerV2PlaneParentReadOnly = false;
    int providerV2PlaneStatus = 0;
    int providerV2PlaneDeviceCapacity = 0;
    int providerV2PlaneSampleCapacity = 0;
    int providerV2PlaneRequiredDeviceCount = 0;
    int providerV2PlaneRequiredSampleCount = 0;
    int providerV2PlaneFailureCount = 0;
    std::uint64_t providerV2PlaneGeneration = 0;
    std::uint64_t providerV2PlaneMappingBytes = 0;
    std::uint64_t providerV2PlaneTransactionToken = 0;
    std::uint64_t snapshotGeneration = 0;
    std::uint64_t snapshotTimestampUs = 0;
    std::uint32_t hostPollHz10 = 0;
    std::uint32_t hostSuccessfulPollHz10 = 0;
    std::uint32_t lastPublishAgeMs = 0;
    std::uint64_t totalPolls = 0;
    std::uint64_t totalSuccessfulPolls = 0;
    int deviceCount = 0;
    std::array<HallJoyPluginTelemetry::DeviceV1, HallJoyPluginTelemetry::kMaxDevices> devices{};
};

bool AnalogHostClient_GetTelemetry(AnalogHostTelemetry* out);

// Copies the dense compatibility plane and optional coherent Provider V2 plane
// under one shared publication sequence. This performs no device I/O. A stable
// dense capture can remain available when the V2 label is absent or invalid.
bool AnalogHostClient_CaptureTickSnapshot(
    halljoy::uap_parent_snapshot::SnapshotV1* out);

using AnalogHostProviderV2ReadLease = halljoy::provider_v2_snapshot_broker::
    ParentSnapshotBroker::ReadLease;

// Bounded, nonblocking acquisition of a parent-owned immutable Provider V2
// snapshot. The lease pins its broker slot until the caller releases it.
AnalogHostProviderV2ReadLease
AnalogHostClient_AcquireProviderV2Snapshot() noexcept;

// Independently validates and copies only the committed variable-plane header.
// B2i-b uses this as transport evidence; production output remains dense until
// the later B2i-c migration.
bool AnalogHostClient_CaptureProviderV2PlaneHeader(
    halljoy::analog_provider_v2::AnalogSnapshotHeaderV2* out);

// Called very early by wWinMain. Returns true when this process is the child
// analog host and places its process exit code in exitCode.
bool AnalogHost_TryRunCommand(int& exitCode);

// Deletes reports from previous runs before a new HallJoy diagnostic session.
void AnalogHostClient_ResetDiagnosticFiles();
