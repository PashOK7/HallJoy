#pragma once
#include <windows.h>
#include <cstdint>

bool RealtimeLoop_Start();
void RealtimeLoop_Stop();
bool RealtimeLoop_IsRunning();

// Wake the realtime thread immediately after a backend publishes fresh input.
// The implementation uses a monotonic process-local generation word with
// WaitOnAddress/WakeByAddress, so there is no closeable event-handle lifecycle.
void RealtimeLoop_NotifyInputChanged();
void RealtimeLoop_NotifyInputChangedAt(LONGLONG sourceQpc);

// Optional support measurement. Disabled in the final build unless explicitly
// requested with --latency-trace.
bool RealtimeLoop_IsLatencyTraceEnabled();
LONGLONG RealtimeLoop_GetLastInputNotifyQpc();
uint64_t RealtimeLoop_GetInputNotifySequence();

void RealtimeLoop_SetIntervalMs(UINT ms);
UINT RealtimeLoop_GetIntervalMs();
