#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

// Shared-memory bridge: HallJoy -> in-game ASI helper.
// Name is stable so ASI can open it read-only.
static constexpr const wchar_t* kHallJoyMouseIpcName = L"Local\\HallJoy_MouseBridge_v1";
static constexpr uint32_t kHallJoyMouseIpcMagic = 0x484A4D42u; // 'HJMB'
static constexpr uint32_t kHallJoyMouseIpcVersion = 1u;
static constexpr LONG kHallJoyMouseIpcStructSize = 40;

struct HallJoyMouseIpcShared
{
    volatile LONG magic = static_cast<LONG>(kHallJoyMouseIpcMagic);
    volatile LONG version = static_cast<LONG>(kHallJoyMouseIpcVersion);
    volatile LONG blockMouseWanted = 0; // user enabled Block Mouse + Mouse->Stick
    volatile LONG blockMouseActive = 0; // HallJoy currently blocks in its own hook
    volatile LONG mouseToStickEnabled = 0;
    volatile LONG pauseByRShift = 0;    // temporary pause requested by user
    volatile LONG heartbeat = 0;        // incremented periodically
    volatile LONG asiHeartbeat = 0;     // incremented by ASI helper
    volatile LONG asiAttached = 0;      // 1 while ASI helper is alive
    // Kept at the former reserved1 offset so the v1 external ABI remains
    // binary-compatible. Zero is accepted once for legacy peers and upgraded.
    volatile LONG structSize = kHallJoyMouseIpcStructSize;
};

static_assert(sizeof(HallJoyMouseIpcShared) == kHallJoyMouseIpcStructSize, "mouse IPC v1 ABI size changed");

bool MouseIpc_InitPublisher();
void MouseIpc_ShutdownPublisher();
void MouseIpc_PublishState(bool blockMouseWanted, bool blockMouseActive, bool mouseToStickEnabled, bool pauseByRShift);
bool MouseIpc_IsAsiConnected();

#if defined(HALLJOY_ANALOG_SIMULATOR)
bool MouseIpc_RunPolicySelfTest();
#endif
