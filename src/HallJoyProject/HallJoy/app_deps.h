#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

enum class DependencyGuidanceResult
{
    NoAction = 0,
    ManualInstallRequired = 1,
};

DependencyGuidanceResult AppDeps_ShowMissingDependencyGuidance(HWND hwnd, uint32_t issues);

