// Allocation-free C11 counterpart of configured_xusb_builder.cpp.
// Slot values are already curve-filtered. Differential tests enforce parity.
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

#define HJO_SLOTS 114u
#define HJO_UNBOUND 255u
#define HJO_SNAP 1u
#define HJO_LKP 2u
#define HJO_SUPPRESS 4u

typedef struct hjo_mapping {
    uint8_t axes[4][2];
    uint8_t triggers[2];
    uint8_t flags;
    float sensitivity;
    // Bits use HallJoy ButtonV1 order, not the XInput wire bit order.
    uint16_t buttons[HJO_SLOTS];
} hjo_mapping;

typedef struct hjo_mapper_state {
    uint8_t previous_minus[4], previous_plus[4];
    int8_t direction[4];
    float minus_valley[4], plus_valley[4];
} hjo_mapper_state;

typedef struct hjo_pad {
    int16_t axes[4];
    uint8_t triggers[2];
    uint16_t buttons;
} hjo_pad;

static inline float hjo_clamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
static inline float hjo_max(float a, float b) { return a < b ? b : a; }
static inline float hjo_min(float a, float b) { return b < a ? b : a; }

static inline int hjo_mapping_valid(const hjo_mapping *p) {
    if (!isfinite(p->sensitivity) || p->sensitivity < .02f || p->sensitivity > .95f ||
        (p->flags & ~(HJO_SNAP | HJO_LKP | HJO_SUPPRESS))) return 0;
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 2; ++j)
            if (p->axes[i][j] >= HJO_SLOTS && p->axes[i][j] != HJO_UNBOUND) return 0;
    for (unsigned i = 0; i < 2; ++i)
        if (p->triggers[i] >= HJO_SLOTS && p->triggers[i] != HJO_UNBOUND) return 0;
    for (unsigned i = 0; i < HJO_SLOTS; ++i)
        if (p->buttons[i] & 0x8000u) return 0;
    return 1;
}

static inline int hjo_bound(const hjo_mapping *p, unsigned slot) {
    if (slot >= HJO_SLOTS) return 0;
    if (p->buttons[slot]) return 1;
    for (unsigned i = 0; i < 4; ++i)
        if (p->axes[i][0] == slot || p->axes[i][1] == slot) return 1;
    return p->triggers[0] == slot || p->triggers[1] == slot;
}

static inline float hjo_value(const float values[HJO_SLOTS], uint8_t slot) {
    return slot < HJO_SLOTS ? values[slot] : 0.0f;
}

static inline float hjo_resolve(unsigned a, float mn, float pl,
                                const hjo_mapping *p, hjo_mapper_state *s) {
    const int snap = (p->flags & HJO_SNAP) != 0, lkp = (p->flags & HJO_LKP) != 0;
    if (!snap && !lkp) return pl - mn;
    const int md = isfinite(mn) && mn >= .1f, pd = isfinite(pl) && pl >= .1f;
    const int pm = s->previous_minus[a], pp = s->previous_plus[a];
    if (md && !pm) s->direction[a] = -1;
    if (pd && !pp) s->direction[a] = 1;
    if (lkp) {
        const float delta = hjo_clamp(p->sensitivity, .02f, .95f);
        if (!md) s->minus_valley[a] = 1;
        else if (!pm) s->minus_valley[a] = mn;
        else {
            s->minus_valley[a] = hjo_min(s->minus_valley[a], mn);
            if (mn - s->minus_valley[a] >= delta) {
                s->direction[a] = -1; s->minus_valley[a] = mn;
            }
        }
        if (!pd) s->plus_valley[a] = 1;
        else if (!pp) s->plus_valley[a] = pl;
        else {
            s->plus_valley[a] = hjo_min(s->plus_valley[a], pl);
            if (pl - s->plus_valley[a] >= delta) {
                s->direction[a] = 1; s->plus_valley[a] = pl;
            }
        }
    }
    s->previous_minus[a] = (uint8_t)md; s->previous_plus[a] = (uint8_t)pd;
    const float maximum = hjo_max(mn, pl);
    if (maximum <= .0001f) return 0;
    if (lkp) {
        if (md && !pd) return -mn;
        if (pd && !md) return pl;
        if (md && pd) {
            int direction = s->direction[a];
            if (!direction) direction = pl >= mn ? 1 : -1;
            const float magnitude = snap ? maximum : (direction > 0 ? pl : mn);
            return direction > 0 ? magnitude : -magnitude;
        }
    }
    if (snap) {
        const float difference = pl - mn;
        if (fabsf(difference) > .002f) return difference > 0 ? maximum : -maximum;
        return s->direction[a] > 0 ? maximum : (s->direction[a] < 0 ? -maximum : 0);
    }
    return pl - mn;
}

static inline void hjo_map(const hjo_mapping *p, const float values[HJO_SLOTS],
                           hjo_mapper_state *s, hjo_pad *out) {
    memset(out, 0, sizeof(*out));
    for (unsigned a = 0; a < 4; ++a) {
        float v = hjo_resolve(a, hjo_value(values, p->axes[a][0]),
                              hjo_value(values, p->axes[a][1]), p, s);
        if (!isfinite(v)) v = 0;
        out->axes[a] = (int16_t)lroundf(hjo_clamp(v, -1, 1) * 32767.0f);
    }
    for (unsigned t = 0; t < 2; ++t) {
        float v = hjo_value(values, p->triggers[t]);
        if (!isfinite(v)) v = 0;
        out->triggers[t] = (uint8_t)lroundf(hjo_clamp(v, 0, 1) * 255.0f);
    }
    for (unsigned k = 0; k < HJO_SLOTS; ++k)
        if (isfinite(values[k]) && values[k] >= .1f) out->buttons |= p->buttons[k];
}

static inline uint16_t hjo_xinput_buttons(uint16_t buttons) {
    static const uint16_t bits[15] = {
        0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0x0020, 0x0010,
        0x0400, 0x0040, 0x0080, 0x0001, 0x0002, 0x0004, 0x0008
    };
    uint16_t result = 0;
    for (unsigned i = 0; i < 15; ++i) if (buttons & (1u << i)) result |= bits[i];
    return result;
}
