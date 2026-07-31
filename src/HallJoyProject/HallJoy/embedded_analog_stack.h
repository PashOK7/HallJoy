#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Handles a legacy helper invocation. V6 does not install anything into
// Program Files and does not request elevation.
bool EmbeddedAnalogStack_TryRunInstallerCommand(HINSTANCE hInst, int& exitCode);

// Extracts the exact embedded ABI1 plugin to a private file next to HallJoy.
// The Wooting SDK layer is deliberately bypassed for the Madlions path.
bool EmbeddedAnalogStack_Prepare(HINSTANCE hInst);

// File name, relative to the HallJoy executable, loaded only by the isolated
// analog-host child process.
const wchar_t* EmbeddedAnalogStack_PrivatePluginFileName();
