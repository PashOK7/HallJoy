#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

// Shared-memory bridge: HallJoy -> in-game ASI helper.
// Name is stable so ASI can open it read-only.
static constexpr const wchar_t* kHallJoyMouseIpcName = L"Local\\HallJoy_MouseBridge_v1";
static constexpr uint32_t kHallJoyMouseIpcMagic = 0x484A4D42u; // 'HJMB'
static constexpr uint32_t kHallJoyMouseIpcVersion = 1u;

struct HallJoyMouseIpcShared
{
    uint32_t magic = kHallJoyMouseIpcMagic;
    uint32_t version = kHallJoyMouseIpcVersion;
    volatile LONG blockMouseWanted = 0; // user enabled Block Mouse + Mouse->Stick
    volatile LONG blockMouseActive = 0; // HallJoy currently blocks in its own hook
    volatile LONG mouseToStickEnabled = 0;
    volatile LONG pauseByRShift = 0;    // temporary pause requested by user
    volatile LONG heartbeat = 0;        // incremented periodically
    volatile LONG asiHeartbeat = 0;     // incremented by ASI helper
    volatile LONG asiAttached = 0;      // 1 while ASI helper is alive
    volatile LONG reserved1 = 0;
};

bool MouseIpc_InitPublisher();
void MouseIpc_ShutdownPublisher();
void MouseIpc_PublishState(bool blockMouseWanted, bool blockMouseActive, bool mouseToStickEnabled, bool pauseByRShift);
bool MouseIpc_IsAsiConnected();
