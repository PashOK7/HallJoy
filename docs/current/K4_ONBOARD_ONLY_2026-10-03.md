# K4 HE with HallJoy firmware: onboard route only, no UAP fallback (2026-10-03)

## Owner request

- Make the K4 connection stable.
- When the keyboard runs HallJoy firmware, HallJoy must use the best protocol
  only, never the UAP fallback.
- Re-enumeration is acceptable only at the moment HallJoy starts or exits.

Rejected options:

- **Deferred stop** (a short park lease after exit): the keyboard would drop
  later, while the user types elsewhere.
- **Permanent controller in the USB descriptor:** Windows would always list a
  controller.

Firmware unchanged (r8).

## Cause of the fallback

A previous HallJoy exit stops the session. The firmware removes the XInput
interface and the whole USB device re-enumerates (about 1 s; all interfaces
are one USB configuration). A HallJoy started inside that window found no K4
node. `Prepare` waited only while a node was present, so it gave up at once
and UAP took the K4 for the whole session. Typical case: the build script
closes and relaunches HallJoy. The owner's normal start, seconds later,
claimed onboard. This explains the long "Analog host" text in the Gamepad
Tester.

## Firmware marker (already present, no firmware change)

HJO1 firmware reports USB release 0x1212 (keyboard mode) or 0x1213 (native
pad). The stock firmware reports 0x0111. The serial number also has the
suffix `HJO1`. Windows exposes the release in the hardware IDs (`REV_121x`),
readable from metadata without opening the device.

## Host changes

- `keychron_onboard_channel.cpp`:
  - `K4HjoVendorInterfaces()`: present HID collections `FF60:0061` whose
    hardware IDs are VID_3434&PID_0E40 with REV_1212 or REV_1213;
  - `K4HjoRecentlyRemoved(ms)`: a not-present USB node with REV_121x whose
    `DEVPKEY_Device_LastRemovalDate` is within the window.
- `Prepare()`:
  - when an HJO K4 was removed within 5 s, it waits up to 4 s even while no
    node is present (log `k4.onboard_prepare_waited_return`);
  - on failure, the HJO vendor collection is claimed for KeychronOnboard
    (`k4.onboard_reserved_for_retry`). The UAP child excludes claimed paths, so
    HJO firmware is never driven by UAP;
  - stock firmware has no REV_121x and keeps UAP.
- `app.cpp`, late takeover:
  - `KeychronOnboard_NeedsTakeover()` is true when an HJO K4 is present but
    the current generation does not own it;
  - checked 700 ms after device changes and after the engine becomes Active;
  - action: one automatic Pause+Resume (`k4.onboard_late_takeover`). The
    Paused state is not shown to the user;
  - at most 2 attempts while the K4 stays present; the counter resets when
    it leaves or is owned;
  - a user's Pause is never overridden.

## Validation

- `build_release.ps1` PASS, no warnings. Native suite PASS.
- One physical observation: after the build script relaunched HallJoy
  (10:34:39), the K4 USB device reported REV_1213, with the Xbox 360
  controller interface present: onboard route active. The earlier 10:02
  relaunch had fallen back to UAP. This is a single observation, not a
  repeated stress test.
- EXE SHA-256
  `b73f1d689ef4a032c671d08a6d3ba1b254368439754397bd62cffdd80d740453`.

## Consequence

If the onboard route keeps failing (for example, another program holds the
vendor interface), an HJO K4 now has no analog input instead of the UAP
fallback. This is the owner's explicit choice. The support log events above
identify the stage.
