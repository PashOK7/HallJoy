# MCHOSE M HUB ARM family: experimental support (2026-10-02)

Owner request: "yellow" support for every MCHOSE model we can. This record
covers group B of the [family survey](MCHOSE_FAMILY_SURVEY_2026-10-02.md):
ARM (Thumb, base 0x08008000) boards on the Mix 87 III design. They now run on
the Mix 87 III backend (protocol 27) through a model table. Group A (RISC-V) is
in [MCHOSE_RISCV_FAMILY_2026-10-02.md](MCHOSE_RISCV_FAMILY_2026-10-02.md).

## Models

| Model | VID:PID | Reviewed FW (build) | Keys | Layout preset |
| --- | --- | --- | --- | --- |
| Mix 87 III (Supported, unchanged: exact 1.22 fingerprints, max 341) | 3837:300D | 1.22 | 86+Fn | MCHOSE Mix 87 III ANSI |
| Ace 68 III | 3837:3003 | 1.13 (Mar 15 2026) | 67+Fn | MCHOSE Ace 68 ANSI |
| Ace 68 Air III | 41E4:2132 | 1.16 (Nov 8 2025) | 67+Fn | MCHOSE Ace 68 ANSI |
| Ace 68 Air 2 | 3837:300A | 1.14 (Mar 24 2026) | 67+Fn | MCHOSE Ace 68 ANSI |
| Ace 68 V2 III | 3837:3024 | 1.09 (Mar 9 2026) | 67+Fn | MCHOSE Ace 68 ANSI |
| Ace 68 Turbo 8K | 3837:3028 | 1.06 (Oct 14 2025) | 67+Fn | MCHOSE Ace 68 ANSI |
| Ace 75 8K | 3837:303C | 1.14 (Jun 2 2026) | 80+Fn | MCHOSE Ace 75 8K ANSI (new) |

Not included: Ace 60 (`41E4:2101`, ARM but a different build at 0x08004000
without this dispatcher), Ace 68 GT and Turbo 16K (new WCH core, handlers not
reviewed), Ace 75 16K / Jet 75 III / GOD 60 / Ace 60 Pro ISO FR (no image).

## Evidence (official images; scripts in `.local/research/mchose-mhub-20261002/`)

- `arm_dispatch.py`: the command dispatcher in each image (pending byte,
  64-byte buffer through r5, `5F`/`55` frames, length ≤ 0x38, sum checksum,
  `AA` reply). Same structure as Mix 87 III.
- `arm_replay.py` (Unicorn, the project's `firmware_replay` harness, synthetic
  RAM, stops at the flash writer): through each image's real dispatcher
  - `03`: `AA 03`, length 31, version + build stamp;
  - `E0`: returns the requested bytes of 0x2C000 (settings) and 0x2A000 (base);
  - `06` (profiles 0..3 × flag on/off): reaches the flash writer with
    r0 = 0x0802C000, r1 = 256 and a staged image equal to the stored settings
    with ONLY byte profile×64+7 changed. All 6 images and the reference PASS.
- `arm_replay2.py`: writer modeled as completed (8 KiB page erased, 256 bytes
  programmed): reply `AA 06`, config RAM byte 7 bit 3 follows the flag, no
  reset (AIRCR) write. The replay then waits on peripherals (not modeled).
- `arm_map.py`: flash writer identical to Mix 87 III (40/40 instructions;
  Ace 75 8K 34/40); A0 builder identical through report byte 15 (marker,
  descriptor, travel 6..7 with ≤9 → 0 / ≥max−5 → max, maximum 14..15); the
  differences start at diagnostic bytes 16..19.
- `arm_byte7.py`: readers of config byte 7 sit in the same five places as in
  Mix 87 III; bit 3 is tested only by the A0 service; the others test bits 1/2,
  and one is the `A8` calibration handler (not used by HallJoy). VID:PID found
  in each image.
- Switch maxima: flash table (341) in Air III, V2 III, Turbo 8K; RAM table in
  Ace 68 III, Air 2, Ace 75 8K, so for these models the decoder accepts the
  200..600 range (Mix 87 III keeps the exact 341).
- `arm_keys.json` / `arm_layouts.py`: descriptor tables (84/76/92 slots) give
  67 or 80 HID keys + Fn, no duplicates; every official M HUB layout has exactly
  these keys. The 68-key boards share the Ace 68 geometry (identical after
  translation). Ace 75 8K's 30 knob/screen controls have no A0 slot (omitted).

