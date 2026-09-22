// Prepared HallJoy curve for Cortex-M4. Configuration-time weight conversion;
// no powf, allocation or host dependency in the sample path.
#pragma once
#include "keychron_onboard_mapper.h"

typedef struct hjo_curve {
    float x[4], y[4];
    float weight[2]; // prepared rational weights, range 0..200
    uint8_t linear, invert;
} hjo_curve;

static inline int hjo_curve_valid(const hjo_curve *c) {
    for (unsigned i = 0; i < 4; ++i) {
        if (!isfinite(c->x[i]) || !isfinite(c->y[i]) || c->x[i] < 0 ||
            c->x[i] > 1 || c->y[i] < 0 || c->y[i] > 1) return 0;
        if (i && c->x[i] < c->x[i - 1]) return 0;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (!isfinite(c->weight[i]) || c->weight[i] < 0 || c->weight[i] > 200) return 0;
    return c->linear <= 1 && c->invert <= 1;
}

static inline float hjo_rational(const hjo_curve *c, float t, const float p[4]) {
    const float u = 1.0f - t;
    const float b0 = u * u * u, b1 = 3.0f * u * u * t;
    const float b2 = 3.0f * u * t * t, b3 = t * t * t;
    const float w1 = c->weight[0], w2 = c->weight[1];
    const float d = hjo_max(b0 + b1 * w1 + b2 * w2 + b3, 1e-8f);
    return (b0 * p[0] + b1 * w1 * p[1] + b2 * w2 * p[2] + b3 * p[3]) / d;
}

static inline float hjo_curve_apply(const hjo_curve *c, float raw) {
    float x = isfinite(raw) ? hjo_clamp(raw, 0, 1) : 0;
    if (c->invert) x = 1.0f - x;
    if (x < c->x[0]) return 0;
    if (x > c->x[3]) return c->y[3];
    if (c->linear) {
        const unsigned i = x <= c->x[1] ? 0 : (x <= c->x[2] ? 1 : 2);
        const float denominator = c->x[i + 1] - c->x[i];
        if (fabsf(denominator) < 1e-6f) return c->y[i + 1];
        const float t = hjo_clamp((x - c->x[i]) / denominator, 0, 1);
        return hjo_clamp(c->y[i] + (c->y[i + 1] - c->y[i]) * t, 0, 1);
    }
    if (x <= c->x[0]) return c->y[0];
    if (x >= c->x[3]) return c->y[3];
    float lo = 0, hi = 1;
    for (unsigned i = 0; i < 18; ++i) {
        const float mid = .5f * (lo + hi);
        if (hjo_rational(c, mid, c->x) < x) lo = mid;
        else hi = mid;
    }
    return hjo_clamp(hjo_rational(c, .5f * (lo + hi), c->y), 0, 1);
}
