#pragma once

#include <cstdint>

void BackendCurve_BeginTick();
void BackendCurve_Invalidate();
uint64_t BackendCurve_GetGeneration();
float BackendCurve_ApplyByHid(uint16_t hid, float x01Raw);
