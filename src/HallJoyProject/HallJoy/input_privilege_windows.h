#pragma once
#include <windows.h>
#include "input_privilege_warning.h"

namespace halljoy::input_privilege
{
// Only used when foreign-token access fails. A unique registered message with
// no payload/command avoids collisions with the target's WM_APP/WM_USER API.
// No input injection, focus change, synchronous wait, or message-filter change.
// Invoke once per foreground transition, never for each key or polling tick.
inline bool IsWindowMessageAccessBlocked(HWND window) noexcept
{
    if (!window) return false; // NULL would post to our own thread.
    static const UINT message = RegisterWindowMessageW(
        L"HallJoy.InputPermissionProbe.{46BD4C20-2301-47AD-BC98-CA9246A326CD}");
    if (!message) return false;
    SetLastError(ERROR_SUCCESS);
    const BOOL posted = PostMessageW(window, message, 0, 0);
    const DWORD error = GetLastError();
    return PostingDeniedByUipi(posted != FALSE, error);
}
}
