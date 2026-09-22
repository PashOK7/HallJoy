// Experimental onboard lifecycle core, shared by firmware C and host tests.
// Single owner: firmware control/scan task. No I/O, allocation or persistence.
#pragma once
#include <stdint.h>

#define HJO_LEASE_MS 500u
#define HJO_HEARTBEAT_MS 100u
#define HJO_ARM_LEASE_MS 5000u

typedef enum hjo_phase { HJO_OFF, HJO_ARMED, HJO_ACTIVE } hjo_phase;
typedef struct hjo_session {
    uint32_t generation;
    uint32_t sequence;
    uint32_t refreshed_ms;
    hjo_phase phase;
    uint8_t neutral_pending;
} hjo_session;

// generation is issued by this firmware, never supplied by a host START.
// At UINT32_MAX refuse further sessions until USB reconnect/reboot; no wrap reuse.
static inline void hjo_stop(hjo_session *s) {
    if (s->phase == HJO_ACTIVE) s->neutral_pending = 1;
    s->phase = HJO_OFF;
}

// Call before processing control packets AND each scan. Unsigned subtraction
// handles timer wrap; the caller must run at least once per 2^32 ms.
static inline void hjo_tick(hjo_session *s, uint32_t now_ms) {
    if (s->phase != HJO_OFF && (uint32_t)(now_ms - s->refreshed_ms) >=
        (s->phase == HJO_ARMED ? HJO_ARM_LEASE_MS : HJO_LEASE_MS))
        hjo_stop(s);
}

// Handshake expires too. A duplicate OPEN cannot replace a live session.
// The transport must flush queues on USB reset, and retain generation across
// reset within this boot. A boot has a new USB connection, never old packets.
static inline uint32_t hjo_open(hjo_session *s, uint32_t now_ms) {
    hjo_tick(s, now_ms);
    if (s->phase != HJO_OFF || s->neutral_pending || s->generation == UINT32_MAX)
        return 0;
    ++s->generation;
    s->sequence = 0;
    s->refreshed_ms = now_ms;
    s->phase = HJO_ARMED;
    return s->generation;
}

static inline int hjo_new_sequence(uint32_t next, uint32_t previous) {
    const uint32_t delta = next - previous;
    return delta != 0 && delta < 0x80000000u;
}

// Caller has already validated profile, USB wired mode and command framing.
static inline int hjo_start(hjo_session *s, uint32_t generation,
                            uint32_t sequence, uint32_t now_ms) {
    hjo_tick(s, now_ms);
    if (s->phase != HJO_ARMED || generation != s->generation ||
        !hjo_new_sequence(sequence, s->sequence)) return 0;
    s->sequence = sequence;
    s->refreshed_ms = now_ms;
    s->phase = HJO_ACTIVE;
    return 1;
}

static inline int hjo_heartbeat(hjo_session *s, uint32_t generation,
                                uint32_t sequence, uint32_t now_ms) {
    hjo_tick(s, now_ms);
    if (s->phase != HJO_ACTIVE || generation != s->generation ||
        !hjo_new_sequence(sequence, s->sequence)) return 0;
    s->sequence = sequence;
    s->refreshed_ms = now_ms;
    return 1;
}

// STOP may be repeated; it may not terminate a different active generation.
static inline int hjo_host_stop(hjo_session *s, uint32_t generation) {
    if (!generation || generation != s->generation) return 0;
    hjo_stop(s);
    return 1;
}

// Call only after successful neutral transmission. A full USB queue is not an
// acknowledgement. While pending, adapter replaces all queued gamepad data with
// neutral; it must not emit a previously queued active report afterwards.
static inline void hjo_neutral_delivered(hjo_session *s) {
    if (s->phase == HJO_OFF) s->neutral_pending = 0;
}

// Physical disconnection/reset releases host state; no packet can be delivered.
static inline void hjo_disconnect(hjo_session *s) {
    hjo_stop(s);
    s->neutral_pending = 0;
}
