#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

enum class EmbeddedAnalogRuntimeLocation
{
    None = 0,
    BesideExecutable = 1,
    PerUser = 2,
};

// Handles a legacy helper invocation. V6 does not install anything into
// Program Files and does not request elevation.
bool EmbeddedAnalogStack_TryRunInstallerCommand(HINSTANCE hInst, int& exitCode);

// Verifies or atomically extracts the exact embedded ABI1 plugin. Portable
// installs use the executable directory; protected installs fall back to a
// versioned per-user runtime directory without elevation.
bool EmbeddedAnalogStack_Prepare(HINSTANCE hInst);

// Absolute verified path passed explicitly to the isolated analog-host child.
const std::wstring& EmbeddedAnalogStack_PrivatePluginPath();
EmbeddedAnalogRuntimeLocation EmbeddedAnalogStack_RuntimeLocation();
const wchar_t* EmbeddedAnalogStack_RuntimeLocationName();
DWORD EmbeddedAnalogStack_LastError();
