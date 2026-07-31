#pragma once

#include <array>
#include <cstdint>
#include "halljoy_plugin_telemetry.h"
#include "wooting-analog-sdk.h"

// Starts and supervises the crash-isolated analog host. All plugin/Soup/HID code
// runs in a child process; these functions never load the SDK into HallJoy.
int AnalogHostClient_Initialise();
bool AnalogHostClient_IsInitialised();
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

// Called very early by wWinMain. Returns true when this process is the child
// analog host and places its process exit code in exitCode.
bool AnalogHost_TryRunCommand(int& exitCode);

// Deletes reports from previous runs before a new HallJoy diagnostic session.
void AnalogHostClient_ResetDiagnosticFiles();
