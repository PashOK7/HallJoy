#pragma once

#include <cstdint>

#include "native_analog_backend.h"

struct AddressedAnalogTelemetry
{
    bool present = false;
    bool connected = false;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint32_t mappedKeys = 0;
    std::uint32_t activeKeys = 0;
    std::uint32_t inputReportBytes = 0;
    std::uint32_t outputReportBytes = 0;
    std::uint64_t pollAttempts = 0;
    std::uint64_t pollSuccess = 0;
    std::uint64_t pollFail = 0;
    std::uint32_t lastResponseAgeMs = 0;
};

// Startup-only capability classification. A device is reserved from UAP only
// after the exact FF60:0061/64-byte fingerprint and a valid checksummed
// 09/94/02 response for the requested key IDs have both been observed.
bool AddressedAnalog_PrepareProtocolRouting();
bool AddressedAnalog_Start();
void AddressedAnalog_Stop();
void AddressedAnalog_NotifyDeviceChange();
bool AddressedAnalog_IsProtocolDevicePresent();
bool AddressedAnalog_IsConnected();
bool AddressedAnalog_OwnsHid(std::uint16_t hidUsage);
std::uint16_t AddressedAnalog_GetMilli(std::uint16_t hidUsage);
void AddressedAnalog_GetTelemetry(AddressedAnalogTelemetry* out);

// Standard native-backend registry descriptor.
const NativeAnalogBackendDescriptor& AddressedAnalog_GetNativeBackendDescriptor();
