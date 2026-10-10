// SPDX-License-Identifier: AGPL-3.0-only OR GPL-2.0-or-later
// Copyright (c) 2026 PashOK7
// Shared firmware interface; see firmware/keychron_k4_he/LICENSING.md.
// Explicit little-endian wire profile; no compiler structure packing on USB.
//
// HJP1 (5044 bytes, firmware r1..r8): one key per stick direction, CRC at 5040.
// HJP2 (5100 bytes, firmware r9+): up to HJO_AXIS_KEYS keys per direction.
//   Identical to HJP1 up to 5039 (curves unchanged). Entry 0 of each direction
//   stays at 12..19, as in HJP1. Extra keys 1..7 of the 8 directions occupy
//   5040..5095 (56 bytes), and the CRC32 moves to 5096..5099.
#pragma once
#include "keychron_hj_protocol.h"
#include "keychron_onboard_curve.h"

#define HJO_PROFILE_BYTES 5044u          // HJP1
#define HJO_PROFILE_BYTES_V2 5100u       // HJP2
#define HJO_PROFILE_BYTES_MAX HJO_PROFILE_BYTES_V2
#define HJO_CURVE_OFFSET 252u
#define HJO_CURVE_BYTES 42u
#define HJO_AXIS_EXTRA_OFFSET 5040u      // HJP2 extra keys, 7 bytes per direction
#define HJO_AXIS_EXTRA_PER_SIDE 7u       // keys 1..7 of an 8-key direction

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

// Direction index: 4 axes x 2 sides, minus side first (matches mapping.axes).
static inline unsigned hjo_direction(unsigned axis, unsigned side) { return axis * 2u + side; }
// Entry 0 of every direction: the HJP1 location, unchanged in HJP2.
static inline unsigned hjo_axis_first_offset(unsigned axis, unsigned side) {
    return 12u + hjo_direction(axis, side);
}
// Extra key k (1..7) of a direction: HJP2 extension block.
static inline unsigned hjo_axis_extra_offset(unsigned axis, unsigned side, unsigned k) {
    return HJO_AXIS_EXTRA_OFFSET + hjo_direction(axis, side) * HJO_AXIS_EXTRA_PER_SIDE + (k - 1u);
}

// Profile size announced by the magic at p[0..3], or 0 when neither format matches.
static inline unsigned hjo_profile_size_from_magic(const uint8_t *p) {
    if (!memcmp(p, "HJP1", 4)) return HJO_PROFILE_BYTES;
    if (!memcmp(p, "HJP2", 4)) return HJO_PROFILE_BYTES_V2;
    return 0;
}

// First wire entry that names a key position the keyboard does not have, from
// the bound entries only (unused entries are HJO_UNBOUND, and the CRC bytes
// after the profile are never read). kind: 0 stick direction, 1 trigger, 2 extra
// stick key, 3 button key. Returns 0 when every bound entry is physical.
static inline int hjo_profile_first_unphysical(const uint8_t *p, size_t size,
        int (*physical)(unsigned slot), uint8_t *slot, uint8_t *kind) {
    for (unsigned i = 12; i < 22; ++i)
        if (p[i] != HJO_UNBOUND && !physical(p[i])) { *slot = p[i]; *kind = i < 20 ? 0 : 1; return 1; }
    if (size == HJO_PROFILE_BYTES_V2) {
        for (unsigned i = 0; i < 4u * 2u * HJO_AXIS_EXTRA_PER_SIDE; ++i) {
            const uint8_t key = p[HJO_AXIS_EXTRA_OFFSET + i];
            if (key != HJO_UNBOUND && !physical(key)) { *slot = key; *kind = 2; return 1; }
        }
    }
    for (unsigned i = 0; i < HJO_SLOTS; ++i)
        if (hjk4_u16(p + 24 + 2*i) && !physical(i)) { *slot = (uint8_t)i; *kind = 3; return 1; }
    return 0;
}

// Whether a mapping needs the HJP2 format (any direction with a second key).
static inline int hjo_mapping_needs_v2(const hjo_mapping *p) {
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 2; ++j)
            for (unsigned k = 1; k < HJO_AXIS_KEYS; ++k)
                if (p->axes[i][j][k] != HJO_UNBOUND) return 1;
    return 0;
}

// Full validation precedes any publication into destination. Caller owns its
// task/lock and can decode into the active object after validation completes.
static inline int hjo_profile_decode(hjo_profile *dst, const uint8_t *p, size_t size) {
    if (!dst || !p) return 0;
    const unsigned expected = hjo_profile_size_from_magic(p);
    if (!expected || size != expected || hjk4_u16(p+4) != expected || p[7] || p[22] || p[23]) return 0;
    const unsigned crc_at = expected - 4u;
    if (hjk4_u32(p+crc_at) != hjk4_crc32(p, crc_at)) return 0;
    hjo_mapping mapping;
    memset(&mapping, 0, sizeof(mapping));
    mapping.flags = p[6]; mapping.sensitivity = hjo_get_float(p+8);
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 2; ++j)
            for (unsigned k = 0; k < HJO_AXIS_KEYS; ++k)
                mapping.axes[i][j][k] = HJO_UNBOUND;
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 2; ++j) {
            mapping.axes[i][j][0] = p[hjo_axis_first_offset(i, j)];
            if (expected == HJO_PROFILE_BYTES_V2)
                for (unsigned k = 1; k < HJO_AXIS_KEYS; ++k)
                    mapping.axes[i][j][k] = p[hjo_axis_extra_offset(i, j, k)];
        }
    memcpy(mapping.triggers, p+20, 2);
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

// Encodes HJP2 when a direction holds more than one key, HJP1 otherwise, so a
// single-key profile stays acceptable to firmware that predates r9.
// dst must hold HJO_PROFILE_BYTES_MAX bytes. Returns the written size or 0.
static inline unsigned hjo_profile_encode(uint8_t *dst, size_t size, const hjo_profile *p) {
    if (!dst || !p || !hjo_mapping_valid(&p->mapping)) return 0;
    for (unsigned i=0;i<HJO_SLOTS;++i) if (!hjo_curve_valid(&p->curves[i])) return 0;
    const unsigned out_size = hjo_mapping_needs_v2(&p->mapping) ? HJO_PROFILE_BYTES_V2 : HJO_PROFILE_BYTES;
    if (size < out_size) return 0;
    memset(dst,0,out_size);
    memcpy(dst,out_size == HJO_PROFILE_BYTES_V2 ? "HJP2" : "HJP1",4);
    hjk4_put16(dst+4,(uint16_t)out_size);
    dst[6]=p->mapping.flags; hjo_put_float(dst+8,p->mapping.sensitivity);
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 2; ++j) {
            dst[hjo_axis_first_offset(i, j)] = p->mapping.axes[i][j][0];
            if (out_size == HJO_PROFILE_BYTES_V2)
                for (unsigned k = 1; k < HJO_AXIS_KEYS; ++k)
                    dst[hjo_axis_extra_offset(i, j, k)] = p->mapping.axes[i][j][k];
        }
    memcpy(dst+20,p->mapping.triggers,2);
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
    hjk4_put32(dst+out_size-4,hjk4_crc32(dst,out_size-4));
    return out_size;
}
