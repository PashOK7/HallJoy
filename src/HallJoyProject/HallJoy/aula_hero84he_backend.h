#pragma once

#include "native_analog_backend.h"

// Exact AULA HERO84 HE model-fingerprinted experimental backend.  This is
// intentionally separate from the no-input diagnostic descriptor.
const NativeAnalogBackendDescriptor &AulaHero84He_GetNativeBackendDescriptor();

// Headless linked-image software check; never opens HID or starts workers.
bool AulaHero84He_TestPublication(int* failedLine = nullptr);