## Implementation

- `mchose_mix87_protocol.h`: model table (`Models[]`, `FindModel`), key lists,
  `Decode(..., exact341)`.
- `mchose_mix87_backend.cpp`: enumerates all model PIDs; Mix 87 III keeps the
  exact 1.22 + fingerprint admission; the others need a valid `03` reply (version
  logged as `mix87.firmware_version`, model as `mix87.model`) and use the model's
  key list. Same lifecycle (enable at start/resume, disable at pause/exit, one
  automatic enable per generation, padding/readback checks). Telemetry
  name/IDs/key count/layout token per model.
- Layouts: Ace 68 ANSI gains five ARM product tokens; new Ace 75 8K preset
  (`mhub-ace75` source with a regex reader, since its icon fields are JSX).
- Notice group `MchoseArmFamily` (flag 33554432, protocol 27): Ace 68 and Ace 75
  8K preset tokens only (Mix 87 III keeps its Supported state).
- Backup: `.local/backups/mix87-backend-before-family-2026-10-02.tgz`.

## Tests

- `mchose_mix87_protocol_test`: previous checks + model table (exact PIDs, no
  duplicate keys, excluded PIDs) and range/exact maximum decoding: PASS.
- `mchose_mix87_session_test`: previous scenarios + Ace 75 8K session (no
  hashing, any version, model keys, telemetry, layout token, cleanup): PASS.
- No physical test of any ARM model except Mix 87 III.
- Release build (`build_release.ps1`), `check_support_diagnostics.py` and
  `run_native_backend_checks.py --require-compiler`: PASS; full `build.ps1`: PASS.
  EXE SHA-256 `53e7d826aec75808ffd00fdfb4cb77d59ca0e76910b7715962162366f160a250`
  (contains both MCHOSE families).

## Limits

- Same firmware limits as Mix 87 III: A0 is event-serialized, very shallow
  presses are cut off (≤ 0.09 mm), and the analog-mode flag is saved in the
  keyboard (disabled again at pause/exit).
- The replay does not model flash erase/program timing or power loss.

## Ace 68 Air III: Supported (2026-10-05)

Tester log `message (14).txt` (HallJoy 1.6.7, 11:39 UTC): USB product
"Ace 68 Air-III" `41E4:2132`, firmware 1.16 (`mix87.firmware_version=278`),
analog mode enabled automatically, protocol 27 `connected`, 67 mapped keys,
81 updates, 0 failures. Under the owner rule of 2026-10-02 (criteria met, trust
the tester's model) the model is Supported.

- Built-in layout "MCHOSE Ace 68 ANSI" (already selected automatically by the
  `ACE68AIRIII-41E4-2132` identity); `supported_layouts.json` entry
  "Ace 68 Air (III revision)" with `shownAs` "Ace 68": the picker lists this
  shared geometry as "MCHOSE Ace 68 ANSI"; the preset is not renamed because
  saved profiles store preset names.
- Testing notice: `MchoseArmFamily` keeps the shared Ace 68 preset token but
  excludes product ID 0x2132 (`confirmed_devices`); test asserts no notice for
  0x2132 and a notice for 0x300A.
- Sheet (live, read back): the single "Ace 68 Air" row covered three boards.
  Renamed to "Ace 68 Air (II revision)" (`41E4:2120`, Implemented); inserted
  "Ace 68 Air 2" (`3837:300A`, Implemented) and "Ace 68 Air (III revision)"
  (Supported, green) with the batch planner. Full re-read compared with the
  before snapshot: only the renamed B cell changed among existing rows; structure
  PASS (149 blocks), notices PASS (282 yellow), supported layouts PASS (71).
  Snapshots: `.local/research/mchose-ace68air-sheet-20261005/`.
- README Supported list and SUPPORTED_HARDWARE row updated.
- Separate incident in the same tester's first run: HallJoy showed "HallJoy is
  paused" at start and Resume did not help; a HallJoy restart fixed it, and the
  log above is from the working run. Mechanism in code: a failed step of the
  startup Resume transaction rolls back to Paused (generation > 0 shows the
  card), Resume retries the same steps, and no support log is auto-saved because
  the search never ran. Not linked to this keyboard. Owner: no tester build, but
  fix the potential causes: done in STARTUP_PAUSE_FIX_2026-10-05.md.
