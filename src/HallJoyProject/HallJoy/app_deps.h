#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

enum class DependencyGuidanceResult
{
    NoAction = 0,
    ManualInstallRequired = 1,
    InstallCompleted = 2,
};

DependencyGuidanceResult AppDeps_ShowMissingDependencyGuidance(
    HINSTANCE hInst, HWND hwnd, uint32_t issues);

// Build-time exact-artifact gate. This command verifies and removes the
// embedded installer without ever launching or elevating it.
bool AppDeps_TryRunEmbeddedInstallerVerificationCommand(
    HINSTANCE hInst, int& exitCode) noexcept;
