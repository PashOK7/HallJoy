#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mouse_ipc.h"

static HANDLE g_mouseIpcMap = nullptr;
static HallJoyMouseIpcShared* g_mouseIpc = nullptr;
static LONG g_lastAsiHeartbeat = 0;
static ULONGLONG g_lastAsiHeartbeatTick = 0;

bool MouseIpc_InitPublisher()
{
    if (g_mouseIpc) return true;

    g_mouseIpcMap = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        (DWORD)sizeof(HallJoyMouseIpcShared),
        kHallJoyMouseIpcName);
    if (!g_mouseIpcMap)
        return false;

    void* view = MapViewOfFile(g_mouseIpcMap, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, sizeof(HallJoyMouseIpcShared));
    if (!view)
    {
        CloseHandle(g_mouseIpcMap);
        g_mouseIpcMap = nullptr;
        return false;
    }

    g_mouseIpc = (HallJoyMouseIpcShared*)view;
    if (GetLastError() != ERROR_ALREADY_EXISTS)
    {
        ZeroMemory(g_mouseIpc, sizeof(*g_mouseIpc));
        g_mouseIpc->magic = kHallJoyMouseIpcMagic;
        g_mouseIpc->version = kHallJoyMouseIpcVersion;
    }
    return true;
}

void MouseIpc_ShutdownPublisher()
{
    if (g_mouseIpc)
    {
        UnmapViewOfFile(g_mouseIpc);
        g_mouseIpc = nullptr;
    }
    if (g_mouseIpcMap)
    {
        CloseHandle(g_mouseIpcMap);
        g_mouseIpcMap = nullptr;
    }
    g_lastAsiHeartbeat = 0;
    g_lastAsiHeartbeatTick = 0;
}

void MouseIpc_PublishState(bool blockMouseWanted, bool blockMouseActive, bool mouseToStickEnabled, bool pauseByRShift)
{
    if (!g_mouseIpc) return;

    InterlockedExchange(&g_mouseIpc->blockMouseWanted, blockMouseWanted ? 1 : 0);
    InterlockedExchange(&g_mouseIpc->blockMouseActive, blockMouseActive ? 1 : 0);
    InterlockedExchange(&g_mouseIpc->mouseToStickEnabled, mouseToStickEnabled ? 1 : 0);
    InterlockedExchange(&g_mouseIpc->pauseByRShift, pauseByRShift ? 1 : 0);
    InterlockedIncrement(&g_mouseIpc->heartbeat);
}

bool MouseIpc_IsAsiConnected()
{
    if (!g_mouseIpc) return false;
    if (g_mouseIpc->asiAttached == 0) return false;

    LONG hb = g_mouseIpc->asiHeartbeat;
    ULONGLONG now = GetTickCount64();
    if (hb != g_lastAsiHeartbeat)
    {
        g_lastAsiHeartbeat = hb;
        g_lastAsiHeartbeatTick = now;
        return true;
    }
    return (g_lastAsiHeartbeatTick != 0 && (now - g_lastAsiHeartbeatTick) <= 1500);
}
