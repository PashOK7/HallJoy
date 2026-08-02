#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int App_Run(HINSTANCE hInst, int nCmdShow);
void App_ForceFinalShutdown() noexcept;
void App_DisarmShutdownWatchdog() noexcept;
bool App_RequiresImmediateProcessExit() noexcept;
bool App_TakeRelaunchRequest() noexcept;
bool App_RelaunchSelf() noexcept;
