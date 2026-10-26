# K4 HE gamepad over wireless (2.4 GHz / Bluetooth): review and design (2026-10-24)

Owner question: can the owner's Keychron K4 HE (HallJoy HJO1 firmware, r9 series)
provide the HallJoy gamepad while connected wirelessly instead of by cable?
Owner follow-up: the goal is how to make it work, not how it works today. The
design is in "Receiver identity" and "How it can be done" below; a probe firmware
was flashed, measured and rolled back (see "Probe firmware results" and "Conclusion").

Current state: **not with the current firmware and host.** In 2.4 GHz or Bluetooth
mode the keyboard types normally, but HallJoy gets neither the onboard native
gamepad nor analog depth. Wireless mode with the cable still plugged in does not
work either: the firmware refuses the session because the active transport is not
USB. HallJoy host code was not changed.

Sources: firmware delta `firmware/keychron_k4_he/` applied to the pinned Keychron
QMK tree `ee7390c3` (local checkout `.local/research/k4-lowlatency`), host
backend `src/HallJoyProject/HallJoy/keychron_onboard_*.cpp`. No hardware test
was run; on 2026-10-24 the PC listed only the cabled K4 (`USB\VID_3434&PID_0E40\...HJO1`)
and no Keychron receiver or Bluetooth K4 node, present or past.

## What the current path needs

The HJO1 route is one USB device: vendor raw HID `FF60:0061` (control, profile
upload, heartbeat lease of 500 ms, telemetry) plus a native XInput interface
appended to the USB configuration only during a leased session. UAP is never
used for HJO1 firmware (`K4_ONBOARD_ONLY_2026-10-03.md`).

## Firmware: every HallJoy path is USB-only

`halljoy_onboard.c`:

- `0x71` OPEN returns status 1 when `get_transport() != TRANSPORT_USB` (line 170);
  `0x75` START returns status 4 under the same condition (line 206).
- `halljoy_onboard_task` calls `hjo_stop()` whenever the transport is not USB
  (line 294), so switching the mode slider ends a running session.
- Pad reports, neutral reports and replies are written straight to USB endpoints
  through `try_packet()` (`USB_ENDPOINT_IN_XINPUT`, `USB_ENDPOINT_IN_RAW`; lines
  61-66, 301, 330, 346). Nothing goes through the QMK host driver, so nothing can
  reach the wireless module.
- The native gamepad is a USB descriptor change plus `restart_usb_driver()`; a
  radio link has no equivalent.

Keychron wireless code in the same tree (`keyboards/keychron/common/wireless/`):

- `KEEP_USB_CONNECTION_IN_WIRELESS_MODE` is defined for K4 HE (`k4_he/config.h:90`):
  with the cable plugged in and the slider on BT/2.4 GHz the USB device stays
  enumerated (still REV_1212 + serial `...HJO1`), while keys go over the radio.
  HallJoy then finds the K4, but OPEN fails (status 1, see above).
- The LKBT51 module protocol has a raw HID tunnel (`0x91` init, `0x92` RX, `0x93` TX,
  `lkbt51.c:109-111`). Host-to-keyboard raw packets over the radio are compiled
  out: the `EVT_RAW_HID` handler is under `WILRESS_RAW_ENABLE` (`wireless.c:610`),
  which nothing defines. HJO1 commands therefore cannot reach the keyboard over
  2.4 GHz or Bluetooth in this build.
- The module protocol also has XInput commands: `0x19` send report (used by
  Keychron's own game-controller mode through `host_xinput_send` →
  `wireless_send_xinput`) and `0x26` XInput mode (`lkbt51_set_xinput_mode`),
  which nothing in this tree calls. Whether the module/receiver firmware (closed,
  not in this tree) exposes an XInput or HID gamepad interface to Windows is
  unknown.

## Host: the K4 is recognised only as the USB device

`keychron_onboard_channel.cpp:44-58` accepts a device only when the path contains
`vid_3434&pid_0e40`, the HID release is 0x1212/0x1213, the collection is
`FF60:0061` with 33-byte reports and the serial ends with `HJO1`.
`keychron_onboard_gamepad.cpp:25` reads the OS gamepad only for 3434:0E40.
A 2.4 GHz receiver enumerates under its own product ID and a Bluetooth device
has a `BTHLEDevice`/`_VID&02...` path, so neither is matched today.

## Expected HallJoy.log (derived from code, not observed)

- Pure wireless (no cable): no `k4.*` lines; the K4 is simply absent for HallJoy.
- Wireless slider with the cable plugged in: the K4 is claimed, then the session
  OPEN fails every retry (backoff 500 ms → 30 s):
  `k4.session_failed value=2 error=1 detail=1 detail_kind=protocol`
  (value 2 = StepOpen, protocol status 1 = firmware refused OPEN).

