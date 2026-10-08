#pragma once
#include "native_analog_backend.h"
const NativeAnalogBackendDescriptor& KeychronOnboard_GetNativeBackendDescriptor();
// Latched for this engine generation after exact firmware proof. It remains
// reserved through reconnect; a disappearing keyboard must not spawn ViGEm.
bool KeychronOnboard_OwnsOutput() noexcept;
void KeychronOnboard_SetAdmission(bool admitted) noexcept;
// Default true: engine stop (pause) parks the K4 session without USB
// re-enumeration. Set false before process exit so the session fully closes.
void KeychronOnboard_SetParkOnStop(bool park) noexcept;
// Process exit after the engine stopped: disables parking and returns a K4
// that this process parked to ordinary keyboard mode.
void KeychronOnboard_ReleaseParked() noexcept;
void KeychronOnboard_MonitorVisible(bool visible) noexcept;

bool KeychronOnboard_CopyPad(std::uint8_t* destination,std::size_t size) noexcept;
std::uint64_t KeychronOnboard_MonitorGeneration() noexcept;

bool KeychronOnboard_WorkerHealthy() noexcept;
// True when a K4 running HallJoy onboard firmware is present but this engine
// generation does not own it (it was absent or busy at startup). The
// application then starts a fresh generation so the onboard route takes it.
bool KeychronOnboard_NeedsTakeover() noexcept;
