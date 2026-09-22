// Calibrated K4 travel before uint8 rounding. No temporal filtering/deadband.
#pragma once
#include <stdint.h>
#include <math.h>
static inline float hjk4_precise_depth(uint16_t raw,uint16_t zero,float scale,
                                     float reference,float b,float c,float d) {
    if(!isfinite(scale) || scale<=0 || raw>=zero)return 0;
    // P(x)-P(reference), factored to avoid cancellation near released position.
    const float delta=(float)raw-(float)zero;
    const float x=reference+delta;
    const float displacement=delta*(b+c*(x+reference)+d*(x*x+x*reference+reference*reference));
    const float value=displacement*(scale/40.0f);
    if(!isfinite(value) || value<=0)return 0;
    return value>=1?1:value;
}
