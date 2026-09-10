# Keychron K4 HE onboard HallJoy design - 2026-08-30

## Goal

Run the latency-critical HallJoy transform on the K4 HE itself. The keyboard
reads Hall sensors, applies HallJoy curves and axis conflict policy, and sends
its native XInput report directly to the game. HallJoy on Windows remains the
profile editor, uploader, and all-key visual monitor; it is not in the runtime
game-input path and does not create a ViGEm device for this keyboard.

## Why this route

The connected K4 HE (`3434:0E40`) has an STM32F401, public QMK source, an
existing analogue scan, a vendor HID configuration interface, and a native
XInput report path. The stock external `AMC_GET_REALTIME_TRAVEL` round trip
measured 2-3 ms, so using it for real-time PC-side transformation is slower
than executing from the already sampled matrix on the MCU.

## Firmware responsibilities

- Read the existing 6x19 Keychron analogue matrix.
- Normalize each used key travel to `[0, 1]`.
- Apply the same HallJoy curve definition as the editor: segmented or rational
  cubic Bezier, per-key override, output cap, deadzones, and invert.
- Combine configured keys into two triggers, four signed stick axes, and XInput
  buttons.
- Implement the exact current HallJoy Snap Stick and Last Key Priority state
  machine from `configured_xusb_builder.cpp` on the MCU.
- Send a native XInput packet only when the resulting packet changes.
- Preserve an ordinary keyboard fallback and a recoverable DFU path.

## Host responsibilities

- Keep the existing HallJoy configuration UI and visual curve editor.
- Translate HID-based HallJoy bindings to K4 matrix slots during upload.
- Build one versioned, checksummed onboard profile and upload it atomically.
- Read low-rate visual telemetry only. Telemetry cannot drive the game output.
- Show an explicit `Onboard K4 HE` status, profile CRC, firmware capability
  revision, and native-XInput ownership so users cannot accidentally enable
  the ViGEm route at the same time.

## Vendor-HID protocol v1

All commands retain Keychron's `A9` packet prefix and use the custom range
`60..65`, which stock analogue-matrix firmware does not dispatch.

| Command | Direction | Purpose |
| --- | --- | --- |
| `A9 60` | request/reply | capabilities, protocol version, matrix identity |
| `A9 61` | request/reply | begin staged profile upload: version, length, CRC32 |
| `A9 62` | request/reply | write one bounded staged chunk at an explicit offset |
| `A9 63` | request/reply | validate CRC and atomically commit the staged profile |
| `A9 64` | request/reply | read active profile metadata / profile bytes |
| `A9 65` | request/reply | read one low-rate page of raw and transformed visual values |

The configuration is double-buffered: a bad, partial, or unplugged upload never
changes the active game profile. Firmware sends a neutral XInput frame before
and after an accepted profile switch.

## Profile v1

The persistent profile contains a magic, protocol revision, total length and
CRC32; flags (enabled, keyboard suppression, Snap Stick, LKP); LKP sensitivity;
four two-key axis bindings; two trigger bindings; button masks; and an explicit
curve record for each physical K4 key. Curve values use fixed normalized units
on the wire. Firmware converts them once into an FPU-ready runtime definition.

For strict UI/firmware matching, the initial implementation uses the same
rational-Bezier evaluation formula and 18 bisection iterations as HallJoy. A
host-side golden-vector test must compare every uploaded curve and all LKP/Snap
Stick transitions against `configured_xusb_builder.cpp` before a keyboard is
flashed.

## Staged implementation

1. Add a standalone QMK `halljoy_onboard` module that compiles but is disabled
   by default. It provides protocol capability replies and an in-RAM profile.
2. Port and unit-test the pure curve and Snap Stick/LKP logic against HallJoy
   golden vectors.
3. Route its output to Keychron's existing native XInput transport, gated by
   the onboard-profile enabled flag.
4. Add transactional EEPROM persistence and read-only telemetry pages.
5. Add HallJoy upload/visual mode, then perform physical latency and recovery
   tests before any default enablement.

No device firmware is flashed during stages 1-4. The stock official image and
the upstream source baseline tag are retained for recovery.

## Implementation status

The separate `halljoy-onboard-k4he` source branch now contains the stage-1
transport. It handles `A9 60..64`, returns an `HJO` reply marker, and uses a
bounded 1536-byte RAM staging area with per-byte receipt tracking and CRC32
validation. It has no enable command, no EEPROM writes, and no matrix or
XInput hook, so this build cannot change typing or game-controller behaviour.

Correction: this branch was initially based on the official Keychron source
commit and is therefore not a valid final base. The K4 already has a
HallJoy/UAP full-report candidate which adds `A9 31` (four vendor HID frames
for the complete 6x19 matrix), stored as
`build/firmware/keychron-k4he-ansi-full-report-20260807/`
`keychron_k4_he_ansi_halljoy_full_report.bin` in the earlier HallJoy worktree.
All subsequent on-board work must preserve and build on that `A9 31` ABI; the
current branch is an inert protocol prototype only and must not be flashed.

