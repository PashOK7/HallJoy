#pragma once

#include <cstdint>

void BackendCurve_BeginTick();
void BackendCurve_Invalidate();
uint64_t BackendCurve_GetGeneration();
float BackendCurve_ApplyByHid(uint16_t hid, float x01Raw);

// Builds one immutable curve definition and applies it to both values. This is
// the only supported way for a route shadow to evaluate a divergent raw value:
// two separate settings reads could otherwise compare different user edits.
void BackendCurve_ApplyPairByHid(uint16_t hid,
    float qualifiedRaw, float shadowRaw,
    float* qualifiedFiltered, float* shadowFiltered);
