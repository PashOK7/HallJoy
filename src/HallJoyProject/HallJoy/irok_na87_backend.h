#pragma once
#include <windows.h>
#include "native_analog_backend.h"
const NativeAnalogBackendDescriptor& IrokNa87_GetNativeBackendDescriptor();
bool IrokNa87_TryRunSelfTest(int& result) noexcept;
void IrokNa87_UpdateWindow(HWND window) noexcept;
