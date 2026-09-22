// SPDX-License-Identifier: GPL-2.0-or-later
// Single main-task owner. USB ISR only reads the descriptor mode flag.
#include "quantum.h"
#include "bootloader.h"
#include "analog_matrix.h"
#include "usb_main.h"
#include "transport.h"
#include "halljoy_onboard.h"
#include "keychron_onboard_session.h"
#include "keychron_onboard_profile.h"
#include "keychron_onboard_compact.h"

extern usb_endpoint_in_t usb_endpoints_in[USB_ENDPOINT_IN_COUNT];
extern const matrix_row_t analog_matrix_mask[];
extern uint8_t calibrated;

static hjo_session session;
static hjo_profile profile;
static hjo_mapper_state mapper;
static bool profile_ready, uploading;
static uint32_t profile_crc;
typedef struct {uint16_t raw,depth,scan_us,zero,full;uint8_t travel,valid;} hj_capture_sample;
// Capture is allowed only OFF; OPEN invalidates it before profile staging.
static union {uint8_t staging[HJO_PROFILE_BYTES];hj_capture_sample capture[512];} scratch;
#define staging scratch.staging
#define capture scratch.capture
static uint8_t received[(HJO_PROFILE_BYTES + 7) / 8];
static uint8_t response[32];
static bool response_pending;
static volatile bool native_descriptor;
static bool reconnect_pending;
static uint32_t reconnect_at;
static float values[HJO_SLOTS];
static uint8_t telemetry[HJK4_FRAME_BYTES];
static uint8_t compact[HJK4_COMPACT_BYTES], burst_next_page;
static uint32_t scan_sequence, last_scan_us, maximum_scan_us, acquisition_end_us;
static volatile bool usb_reset_seen;
void halljoy_onboard_usb_reset(void) { usb_reset_seen = true; }
static report_xinput_t last_report;
static bool have_last_report, neutral_submitted, boot_pending;
static uint32_t boot_at;
// Explicit bounded raw capture; dormant during ordinary gameplay.

static uint16_t capture_count;
static uint8_t capture_slot;
static bool capture_running;
static uint32_t capture_first_scan;


// At most one packet in flight: if busy, keep the new state in our own task,
// not in a USB FIFO. Full-buffer post is bounded and has no retry loop.
static bool try_packet(usb_endpoint_in_lut_t index, const void *data, size_t size) {
    usb_endpoint_in_t *ep = &usb_endpoints_in[index];
    bool sent = false;
    osalSysLock();
    if (usbGetDriverStateI(ep->config.usbp) == USB_ACTIVE &&
        !ep->obqueue.suspended && !usbGetTransmitStatusI(ep->config.usbp, ep->config.ep) &&
        obqIsEmptyI(&ep->obqueue) && ep->obqueue.ptr == NULL &&
        size > 0 && size <= ep->config.buffer_size &&
        obqGetEmptyBufferTimeoutS(&ep->obqueue, TIME_IMMEDIATE) == MSG_OK) {
        memcpy(ep->obqueue.ptr, data, size);
        obqPostFullBufferS(&ep->obqueue, size);
        sent = true;
    }
    osalSysUnlock();
    return sent;
}

bool halljoy_onboard_native_descriptor(void) { return native_descriptor; }

static void schedule_mode(bool enabled) {
    if (native_descriptor == enabled && !reconnect_pending) return;
    native_descriptor = enabled;
    reconnect_pending = true;
    reconnect_at = timer_read32();
    have_last_report = false;
}

static bool physical(unsigned slot) {
    return slot < HJO_SLOTS && (analog_matrix_mask[slot / MATRIX_COLS] &
           ((matrix_row_t)1 << (slot % MATRIX_COLS)));
}

bool halljoy_onboard_suppressed(uint8_t row, uint8_t col) {
    return session.phase == HJO_ACTIVE && profile_ready &&
           (profile.mapping.flags & HJO_SUPPRESS) &&
           hjo_bound(&profile.mapping, row * MATRIX_COLS + col);
}

