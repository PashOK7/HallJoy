// Explicit little-endian wire profile; no compiler structure packing on USB.
#pragma once
#include "keychron_hj_protocol.h"
#include "keychron_onboard_curve.h"

#define HJO_PROFILE_BYTES 5044u
#define HJO_CURVE_OFFSET 252u
#define HJO_CURVE_BYTES 42u

typedef struct hjo_profile {
    hjo_mapping mapping;
    hjo_curve curves[HJO_SLOTS];
} hjo_profile;

static inline float hjo_get_float(const uint8_t *p) {
    const uint32_t bits = hjk4_u32(p);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
static inline void hjo_put_float(uint8_t *p, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    hjk4_put32(p, bits);
}
static inline void hjo_decode_curve(hjo_curve *c, const uint8_t *p) {
    for (unsigned i = 0; i < 4; ++i) {
        c->x[i] = hjo_get_float(p + 4*i);
        c->y[i] = hjo_get_float(p + 16 + 4*i);
    }
    c->weight[0] = hjo_get_float(p + 32);
    c->weight[1] = hjo_get_float(p + 36);
    c->linear = p[40]; c->invert = p[41];
}

// Full validation precedes any publication into destination. Caller owns its
// task/lock and can decode into the active object after validation completes.
static inline int hjo_profile_decode(hjo_profile *dst, const uint8_t *p, size_t size) {
    if (!dst || !p || size != HJO_PROFILE_BYTES || memcmp(p, "HJP1", 4) ||
        hjk4_u16(p+4) != HJO_PROFILE_BYTES || p[7] || p[22] || p[23] ||
        hjk4_u32(p+5040) != hjk4_crc32(p,5040)) return 0;
    hjo_mapping mapping;
    memset(&mapping, 0, sizeof(mapping));
    mapping.flags = p[6]; mapping.sensitivity = hjo_get_float(p+8);
    memcpy(mapping.axes, p+12, 8); memcpy(mapping.triggers, p+20, 2);
    for (unsigned i=0;i<HJO_SLOTS;++i) mapping.buttons[i]=hjk4_u16(p+24+2*i);
    if (!hjo_mapping_valid(&mapping)) return 0;
    for (unsigned i=0;i<HJO_SLOTS;++i) {
        hjo_curve c;
        hjo_decode_curve(&c,p+HJO_CURVE_OFFSET+i*HJO_CURVE_BYTES);
        if (!hjo_curve_valid(&c)) return 0;
    }
    dst->mapping=mapping;
    for (unsigned i=0;i<HJO_SLOTS;++i)
        hjo_decode_curve(&dst->curves[i],p+HJO_CURVE_OFFSET+i*HJO_CURVE_BYTES);
    return 1;
}

static inline int hjo_profile_encode(uint8_t *dst, size_t size, const hjo_profile *p) {
    if (!dst || !p || size < HJO_PROFILE_BYTES || !hjo_mapping_valid(&p->mapping)) return 0;
    for (unsigned i=0;i<HJO_SLOTS;++i) if (!hjo_curve_valid(&p->curves[i])) return 0;
    memset(dst,0,HJO_PROFILE_BYTES);
    memcpy(dst,"HJP1",4); hjk4_put16(dst+4,HJO_PROFILE_BYTES);
    dst[6]=p->mapping.flags; hjo_put_float(dst+8,p->mapping.sensitivity);
    memcpy(dst+12,p->mapping.axes,8); memcpy(dst+20,p->mapping.triggers,2);
    for (unsigned i=0;i<HJO_SLOTS;++i) {
        hjk4_put16(dst+24+2*i,p->mapping.buttons[i]);
        uint8_t *out=dst+HJO_CURVE_OFFSET+i*HJO_CURVE_BYTES;
        const hjo_curve *c=&p->curves[i];
        for (unsigned j=0;j<4;++j) {
            hjo_put_float(out+4*j,c->x[j]); hjo_put_float(out+16+4*j,c->y[j]);
        }
        hjo_put_float(out+32,c->weight[0]); hjo_put_float(out+36,c->weight[1]);
        out[40]=c->linear; out[41]=c->invert;
    }
    hjk4_put32(dst+5040,hjk4_crc32(dst,5040));
    return 1;
}
