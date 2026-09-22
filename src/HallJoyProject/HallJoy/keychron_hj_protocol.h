// Experimental K4 protocol core. Shared by firmware C and HallJoy C++.
// No device admission or transport is enabled merely by including this file.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define HJK4_VERSION 1u
#define HJK4_SLOTS 114u
#define HJK4_FRAME_BYTES 256u
#define HJK4_DEPTH_FRAME 1u
#define HJK4_RAW_FRAME 2u
#define HJK4_CALIBRATED 1u
#define HJK4_OVERRUN 2u

static inline uint16_t hjk4_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t hjk4_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void hjk4_put16(uint8_t *p, uint16_t n) {
    p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
}
static inline void hjk4_put32(uint8_t *p, uint32_t n) {
    p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
    p[2] = (uint8_t)(n >> 16); p[3] = (uint8_t)(n >> 24);
}
static inline uint32_t hjk4_crc32(const uint8_t *p, size_t length) {
    static const uint32_t table[16] = {
        0x00000000u,0x1db71064u,0x3b6e20c8u,0x26d930acu,
        0x76dc4190u,0x6b6b51f4u,0x4db26158u,0x5005713cu,
        0xedb88320u,0xf00f9344u,0xd6d6a3e8u,0xcb61b38cu,
        0x9b64c2b0u,0x86d3d2d4u,0xa00ae278u,0xbdbdf21cu
    };
    uint32_t crc = 0xffffffffu;
    while (length--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ table[crc & 15u];
        crc = (crc >> 4) ^ table[crc & 15u];
    }
    return crc ^ 0xffffffffu;
}

// depth: 0..65535 calibrated full travel; raw: unmodified ADC counts.
// timestamp is end of acquisition, NOT USB-send time. Duration covers the scan.
static inline int hjk4_encode(uint8_t *dst, size_t capacity, uint8_t kind,
    uint32_t session, uint32_t sequence, uint32_t timestamp_us,
    uint16_t scan_duration_us, uint8_t flags, const uint16_t *values) {
    if (!dst || !values || capacity < HJK4_FRAME_BYTES || session == 0 ||
        scan_duration_us == 0 || (flags & ~(HJK4_CALIBRATED | HJK4_OVERRUN)) ||
        (kind != HJK4_DEPTH_FRAME && kind != HJK4_RAW_FRAME)) return 0;
    dst[0]='H'; dst[1]='J'; dst[2]='K'; dst[3]='4';
    dst[4]=HJK4_VERSION; dst[5]=kind;
    hjk4_put16(dst+6,HJK4_FRAME_BYTES);
    hjk4_put32(dst+8,session); hjk4_put32(dst+12,sequence);
    hjk4_put32(dst+16,timestamp_us); hjk4_put16(dst+20,scan_duration_us);
    dst[22]=HJK4_SLOTS; dst[23]=flags;
    for (size_t i=0;i<HJK4_SLOTS;++i) hjk4_put16(dst+24+2*i,values[i]);
    hjk4_put32(dst+252,hjk4_crc32(dst,252));
    return 1;
}

typedef struct hjk4_receiver {
    uint32_t session;
    uint32_t sequence;
    uint32_t missing_scans;
    uint8_t have_sequence;
} hjk4_receiver;

// This core accepts one COMPLETE depth frame from a transport assembler.
// Rejected input never modifies output/state. A transport timeout must clear
// active input in the host; an old snapshot must not remain pressed forever.
// Reset receiver on each negotiated session. RAW frames cannot drive controls.
static inline int hjk4_accept(hjk4_receiver *state, const uint8_t *src,
    size_t size, uint16_t *depth) {
    if (!state || !src || !depth || size != HJK4_FRAME_BYTES ||
        !state->session || src[0]!='H' || src[1]!='J' || src[2]!='K' || src[3]!='4' ||
        src[4]!=HJK4_VERSION || src[5]!=HJK4_DEPTH_FRAME ||
        hjk4_u16(src+6)!=HJK4_FRAME_BYTES || hjk4_u32(src+8)!=state->session ||
        !hjk4_u16(src+20) || src[22]!=HJK4_SLOTS ||
        !(src[23] & HJK4_CALIBRATED) ||
        (src[23] & ~(HJK4_CALIBRATED | HJK4_OVERRUN)) ||
        hjk4_u32(src+252)!=hjk4_crc32(src,252)) return 0;
    const uint32_t sequence=hjk4_u32(src+12);
    const uint32_t delta=sequence-state->sequence;
    if (state->have_sequence && (delta==0 || delta>=0x80000000u)) return 0;
    for (size_t i=0;i<HJK4_SLOTS;++i) depth[i]=hjk4_u16(src+24+2*i);
    if (state->have_sequence) {
        const uint32_t missing=delta-1;
        state->missing_scans = missing > UINT32_MAX-state->missing_scans
            ? UINT32_MAX : state->missing_scans+missing;
    }
    state->sequence=sequence; state->have_sequence=1;
    return 1;
}