The active source branch is now based on the exact `2025q3` commit
`ee7390c3bbdc1f71a1cc8d54323f3f1d97868593`, with a local recovery tag
`halljoy-k4he-a931-upstream-base`. It contains the compatible read-only
implementation: `A9 01` ends in marker `0x45`, and one `A9 31` request emits
four untagged `A9 31` reports with 30 travel bytes each (114 matrix values plus
six zero padding bytes). The HallJoy on-board transport remains inactive and
is layered after that established UAP path.

The local ARM GCC toolchain is present. The old Keychron make flow still needs
its matching MSYS QMK CLI environment before it can compile this prototype.

## Delivery roadmap

### 0. Reproducible base and recovery

- Freeze the exact `ee7390...` / `A9 31` source and keep the known-good UAP
  image and stock image as separately hash-verified recovery artefacts.
- Establish a reproducible QMK/MSYS build that produces an ELF, BIN and size
  report. No DFU operation is permitted in this phase.
- Add a firmware-side build/static gate for the four-frame `A9 31` ABI.

Exit: a clean checkout builds repeatably and still exposes the known UAP path.

### 1. HallJoy on-board profile ABI

- Finalize a versioned, bounded binary profile: enabled/suppression flags;
  four signed axes; two triggers; all 15 standard buttons; Snap Stick, LKP and
  sensitivity; and one rational-Bezier curve per physical matrix slot.
- Retain transactional upload: begin, ordered-or-unordered chunks, receipt
  bitmap, CRC32, validation, atomic activation and explicit error replies.
- Add readback/telemetry commands and a profile revision/generation counter.

Exit: malformed, incomplete and stale profiles cannot alter active input.

### 2. Deterministic STM32 engine

- Port `curve_math.cpp` exactly: normalized wire values, rational weights,
  18-step inverse-X search and endpoint/clamp rules.
- Port `configured_xusb_builder.cpp` exactly: threshold, transition state,
  valley re-triggering, LKP and Snap Stick tie handling.
- Create golden vectors shared with HallJoy and run them against a host-compiled
  C engine; include curves, all simultaneous-direction cases and re-presses.

Exit: firmware engine matches HallJoy's frame output bit-for-bit for every
golden vector.

### 3. Native input ownership and 1000 Hz output

- Run the engine once after each completed 6x19 matrix scan; never issue USB
  reads or allocate memory in that path.
- Send Keychron's native `report_xinput_t` only when its complete frame changes.
- Disable the stock game-controller action path while the on-board profile is
  active, and mask only profile-bound matrix slots from normal keyboard reports.
- Keep unbound keys as ordinary keyboard keys and send a neutral XInput report
  on profile disable, disconnect and invalid profile activation.

Exit: no duplicated keyboard/XInput events; USB capture confirms a maximum
1 ms report cadence under changing input.

### 4. Persistence, fail-safe and telemetry

- Add two-slot EEPROM storage with header, generation, CRC and write-last
  commit marker; preserve a valid prior profile on power loss.
- Add a physical/boot-time safe escape that disables on-board mode without
  erasing calibration or the UAP reader.
- Provide read-only telemetry: scan counter, active generation, frame counter,
  output interval extrema and raw/curved values for HallJoy's visual UI.

Exit: interrupted writes recover safely, calibration survives, and a user can
always return to normal typing without reflashing.

### 5. HallJoy support

- Add K4 HE on-board discovery via `A9 60` and verify the `HJO` marker; never
  mistake a stock echoed packet for support.
- Compile current HallJoy settings to the versioned slot-based profile using
  the existing exact K4 ANSI layout mapping; upload atomically and verify CRC.
- In on-board mode, use `A9 31` only for UI visualization/telemetry, not for
  gamepad output. Disable ViGEm ownership for this device.
- Present firmware/profile status, rollback-safe errors and a clear distinction
  between stock UAP mode and on-board native-XInput mode.

Exit: changing a HallJoy setting updates the keyboard profile and the UI shows
the same values the STM32 uses.

### 6. Hardware qualification and release

- Bench: input-to-XInput latency, jitter, scan frequency, sustained diagonal,
  all 15 buttons, trigger range, rapid re-press and long simultaneous holds.
- Regression: wired/2.4 GHz modes where supported, reconnect, sleep/wake,
  profile writes under load, reboot during write, UAP visualization and stock
  rollback.
- Release only a model/layout-locked image with firmware hash, source commit,
  recovery instructions and measured evidence. Never publish a universal image.

Exit: the measured native path is stable at 1000 Hz and all recovery tests pass.
