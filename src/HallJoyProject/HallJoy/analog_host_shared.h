#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <type_traits>
#include "halljoy_plugin_telemetry.h"
#include "halljoy_dense_snapshot.h"

// Shared, fixed-layout protocol between the HallJoy UI/realtime process and
// its crash-isolated direct ABI1 plugin host. Keep this header POD-only.
namespace HallJoyAnalogHost
{
    constexpr std::uint32_t kMagic = 0x39414A48u; // "HJA9"
    constexpr std::uint32_t kVersion = 9;
    constexpr std::uint32_t kMaxKeys = HallJoyDenseSnapshot::kKeyCount;
    constexpr std::uint32_t kMaxDevices = HallJoyPluginTelemetry::kMaxDevices;

    enum Status : LONG
    {
        Status_Stopped = 0,
        Status_Starting = 1,
        Status_Ready = 2,
        Status_Error = 3,
        Status_Restarting = 4,
    };

    enum Checkpoint : LONG
    {
        Checkpoint_None = 0,
        Checkpoint_ProcessStart = 10,
        Checkpoint_OpenSharedMemory = 20,
        Checkpoint_LoadSdk = 30,
        Checkpoint_ResolveExports = 40,
        Checkpoint_SdkInitialise = 50,
        Checkpoint_SetKeycodeMode = 60,
        Checkpoint_WaitForPoll = 70,
        Checkpoint_BeforeReadFullBuffer = 80,
        Checkpoint_AfterReadFullBuffer = 90,
        Checkpoint_ValidateSnapshot = 100,
        Checkpoint_PublishSnapshot = 110,
        Checkpoint_SdkUninitialise = 120,
        Checkpoint_ProcessExit = 130,

        Checkpoint_PluginReadEntry = 200,
        Checkpoint_PluginBeforeDeviceLock = 210,
        Checkpoint_PluginBeforeKeyboardUpdate = 220,
        Checkpoint_PluginAfterKeyboardUpdate = 230,
        Checkpoint_PluginReadReturn = 240,

        Checkpoint_MadlionsEntry = 300,
        Checkpoint_MadlionsBeforeMutex = 310,
        Checkpoint_MadlionsTransportBegin = 320,
        Checkpoint_MadlionsReadArm = 321,
        Checkpoint_MadlionsStaleDiscarded = 322,
        Checkpoint_MadlionsReadPending = 323,
        Checkpoint_MadlionsWriteBegin = 330,
        Checkpoint_MadlionsWriteWait = 331,
        Checkpoint_MadlionsWriteComplete = 332,
        Checkpoint_MadlionsReadWait = 340,
        Checkpoint_MadlionsReadComplete = 341,
        Checkpoint_MadlionsCancelRead = 342,
        Checkpoint_MadlionsCancelWrite = 343,
        Checkpoint_MadlionsTransportReturn = 348,
        Checkpoint_MadlionsTransportFailed = 349,
        Checkpoint_MadlionsAfterTransaction = 350,
        Checkpoint_MadlionsParse = 360,
        Checkpoint_MadlionsReturn = 370,
    };

    struct alignas(64) SharedState
    {
        std::uint32_t magic;
        std::uint32_t version;
        std::uint32_t structSize;
        std::uint32_t reserved0;

        // Snapshot seqlock: odd while the host writes, even while stable.
        volatile LONG snapshotSequence;
        volatile LONG status;
        volatile LONG initResult;
        volatile LONG lastError;

        volatile LONG requestedKeycodeMode;
        volatile LONG appliedKeycodeMode;
        volatile LONG keyCount; // compatibility compact count
        volatile LONG restartCount;

        volatile LONG checkpoint;
        volatile LONG hostPid;
        volatile LONG pollCounterLow;
        volatile LONG invalidSnapshotCount;
        volatile LONG diagnosticCrashAfterPolls;
        volatile LONG transportError;
        volatile LONG denseDeviceCount;
        volatile LONG denseActiveKeyCount;

        alignas(8) volatile LONG64 heartbeatTickMs;
        alignas(8) volatile LONG64 lastPublishTickMs;
        alignas(8) volatile LONG64 totalPolls;
        alignas(8) volatile LONG64 totalSuccessfulPolls;
        alignas(8) volatile LONG64 snapshotGeneration;
        alignas(8) volatile LONG64 snapshotTimestampUs;

        volatile LONG deviceTelemetryCount;
        LONG reservedTelemetry0;
        HallJoyPluginTelemetry::DeviceV1 deviceTelemetry[kMaxDevices];

        // Compatibility sparse view used by the public Wooting-style functions.
        std::uint16_t codes[kMaxKeys];
        float values[kMaxKeys];

        // V11 canonical snapshot: direct HID-indexed values plus coherent
        // per-device snapshots. Source routing can consume these without a
        // sparse-list scan or a 16-active-key ceiling.
        float denseValues[kMaxKeys];
        HallJoyDenseSnapshot::DeviceV1 denseDevices[kMaxDevices];
    };

    static_assert(sizeof(float) == 4, "Shared protocol requires IEEE-754 32-bit float");
    static_assert(std::is_standard_layout_v<SharedState>, "analog-host shared ABI must remain standard-layout");
    static_assert(std::is_trivially_copyable_v<SharedState>, "analog-host shared ABI must remain trivially copyable");
}