## Receiver identity (observed 2026-10-24, keyboard on 2.4 GHz, no cable)

Read from Windows metadata only (no reports sent): one USB device
`USB\VID_3434&PID_D030`, product "Keychron Link", release `0xD300`, empty serial.

| Interface | Collection | Reports (bytes, incl. ID) | Meaning |
|---|---|---|---|
| MI_00 | 0001:0002 | in 8 | mouse |
| MI_01 COL01 | 008C:0001 | in 33, out 33, feature 1001 | receiver service channel (likely update/pairing) |
| MI_01 COL02 | 0001:0004 | in 9 | **HID joystick**, report ID 4: X, Y, Z, Rx, Ry, Rz as 8-bit signed (-127..127), buttons 1-16 |
| MI_02 COL01..04 | 0001:0006, 000C:0001, 0001:0080, 0001:0006 | in 9 / 3 / 2 / 21 | keyboard, consumer, system, NKRO keyboard |
| MI_03 | **FF60:0061** | in 33, out 33 | **raw HID tunnel**, same collection and report size as the K4's USB vendor channel |

No XInput (`IG_`/XUSB) interface is present. The joystick collection matches the
LKBT51 `0x18` joystick command (QMK 6-axis 8-bit joystick, `JOYSTICK_ENABLE = no`
in the K4 build, so it stays idle today). `FF60:0061` matches the LKBT51 raw
tunnel `0x92`/`0x93`, which Keychron Launcher uses over the receiver. The
receiver is the K4's only identity over 2.4 GHz; the HJO1 serial and REV_121x are
not visible through it.

## How it can be done (design, not implemented; flashing needs owner go-ahead)

Common to every route: the HJO1 control channel moves to the radio tunnel.

- Firmware: define `WILRESS_RAW_ENABLE` so `EVT_RAW_HID` reaches
  `analog_matrix_rx` → `halljoy_onboard_rx`; remember the request source and send
  replies through `kc_raw_hid_send(src, ...)` (USB endpoint or LKBT51 `0x93`)
  instead of the fixed `try_packet(USB_ENDPOINT_IN_RAW)`.
- Firmware: a radio session that is allowed when `get_transport() == TRANSPORT_P2P4`,
  with no USB descriptor switch and no `restart_usb_driver()`. Leaving 2.4 GHz
  ends it, like unplugging does today.
- Firmware: pace tunnel sends. `lkbt51_send_cmd` writes SPI without ack or queue,
  and the module reports FIFO half/full states; keyboard reports use a 1 ms
  interval on 2.4 GHz (`DEFAULT_2P4G_REPORT_INVERVAL_MS`). Telemetry must yield to
  key reports so typing never drops.
- Host: a second HJO1 channel on `3434:D030` MI_03 `FF60:0061`, admitted only after
  the HJO1 status reply proves the K4 behind the receiver (receiver identity alone
  is shared by every Keychron Link keyboard). Same 33-byte reports, so the
  existing `KeychronOnboardClient` framing is reused.
- Lease: the heartbeat runs over the radio. Radio dropouts need a grace period
  before the pad goes neutral (the open decision from `K4_MULTIKEY_R9_2026-10-09.md`).

Route A — **analog over the tunnel, HallJoy makes the pad (recommended first).**
The keyboard pushes sparse depth frames (existing `0x7E` format: 5 keys per
32-byte frame, CRC16, already sized for this channel) when values change; HallJoy
feeds them into its normal bindings/curves and outputs through its existing ViGEm
virtual controller. Result: full 16-bit XInput, every HallJoy feature (several pads,
mouse, shortcuts), no Windows controller outside a HallJoy session. Cost: radio
hop + host processing; the exact rate and latency of the LKBT51 SPI → radio →
receiver path are unmeasured.

Route B — **onboard mapper → receiver joystick.** The keyboard's existing mapper
(HJP2 profile) sends LKBT51 `0x18` reports. Windows sees a DirectInput joystick
"Keychron Link": 6 axes at 8 bits (255 levels instead of 16-bit), 16 buttons, no
XInput, so many games ignore it. The collection is always present on the receiver
(Keychron descriptor, not ours). Useful only as a fallback or for DirectInput games.

Route C — **onboard mapper → XInput through the module.** Keychron's tree has
`0x19` (send XInput report) and `0x26` (XInput mode, never called). The receiver
currently shows no XInput interface; whether `0x26` makes it re-enumerate with one
is unknown (closed receiver/module firmware). If it does, this is the closest match
to today's cable behaviour (native XInput, no ViGEm). One guarded experiment
decides it.

Bluetooth: the same tunnel commands exist, but BLE HID has no XInput and the
connection interval is longer (3 ms default report interval). Treat it after 2.4 GHz.

