# Generic protocol support and default layout (2026-10-07)

Owner request: yellow support for every keyboard that safely speaks a
protocol HallJoy already implements but is not in the catalog, detected by
HallJoy itself; and show an anonymous 100% layout by default.

## Scope decision (safety)

| Family | Generic? | Why |
| --- | --- | --- |
| BY/IPI UUID platform (VID 372E: IPI, QBZ, Royal Kludge, FuryCube, AULA BY boards) | Yes | Read-only commands only (`82 01` UUID, `83 00` map, `94 02` live, `94 05` calibration); the keyboard describes its own keys; BY key IDs share one factory table. |
| Logitech HID++ `0x1B08` | Yes | Self-describing feature lookup; `0x1B08` stream is a volatile on/off; keys are learned at run time. |
| RongYuan RY5088 (3151:...) | No | Same VID/PIDs as ATTACK SHARK with a different protocol; a generic path would send the stream-enable command to them; board numbers collide across OEM catalogs. Exact boards and reviewed aliases only. |
| W669, MCHOSE Jet75/Mix87, Hex80, others | No | Need exact factory profiles or perform settings writes. |

## BY/IPI generic path (`addressed_analog_backend.cpp`)

- Any `372E` PID (except AULA HERO84 HE `103E`, which has its own backend) is
  asked for its BY UUID. Without a UUID reply, unlisted PIDs keep the previous
  addressed path (listed PIDs 105C/106C/10BF/10C0 keep their rejection).
- UUID in the verified catalog: unchanged exact path.
- UUID outside the catalog: `ReadGenericIpiProfile`. IDs 1..127 are probed one
  at a time with `83 00` and `94 02`; an ID counts only when both echo it.
  Firmware audit (FuryCube M35HE image, same handlers as the IPI/RK set):
  both handlers skip IDs missing from the firmware ID table (lookup returns
  0xFF), so the echo is a presence test. `94 05` does NOT check (it would read
  an arbitrary cell), so calibration is asked only for present IDs; keys with
  invalid calibration are dropped; at least 8 keys are required. Gaps of
  absent IDs are normal; the probe stops early only if the keyboard never
  answers this framing.
- Physical identity: the BY factory table where it has an entry, otherwise
  the key's own live code (e.g. M35HE ID 112). No layout token, so automatic
  layout keeps the manual/default layout; no remap publication.
- Log: `ipi.generic_profile` value = UUID, detail = present << 16 | calibrated.

## Logitech generic path (`logitech_rapid_backend.cpp`)

- Enumeration takes every `046D` vendor HID++ collection. Catalog models
  (C35B, C364) run exactly as before. Other devices get one quiet, read-only
  root `getFeature(0x1B08)` (not counted as a failure; mice, receivers and
  headsets answer with an error or not at all) and are remembered as rejected
  until the next device change. They count as present only after reporting
  `0x1B08`, so a Logitech mouse never raises a keyboard notice.
- A device with `0x1B08` runs the normal session with `kNoKeyMap`: every key
  is learned at run time (`V2KeyLearner`, now with a configurable table), for
  version 0 single-key events and version 2 frames. Unknown versions keep the
  content-bounded raw capture. Raw capture and `capture.key` also run for
  generic version 0/2 sessions until the store is full, so a log completes a
  static table later. Log: `logitech.generic_analog`, `logitech.generic_model`.

## Notice

New flag `GenericProtocol` (536870912), outside the Sheet-synchronized model
catalog: telemetry field `genericProtocol` (backend -> UI) when a family match
is connected. Title "Keyboard not in the list: hardware testing incomplete";
body explains that key positions come from the keyboard and asks for the log.

## Default layout

Without saved settings (and whenever a manual preset cannot be resolved) the
active preset is now "Generic 100% ANSI" (`DefaultPresetIndex`), not catalog
entry 0 (DrunkDeer A75 Pro). Automatic layout still selects a model layout for
catalog keyboards; generic keyboards keep the default.

## Validation

- `build_release.ps1` EXIT=0 (all gates: full catalog image check, profile
  transactions, startup recovery, layout catalog, supported layouts,
  diagnostics release gate); `research_reference_checks.py --record` (all
  original audits passed); `run_native_backend_checks.py --require-compiler`
  PASS after updating two static audits that encoded the old default
  ("preset zero is the startup default") to the owner's new rule.
- `logitech_rapid_protocol_test`: learner with `kNoKeyMap` learns a key that
  the X2 table would know.
- Generic BY discovery and the Logitech quiet probe have no device test (no
  hardware); firmware evidence for the BY presence test is the M35HE image.
- EXE SHA-256 `b8f4335855ec9550a92b6f9008691208b2d3c4345ea960b69dac569c88caee12`.
