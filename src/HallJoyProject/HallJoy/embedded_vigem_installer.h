#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>

enum class EmbeddedVigemInstallStatus : std::uint32_t
{
    Installed = 0,
    RestartRequired = 1,
    UserCancelled = 2,
    ResourceInvalid = 3,
    ExtractionFailed = 4,
    SignatureInvalid = 5,
    LaunchFailed = 6,
    TimedOut = 7,
    InstallerFailed = 8,
};

struct EmbeddedVigemInstallResult
{
    EmbeddedVigemInstallStatus status = EmbeddedVigemInstallStatus::ResourceInvalid;
    DWORD nativeError = ERROR_SUCCESS;
    DWORD installerExitCode = 0;
};

// Extracts the pinned resource to a cryptographically unpredictable, create-new
// path, verifies its exact SHA-256 and Authenticode signature, then runs the
// official interactive installer after Windows elevation consent.
EmbeddedVigemInstallResult EmbeddedVigemInstaller_Run(HINSTANCE hInst, HWND owner) noexcept;

// Verifies the complete resource/extraction/signature path without launching or
// elevating anything. The official build runs this against the exact linked EXE.
bool EmbeddedVigemInstaller_VerifyOnly(HINSTANCE hInst, DWORD* nativeError) noexcept;

