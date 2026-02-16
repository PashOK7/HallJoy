#pragma once

#include <cstdint>

void BackendCurve_BeginTick();
float BackendCurve_ApplyByHid(uint16_t hid, float x01Raw);

