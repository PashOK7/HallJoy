// Exact transport of the current firmware's 0..240 travel, no extra quantization.
#pragma once
#include "keychron_hj_protocol.h"
#define HJK4_COMPACT_BYTES 130u
#define HJK4_COMPACT_PAGES 6u
static inline void hjk4_compact_encode(uint8_t *dst,uint32_t session,uint32_t sequence,
    uint16_t scan_us,uint8_t flags,const uint8_t *travel) {
    dst[0]=1;dst[1]=flags;hjk4_put16(dst+2,scan_us);
    hjk4_put32(dst+4,session);hjk4_put32(dst+8,sequence);
    for(unsigned i=0;i<HJK4_SLOTS;++i)dst[12+i]=travel[i];
    hjk4_put32(dst+126,hjk4_crc32(dst,126));
}
static inline int hjk4_compact_accept(hjk4_receiver *state,const uint8_t *src,size_t size,uint16_t *depth) {
    if(!state || !src || !depth || size!=HJK4_COMPACT_BYTES || src[0]!=1 ||
        !(src[1]&HJK4_CALIBRATED) || (src[1]&~(HJK4_CALIBRATED|HJK4_OVERRUN)) ||
        !hjk4_u16(src+2) || !state->session || hjk4_u32(src+4)!=state->session ||
        hjk4_u32(src+126)!=hjk4_crc32(src,126))return 0;
    const uint32_t sequence=hjk4_u32(src+8),delta=sequence-state->sequence;
    if(state->have_sequence && (!delta || delta>=0x80000000u))return 0;
    for(unsigned i=0;i<HJK4_SLOTS;++i)if(src[12+i]>240)return 0;
    for(unsigned i=0;i<HJK4_SLOTS;++i)depth[i]=(uint16_t)((uint32_t)src[12+i]*65535u/240u);
    state->sequence=sequence;state->have_sequence=1;return 1;
}
