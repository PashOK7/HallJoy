#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "native_analog_backend.h"

bool Mad68ProR_PrepareProtocolRouting();
bool Mad68ProR_Start();
void Mad68ProR_Stop();
void Mad68ProR_NotifyDeviceChange();
void Mad68ProR_NotifyKeyboardDeviceReset();
void Mad68ProR_NotifyKeyboardEvent(std::uint16_t hidUsage, bool isKeyDown, bool isInjected);

// One-shot idempotent A9 recovery used by the out-of-process diagnostic
// watchdog after HallJoy exits. Returns true when the audited device accepted
// a primary-transport write; an ACK is logged when available but is not required
// because the process may be recovering from a lost response path.
bool Mad68ProR_EmergencyRestoreInputOnce();

bool Mad68ProR_IsRunning();
bool Mad68ProR_IsDevicePresent();
bool Mad68ProR_IsAuditedDevicePresent();
bool Mad68ProR_IsProtocolDevicePresent();
bool Mad68ProR_IsRoutedProduct(std::uint16_t productId);
std::uint16_t Mad68ProR_GetProductId();
bool Mad68ProR_IsConnected();
bool Mad68ProR_IsFullConnected();
bool Mad68ProR_IsEmergencyWasd();
std::uint32_t Mad68ProR_GetCoverage();
std::uint16_t Mad68ProR_GetFirmwareVersion();
bool Mad68ProR_OwnsHid(std::uint16_t hidUsage);
std::uint16_t Mad68ProR_GetMilli(std::uint16_t hidUsage);
std::uint16_t Mad68ProR_GetRaw(std::uint16_t hidUsage);

struct Mad68ProRChangeBatch
{
    std::array<std::uint64_t, 4> dirtyHids{};
    std::uint64_t sampleCount = 0;
    std::uint64_t latestSequence = 0;
    std::int64_t earliestA0ReceivedQpc = 0;
    std::int64_t latestA0ReceivedQpc = 0;
    std::int64_t latestSnapshotPublishedQpc = 0;
    std::uint16_t latestHid = 0;
    std::uint16_t latestRaw = 0;
};

// Consume the set of MAD68 HID usages whose analogue values changed since the
// previous realtime tick. The worker publishes these bits before waking the
// realtime loop, so the consumer can update only the affected curve cache and
// attach precise A0 -> snapshot -> ViGEm timing data.
bool Mad68ProR_ConsumeChangeBatch(Mad68ProRChangeBatch* out);

void Mad68ProR_GetStatusText(wchar_t* buffer, std::size_t chars);

// Standard native-backend registry descriptor.
const NativeAnalogBackendDescriptor& Mad68ProR_GetNativeBackendDescriptor();