static bool all_received(void) {
    for (unsigned i = 0; i < HJO_PROFILE_BYTES; ++i)
        if (!(received[i / 8] & (1u << (i % 8)))) return false;
    return true;
}

static bool valid_physical_bindings(void) {
    // Validate staging before touching active profile. Wire slots are bytes.
    for (unsigned i = 12; i < 22; ++i)
        if (staging[i] != HJO_UNBOUND && !physical(staging[i])) return false;
    for (unsigned i = 0; i < HJO_SLOTS; ++i)
        if (hjk4_u16(staging + 24 + 2*i) && !physical(i)) return false;
    return true;
}

static void burst_page(uint8_t page) {
    memset(response,0,sizeof(response));
    response[0]=0xA9;response[1]=0x7B;response[3]=(uint8_t)session.phase;
    hjk4_put32(response+4,session.generation);response[8]=page;
    const unsigned offset=page*22u;
    const unsigned count=HJK4_COMPACT_BYTES-offset<22?HJK4_COMPACT_BYTES-offset:22;
    response[9]=(uint8_t)count;memcpy(response+10,compact+offset,count);
    response_pending=true;
}

// Commands A9/70..78 are reserved for HJO1 and always return tagged state.
// Profiles are RAM-only: HallJoy owns persistence. No EEPROM writes here.
bool halljoy_onboard_rx(uint8_t *data, uint8_t length) {
    if (length != 32 || data[0] != 0xA9 || data[1] < 0x70 || data[1] > 0x7D) return false;
    // Host sends one request at a time. Busy responses are dropped so the host
    // retries with its bounded timeout; never block scanning to queue replies.
    if (response_pending) return true;
    hjo_tick(&session, timer_read32());
    const uint8_t command = data[1];
    const uint32_t token = hjk4_u32(data + 4);
    uint8_t status = 0;
    uint8_t page = data[2];
    switch (command) {
        case 0x70: break; // capabilities/status
        case 0x7C: // RAM-only raw capture, requires exact explicit marker
            if(session.phase!=HJO_OFF || memcmp(data+8,"RAW1",4) || !physical(data[2])) {status=7;break;}
            capture_slot=data[2];capture_count=0;capture_first_scan=0;capture_running=true;
            break;
        case 0x7D:
            if(session.phase!=HJO_OFF || hjk4_u16(data+12)>=512)status=7;
            break;

        case 0x7B: { // one coherent compact snapshot, six unsolicited response pages
            uint8_t travel[HJO_SLOTS];
            for(unsigned i=0;i<HJO_SLOTS;++i)
                travel[i]=physical(i)?MIN(analog_matrix_get_travel(i/MATRIX_COLS,i%MATRIX_COLS),240):0;
            hjk4_compact_encode(compact,session.generation?session.generation:1,scan_sequence,
                (uint16_t)(last_scan_us?MIN(last_scan_us,65535):1),calibrated?HJK4_CALIBRATED:0,travel);
            burst_next_page=1;burst_page(0);return true;
        }

        case 0x7A: break; // last submitted native gamepad report, read-only
        case 0x71: // OPEN and expose native XInput by re-enumerating
            if (memcmp(data+8,"HJO1",4) || get_transport() != TRANSPORT_USB || !hjo_open(&session, timer_read32())) status = 1;
            else { capture_running=false;capture_count=0;profile_ready = false; uploading = false; schedule_mode(true); }
            break;
        case 0x72: // BEGIN profile upload
            if (!token || token != session.generation || session.phase == HJO_OFF) status = 2;
            else { memset(received, 0, sizeof(received)); uploading = true; }
            break;
        case 0x73: { // CHUNK: token[4:8], offset[8:10], count[10], bytes[11:32]
            const unsigned offset = hjk4_u16(data + 8), count = data[10];
            if (!uploading || token != session.generation || session.phase == HJO_OFF ||
                !count || count > 21 || offset + count > HJO_PROFILE_BYTES) { status = 2; break; }
            for (unsigned i = 0; i < count; ++i) {
                staging[offset+i] = data[11+i];
                received[(offset+i)/8] |= (uint8_t)(1u << ((offset+i)%8));
            }
            break;
        }
        case 0x74: // COMMIT: validated before changing any active profile fields
            if (!uploading || token != session.generation || session.phase == HJO_OFF ||
                !all_received() || !valid_physical_bindings() ||
                !hjo_profile_decode(&profile, staging, sizeof(staging))) { status = 3; break; }
            profile_crc = hjk4_u32(staging + HJO_PROFILE_BYTES - 4);
            profile_ready = true; uploading = false;
            memset(&mapper, 0, sizeof(mapper)); have_last_report = false;
            break;
        case 0x75: // START after reconnect and complete validated profile
            usb_reset_seen = false; neutral_submitted = false;
            if (!profile_ready || reconnect_pending || get_transport() != TRANSPORT_USB ||
                !hjo_start(&session, token, hjk4_u32(data+8), timer_read32())) status = 4;
            break;
        case 0x76:
            if (!hjo_heartbeat(&session, token, hjk4_u32(data+8), timer_read32())) status = 4;
            break;
        case 0x77:
            if (!hjo_host_stop(&session, token)) status = 4;
            break;
        case 0x79: // explicit maintenance command, never sent by normal discovery
            if (memcmp(data+4,"HallJoyDFU-K4-v1",15)) status=6;
            else { hjo_stop(&session); boot_pending=true; boot_at=timer_read32(); }
            break;
        case 0x78: { // coherent telemetry: capture page zero, read remaining pages
            if (page >= 12) { status = 5; break; }
            if (!page) {
                uint16_t depth[HJO_SLOTS];
                for (unsigned i=0;i<HJO_SLOTS;++i)
                    depth[i] = physical(i) ? (uint16_t)((uint32_t)MIN(analog_matrix_get_travel(i/MATRIX_COLS,i%MATRIX_COLS),240)*65535u/240u) : 0;
                hjk4_encode(telemetry,sizeof(telemetry),HJK4_DEPTH_FRAME,
                    session.generation ? session.generation : 1,scan_sequence,acquisition_end_us,
                    (uint16_t)(last_scan_us ? (last_scan_us > 65535 ? 65535 : last_scan_us) : 1),
                    calibrated ? HJK4_CALIBRATED : 0,depth);
            }
            break;
        }
    }
    memset(response, 0, sizeof(response));
    response[0]=0xA9; response[1]=command; response[2]=status; response[3]=(uint8_t)session.phase;
    hjk4_put32(response+4,session.generation);
    if (command==0x78 && !status) {
        response[8]=page;
        const unsigned offset=page*22u, count=HJK4_FRAME_BYTES-offset < 22 ? HJK4_FRAME_BYTES-offset : 22;
        response[9]=(uint8_t)count; memcpy(response+10,telemetry+offset,count);
    } else {
        memcpy(response+8,"HJO1",4); hjk4_put32(response+12,profile_crc);
        response[16]=native_descriptor; response[17]=(profile_ready ? 1u : 0u) | 2u | 4u | 8u;
        hjk4_put16(response+18,HJO_PROFILE_BYTES);
        hjk4_put32(response+20,scan_sequence); hjk4_put32(response+24,last_scan_us);
        hjk4_put32(response+28,maximum_scan_us);
    }
    if (command==0x7A && !status) {
        memset(response+12,0,20);
        if (session.phase==HJO_ACTIVE && have_last_report)
            memcpy(response+12,&last_report,sizeof(last_report));
    }
    if(command==0x7D && !status) {
        const uint16_t index=hjk4_u16(data+12);
        hjk4_put16(response+12,capture_count);hjk4_put16(response+14,index);
        hjk4_put32(response+16,capture_first_scan+index);
        if(index<capture_count){
            const hj_capture_sample* sample=&capture[index];
            hjk4_put16(response+20,sample->raw);hjk4_put16(response+22,sample->depth);
            hjk4_put16(response+24,sample->scan_us);response[26]=sample->travel;response[27]=sample->valid;
            hjk4_put16(response+28,sample->zero);hjk4_put16(response+30,sample->full);
        }else memset(response+20,0,12);
    }
    response_pending = true;
    return true;
}

