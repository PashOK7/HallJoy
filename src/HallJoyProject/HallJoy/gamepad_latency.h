#pragma once
// Owner disabled this feature; source is retained for future work.
#define HALLJOY_CAMERA_LATENCY_TEST_ENABLED 0
#include <windows.h>
// Explicit diagnostic window; its sampling/rendering thread is separate from UI/input.
void GamepadLatency_Open(HWND owner) noexcept;
void GamepadLatency_Stop() noexcept;
