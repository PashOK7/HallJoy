# AULA MINI60 HE Pro — first playable native build

> 2026-09-19: [Ordinary NA87 and MINI60 support](NA87_MINI60_STANDARD_SUPPORT_2026-09-19.md) supersedes earlier experimental-only delivery and NA87 frozen status. Other frozen models are unchanged.

Playable experimental build completed; external gameplay validation pending. Owner requested gameplay analog
plus retained logging after hardware logs 13 and 14 confirmed changing depths,
normal keyboard input and three simultaneous analog slots on firmware V1.52.

## Design

The existing isolated, cancellable HID worker now stays in simulation mode while
the normal HallJoy engine is running. No fixed diagnostic stages or capture deadline.
Pause/engine stop closes the session; resume starts a fresh session. HID I/O retains
job containment and bounded recovery. Normal HallJoy profiles, curves, bindings,
SOCD and virtual controller output are used through the native backend registry.
No per-packet pipe/log formatting: an anonymous inherited shared mapping carries
atomic 64-bit per-position samples; the supervisor publishes them to the common
native path. At most one worker owns the wired MINI60 collection.

Admit only 0C45:80A2 / FF68:0061, 65-byte Windows reports and matching device-info
identity (manufacturer 0x0166/product 0x110C). Receiver FEFE receives no commands.
Only 0x10/0x12 reads and temporary 0x66/0x67 are allowed. No firmware flash,
calibration entry, persistent setting write or macro-body read.

## Mapping and normalization

Archived official source at pinned Aether-HE commit
2179fd768fcdbb86f59c4c240b4d4cfcbc18936f, directory
`docs/context/issue-6-mini60-pro-raw/driver_src/`:

- `HFD-D07mGRx8.js`, SHA256 073f00b53a65cd1b25a3346f510512abb518761f6e657581a8a63e52f6b8f79c:
  layout `o` plus master map `y`. Exactly **61 keys**, correcting the third-party
  decode note's count of 62. WASD positions 34/49/50/51 map to HID 26/4/22/7.
  Fn uses HallJoy's extended 0x409 code, not a reserved standard usage.
- `trigger-t6lqQxHu.js`: driver divides keyStroke by 100 (millimetres), default
  travelMax 3.4 mm. The board reports maxStroke=34 and firmware has that constant.
  Native normalization is clamp(travel / (stroke * 10), 0, 1), yielding full scale
  at 340 for the observed board. Measured 369/370 overshoot is clipped. No host
  learning/calibration is performed. Implausible stroke/travel values produce zero.

ANSI geometry is available under the existing Aula brand and selected by the verified
native layout token. Complete 512-byte assignment read is required before publication.
Automatic mode applies supported live keyboard assignments; manual layouts use the
factory map. Complex macros/multi-key assignments are not falsely represented as a
single analog key. Several physical keys assigned to one usage aggregate by maximum;
releasing one must not release another. Reconnect/resume refreshes assignments.

## Release and logging policy

A received zero releases immediately. Every physical position independently expires
after 50 ms without an update; other keys cannot keep it pressed. This is a conservative
initial gameplay policy against the firmware's silent near-rest region, using the
observed maximum host-receipt gap of 16 ms. It is not proof of lossless firmware releases.
The first gameplay log should reveal whether this threshold causes false releases or
perceptible release delay. Worker failure, disconnect and session restart clear input.

One HallJoy.log retains protocol summaries, firmware identity, raw ranges, live map,
normalized-path update counts, active-key counts, fallback release counts and clean-up.
Raw summaries repeat every 30 seconds; native aggregate checkpoints every 5 seconds;
no typed sequence, serial or HID path is logged. Startup/close preserve partial evidence.

## Validation

- Release x64 MSVC build: PASS, including source-encoding audit. Existing ViGEm
  missing-PDB linker warning only.
- Native backend static gate: PASS (`.local/mini60-native-static-current.txt`).
- Portable native model, Windows direct-append/privacy log and configured XUSB
  builder tests: PASS.
- Final EXE embedded self-test: PASS. Exercises actual native publication through
  registry reads and configured gamepad report construction, independent stale
  release, remapping aliases, scaling, tick rollover, process cancellation/failure,
  protocol parsing and aggregate logging. No hardware accessed.
- No local hardware/gameplay or visual run. V1.52 hardware logs 13/14 establish
  changing depths and three simultaneous slots, not complete gameplay validation.

## Delivery

`build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe`

Size: 9305600 bytes. SHA256:
`1ca6a4e2bdd37d27812f3e959771fc7d935b2021e0454c3f6769fb870e1280a3`.

Tester connects the keyboard by USB, runs the usual HallJoy, configures bindings
and plays, then closes HallJoy and returns the adjacent HallJoy.log. No minimum
capture duration or fixed completion deadline. Acknowledgement/data establishes
stream startup; absent startup proof retries automatically. Transport timeouts
remain bounded and are not a user typing schedule.

Pre-change backup: `.local/backups/mini60-native-before-20260917-170253.zip`.
Machine-readable final verification: `.local/aula-mini60-native-verification.json`.

