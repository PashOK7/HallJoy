#pragma once

#include "native_analog_backend.h"

// Present only in the dedicated M901 diagnostic executable. It records the
// normal-mode travel stream but deliberately has no HallJoy input ownership.
const NativeAnalogBackendDescriptor& RogAzoth96HeDiagnostic_GetNativeBackendDescriptor();
