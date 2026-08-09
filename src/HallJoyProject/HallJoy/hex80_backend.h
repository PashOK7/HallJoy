#pragma once

#include <cstdint>

#include "native_analog_backend.h"

struct Hex80Telemetry
{
    bool present = false;
    bool connected = false;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint16_t firmwareVersion = 0;
    std::uint16_t travelMax = 0;
    std::uint32_t mappedKeys = 0;
    std::uint32_t activeKeys = 0;
    std::uint32_t observedKeys = 0;
    std::uint32_t inputReportBytes = 0;
    std::uint32_t outputReportBytes = 0;
    std::uint32_t chunkHz10 = 0;
    std::uint32_t matrixHz10 = 0;
    std::uint32_t averageTransactionUs = 0;
    std::uint32_t maximumTransactionUs = 0;
    std::uint32_t averageMatrixIntervalUs = 0;
    std::uint32_t maximumMatrixIntervalUs = 0;
    std::uint32_t lastPacketAgeMs = 0;
    std::uint64_t pollAttempts = 0;
    std::uint64_t pollSuccess = 0;
    std::uint64_t pollFail = 0;
    std::uint64_t matrixCycles = 0;
};

// Startup-only classification. Known Hex80 PIDs are excluded from UAP only
// after the read-only 0x96 travel-info and travel-buffer probes both validate.
bool Hex80_PrepareProtocolRouting();
bool Hex80_Start();
void Hex80_Stop();
void Hex80_NotifyDeviceChange();

bool Hex80_IsRunning();
bool Hex80_IsDevicePresent();
bool Hex80_IsProtocolDevicePresent();
bool Hex80_IsConnected();
bool Hex80_OwnsHid(std::uint16_t hidUsage);
std::uint16_t Hex80_GetMilli(std::uint16_t hidUsage);
void Hex80_GetTelemetry(Hex80Telemetry* out);

// Standard native-backend registry descriptor.
const NativeAnalogBackendDescriptor& Hex80_GetNativeBackendDescriptor();
