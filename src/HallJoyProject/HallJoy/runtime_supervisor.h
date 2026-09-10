#pragma once

#include "worker_lifecycle.h"

// Owns only recovery of the already-started realtime loop and isolated output
// runtime. It never opens HID, initializes providers, or touches UI state.
bool RuntimeSupervisor_Start() noexcept;
halljoy::lifecycle::StopResult RuntimeSupervisor_Stop() noexcept;
bool RuntimeSupervisor_IsRunning() noexcept;
