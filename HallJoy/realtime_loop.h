#pragma once
#include <windows.h>

bool RealtimeLoop_Start();
void RealtimeLoop_Stop();

void RealtimeLoop_SetIntervalMs(UINT ms);
UINT RealtimeLoop_GetIntervalMs();