void halljoy_onboard_task(uint32_t scan_us) {
    acquisition_end_us=timer_read32()*1000u;
    if (usb_reset_seen) {
        usb_reset_seen=false;
        if (session.phase==HJO_ACTIVE) hjo_stop(&session);
    }
    last_scan_us=scan_us; if(scan_us>maximum_scan_us) maximum_scan_us=scan_us;
    ++scan_sequence;
    if(capture_running) {
        hj_capture_sample* sample=&capture[capture_count];
        const uint8_t row=capture_slot/MATRIX_COLS,col=capture_slot%MATRIX_COLS;
        if(!capture_count)capture_first_scan=scan_sequence;
        halljoy_analog_observation(row,col,&sample->raw,&sample->zero,&sample->full,&sample->valid);
        sample->depth=(uint16_t)lroundf(halljoy_analog_precise(row,col)*65535.0f);
        sample->scan_us=(uint16_t)MIN(scan_us,65535);sample->travel=analog_matrix_get_travel(row,col);
        if(++capture_count==512)capture_running=false;
    }

    hjo_tick(&session,timer_read32());
    if (get_transport()!=TRANSPORT_USB) hjo_stop(&session);
    if (session.phase==HJO_OFF && native_descriptor) {
        memset(&mapper,0,sizeof(mapper));
        schedule_mode(false);
    }
    if (session.neutral_pending && !neutral_submitted) {
        report_xinput_t neutral={0}; neutral.len=0x14;
        neutral_submitted=try_packet(USB_ENDPOINT_IN_XINPUT,&neutral,sizeof(neutral));
    }
    if (boot_pending && timer_elapsed32(boot_at)>=50) bootloader_jump();
    // USB restart is intentionally allowed only at mode boundaries. Normal
    // scans, telemetry and gamepad reports never call the blocking restart.
    if (reconnect_pending && timer_elapsed32(reconnect_at)>=50) {
        response_pending=false;burst_next_page=0;
        restart_usb_driver(&USB_DRIVER);
        reconnect_pending=false;
        if (!native_descriptor) hjo_disconnect(&session);
        return;
    }
    if (session.phase==HJO_ACTIVE && profile_ready && !reconnect_pending) {
        for (unsigned i=0;i<HJO_SLOTS;++i) {
            values[i]=0;
            if (hjo_bound(&profile.mapping,i))
                values[i]=hjo_curve_apply(&profile.curves[i],halljoy_analog_precise(i/MATRIX_COLS,i%MATRIX_COLS));
        }
        hjo_pad pad;
        hjo_map(&profile.mapping,values,&mapper,&pad);
        report_xinput_t report={0};
        report.report_type=0; report.len=0x14;
        report.buttons=hjo_xinput_buttons(pad.buttons);
        report.x=pad.axes[0]; report.y=pad.axes[1]; report.rx=pad.axes[2]; report.ry=pad.axes[3];
        report.left_trigger=pad.triggers[0]; report.right_trigger=pad.triggers[1];
        if ((!have_last_report || memcmp(&report,&last_report,sizeof(report))) &&
            try_packet(USB_ENDPOINT_IN_XINPUT,&report,sizeof(report))) {
            last_report=report; have_last_report=true;
        }
    }
    if (response_pending && try_packet(USB_ENDPOINT_IN_RAW,response,sizeof(response))) {
        response_pending=false;
        if(burst_next_page) {
            if(burst_next_page<HJK4_COMPACT_PAGES) burst_page(burst_next_page++);
            else burst_next_page=0;
        }
    }
}