## Probe firmware results (flashed and rolled back, 2026-10-24)

Owner go-ahead: "flash", on condition that the previous working wired firmware can be
restored. The owner uses the K4 wired only; this test answers a user's request.

Probe build: r9stray plus `halljoy_radio_probe.c` (patch
`.local/k4-radio-probe/radio-probe.patch`). Over the radio it serves only probe frames
`A9 7F "HJR1" op` (ping, paced stream of real pre-gate depth, LKBT51 `0x26`, `0x18`,
`0x19`); no VIA/Launcher, no HJO1 session over the radio; cable HJO1 unchanged
(release 0x1212). Built without warnings, raw 145660 bytes.

Flash record (DFU serial `3381347A3035`, same as earlier K4 flashes):

- Pre-flash state over USB with the slider on 2.4 GHz and the cable plugged in
  (`KEEP_USB_CONNECTION_IN_WIRELESS_MODE` keeps USB up): HJO1, phase 0, native 0.
- Two-read backup `.local/backups/k4-radioprobe-flash-backup-20261024.bin`, SHA256
  `bed7ea15…3b1b91d`. Application region and boot sector equal the r9stray image
  (`624612d3…`); only 610 bytes in the settings sector differ (live settings).
- Image builder `.local/k4-radio-probe/k4_make_full.py` first reproduced the r9stray
  image byte for byte from the 2026-10-10 backup, then built the probe image
  `d2eca990…14add6`. Download 262144 bytes, readback identical, `:leave`.
- After boot: HJO1 status OK over USB.

Measurements (agent probe `.local/k4-radio-probe/k4_radio_probe.py`, records
`radio-probe-20261024.jsonl`; keyboard on 2.4 GHz with the cable plugged in, HallJoy closed):

| Test | Result |
|---|---|
| Baseline on r9stray: probe ping through the receiver | no reply (wireless RX compiled out) |
| Probe build: host → receiver → keyboard → receiver → host | works; no `0x91` init needed |
| Ping round trip, 500 pings | 1 lost; min 5.0 ms, median 8.0 ms, p95 11.0 ms, max 69.6 ms |
| Stream, interval 4 ms (keyboard sent 187 frames/s) | 1990/2000 received (0.5% lost), inter-arrival median 5.0 ms, p99 9.0 ms, max 16 ms |
| Stream, interval 2 / 1 / 0 ms | keyboard tops out at ~345 frames/s; 3.4-4.7% lost; p99 6-7 ms, max 8-23 ms |
| Route B: LKBT51 `0x18` joystick | receiver joystick report 4 follows the keyboard exactly (X 100 / -100 / 0, Y 50, button 1) |
| Route C: LKBT51 `0x26` on, then `0x19` report | no XInput interface or XInput slot within ~15 s; mode switched back off |

After a reboot with the cable plugged in, the firmware starts on USB transport and
switches to 2.4 GHz after about 5 s; probe frames sent earlier were answered late.

Not measured: typing while streaming, Bluetooth, battery drain, a receiver re-plug
while module XInput mode is on (the only untested way Route C could still appear).

Rollback (same session): DFU via guarded `A9/79`, download of the pre-flash backup,
readback SHA256 `bed7ea15…` = backup, `:leave`. After boot HJO1 answers over USB and
probe frames get no reply over USB or radio: r9stray is back. The QMK tree sources
were restored (hashes equal the pre-probe copies in `.local/k4-radio-probe/src-before`);
a rebuild differs from the r9stray binary only in 31 build-date bytes.

## Conclusion

Wireless support is technically feasible on 2.4 GHz with the Keychron Link receiver:

- **Route A (recommended):** HJO1 control and sparse full-state depth frames over the
  tunnel, HallJoy makes the XInput pad via ViGEm. About 190 frames/s with 0.5% loss; a
  lost frame is healed by the next one because every sparse frame carries the full
  pressed state. Expected added delay versus cable: roughly half the 8 ms round trip
  plus the frame interval, with rare spikes (max 16 ms at 187 frames/s).
- **Route B (works today at the protocol level):** onboard mapper → receiver
  DirectInput joystick, 8-bit axes, no XInput; only for DirectInput games.
- **Route C:** not available with this receiver as tested.

Implementing Route A needs the firmware radio session, tunnel pacing that yields to key
reports, HallJoy admission of `3434:D030` MI_03 behind an HJO1 reply, and a lease grace
period for radio dropouts. Not started; the owner's keyboard runs r9stray again.

Enumeration command used (PowerShell, read-only):

```
Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match '3434' } |
  Select-Object Status,Class,FriendlyName,InstanceId | Format-Table -AutoSize -Wrap
```

Cable mode remains the only shipped HallJoy gamepad route for the K4.
