#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

enum class DependencyInstallResult
{
    Skipped = 0,
    Installed = 1,
    Failed = 2,
};

DependencyInstallResult AppDeps_TryInstallMissingDependencies(HWND hwnd, uint32_t issues);

