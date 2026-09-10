#pragma once

#include <cstdint>

#include "native_analog_backend.h"

// Exists only in the dedicated AULA HERO84 HE diagnostic image.  It records
// bounded read-only evidence and deliberately never owns or transforms input.
const NativeAnalogBackendDescriptor& AulaHero84HeDiagnostic_GetNativeBackendDescriptor();

#if defined(HALLJOY_AULA_HERO84HE_DIAGNOSTIC)
void AulaHero84HeDiagnostic_RecordRawKeyboardEvent(
    std::uintptr_t rawDevice, std::uint16_t hidUsage,
    std::uint16_t makeCode, std::uint16_t flags, std::uint16_t virtualKey);
// Called only after app.cpp has registered keyboard Raw Input.  The pre-UAP
// worker is deliberately parked until this handshake succeeds.
void AulaHero84HeDiagnostic_NotifyRawInputReady(bool registered);
#endif
