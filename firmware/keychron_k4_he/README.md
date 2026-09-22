# K4 HE onboard integration work

Experimental firmware, not a release. Current checkpoint:
`docs/current/K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md`.

Base: Keychron/qmk_firmware commit
`ee7390c3bbdc1f71a1cc8d54323f3f1d97868593`.
Apply `base.patch` to a clean base, copy `halljoy_onboard.c/.h` and
`usb_descriptor_override.c` into `keyboards/keychron/common/analog_matrix/`.
The patch expects the QMK checkout at `.local/research/k4-lowlatency` so the
shared C11 headers resolve from `src/HallJoyProject/HallJoy` in this project.
Build target `keychron/k4_he/ansi:keychron`. Keep pinned QMK submodules and
ARM GCC recorded in the checkpoint. Never patch the preserved legacy UAP tree.

Protocol A9/70..7D: capabilities, open, staged profile begin/chunk/commit,
start, heartbeat, stop, telemetry, explicit maintenance DFU, actual pad report
and compact burst telemetry. Status byte17 bit2 advertises A9/7B: one request,
six 22-byte fragments of a CRC-protected 130-byte coherent snapshot. Exact
0..240 travel is preserved; the legacy 12-page transport remains available. See the source
for byte layouts. HJO1 response magic and USB revision are mandatory admission
gates; do not send activation/maintenance commands to stock or legacy UAP.
Profiles use HJP1 + explicit little-endian floats + CRC32; no packed C ABI.
Profile persistence remains on the PC, active session is never persistent.

r5 keeps the legacy scanner and digital/rapid-trigger processing, but native
gamepad output uses fresh pre-gate ADC values and floating calibrated travel.
The old5-count gate and uint8 travel rounding no longer affect onboard output.
The polynomial/per-key calibration are preserved; no temporal smoothing added.
Existing keyboard UI telemetry remains0..240; the actual controller tester
reads the high-precision output independently. Status byte17 bit3 advertises
the precision/capture capability. A9/7C with RAW1 and slot starts512 consecutive
raw samples in RAM; A9/7D reads them. Capture only allowed while OFF; its buffer
shares profile staging and is invalidated on OPEN. No automatic file logging.
Telemetry acquisition timestamps currently have millisecond resolution.
It does not establish1000fresh scans/s. Removing the analog delta gate changes
onset sensitivity; actual physical latency/noise limits are not fully qualified.
The first experimental HallJoy host is built with tools/build_keychron_onboard.ps1.
Owner confirmed the first application/profile works; r4 separates the actual
Windows controller monitor from key telemetry and accelerates key snapshots.
Scanner redesign and physical onset/noise qualification remain pending.

The native XInput interface appears only during a leased session. Ordinary
mode excludes both XInput and generic HID joystick. Mode changes re-enumerate
the whole keyboard, explicitly accepted by the owner. Existing Keychron settings
and native game-controller mode do not override HallJoy output ownership.

Hardware probe: `tools/keychron_onboard_probe.py cycle` uploads an unbound
neutral profile and tests watchdog removal without generating game actions.
Current firmware supports a guarded maintenance command for later DFU entry.

Do not write an ordinary QMK binary across the emulated EEPROM sector without
preserving settings. Sector 0x08004000..0x08007fff must be copied from the exact
connected-device dump when constructing a full image. Strip the 16-byte DFU
suffix before overlaying executable bytes. Record hashes and verify readback
before boot. Full backup excludes the separate external calibration EEPROM.
