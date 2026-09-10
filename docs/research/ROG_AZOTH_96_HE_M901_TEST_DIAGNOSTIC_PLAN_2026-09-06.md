# ROG Azoth 96 HE / M901 tester diagnostic plan

Date: 2026-09-06. Status: scaffolded, not compiled, not distributed and not
qualified for production use.

## Purpose and evidence boundary

The official M901 firmware and Gear Link client establish a normal-mode travel
notification command (`FF00: 51 61`) and travel event (`FFC0`, report ID `03`,
event `7E`). They do not establish a complete key map, physical scale, event
cadence, release behavior, reconnect behavior, or a supported HallJoy output
contract. The tester build therefore must collect evidence only. It must never
publish an analogue value, own a HallJoy HID key, or change a binding.

The calibration command `FF00: 80 26` is prohibited in this build.

## Options considered

| Option | Assessment |
|---|---|
| Add M901 to an ordinary native backend | Rejected: would expose unvalidated key mapping, scale and lifecycle behavior to normal users. |
| Reuse an existing vendor backend | Rejected: no existing protocol owns `FF00`/`FFC0` or decodes `51 61` / `7E`. |
| Dedicated, one-backend diagnostic executable | Chosen: exact interface admission, bounded log-only capture, no gameplay ownership, and no unrelated native probing. |

## Scope of the chosen diagnostic

The opt-in MSBuild property `HallJoyRogAzoth96HeDiagnostic=true` selects a
separate target named `HallJoy-ROG-Azoth96HE-Diagnostic`. It compiles the M901
diagnostic backend and uses a one-backend catalog. The ordinary build excludes
the source file and the M901 descriptor completely.

The diagnostic accepts only this two-interface pair:

| Interface | VID:PID | Usage page:usage | Required report length |
|---|---|---|---|
| Control | `0B05:1C10` | `FF00:0001` | 64-byte input and output |
| Event | `0B05:1C10` | `FFC0:0001` | 21-byte Windows input report (`03` report ID + 20-byte payload) |

After both interfaces are present, it sends exactly one 64-byte control report
whose leading bytes are `51 61 00 00`. It then logs only events whose report ID
is `03` and payload type is `7E`:

```text
03 7E <firmware-key low> <firmware-key high> <travel low> <travel high> ...
```

The log records the firmware key ID, raw unsigned little-endian travel value,
sequence and inter-event interval. It deliberately does not infer a HID key,
millimetres, a release threshold, a polling rate, or a gamepad value.

## Runtime containment verified in source

The diagnostic target is transport-only end-to-end: its dedicated native catalog
is prepared before UAP, but `Backend_Init` returns after the native diagnostic
session has started. UAP, ViGEm output, publication recovery and Raw Input
keyboard registration are not enabled by this variant. Normal Windows keyboard
typing therefore remains owned by the standard keyboard stack, not HallJoy.

Static regression coverage checks the exact VID/PID, usages, report lengths,
normal-mode opcode, event decoder, one-backend catalog, target wiring, and the
absence of a calibration command or build/package artifact. It passed on
2026-09-06. No diagnostic EXE was built, started, or distributed; hardware
evidence remains required.

The paired control/event routing claims are all-or-nothing. If the second exact
interface cannot be reserved, the diagnostic's dedicated catalog clears the
first claim before reporting the device absent; UAP therefore cannot be left
excluding half of an unusable diagnostic pair.

## Tester procedure when the user is ready

1. Build only the dedicated diagnostic target; do not replace the ordinary
   HallJoy executable.
2. Close Gear Link and Armoury Crate so only the diagnostic has the vendor HID
   handles.
3. Confirm normal typing works in a text editor before and during the capture.
4. Start with all keys released; make graded presses and releases of a named
   key, then repeat for several keys and a two-key rollover.
5. Capture a reconnect and stop the diagnostic normally. Do not enter switch
   calibration.
6. Return the diagnostic log together with the typed-key observations and the
   connection mode (wired/RF). No raw log is to be published without the
   tester's consent.

## Acceptance criteria for the next phase

- Standard keyboard output remains usable while `7E` events arrive.
- One firmware-key ID maps consistently to each manually named physical key.
- Travel values have a measured range and release behavior.
- Event cadence and gaps are measured in wired normal and Game Mode.
- Stop/reconnect leaves no blocked device handle or surviving worker.

Only after all five are evidenced may a separate proposal consider an
experimental value-publishing backend. This diagnostic is not that proposal.
