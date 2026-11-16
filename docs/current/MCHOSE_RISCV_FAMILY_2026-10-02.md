# MCHOSE M HUB RISC-V family: experimental support (2026-10-02)

Owner request: "yellow" support for every MCHOSE model we can. This record
covers group A of the [family survey](MCHOSE_FAMILY_SURVEY_2026-10-02.md)
(WCH RISC-V, base 0x5000): every board on the same design as Jet 75 II now
runs on the Jet 75 II backend (protocol 29). The ARM group (Mix87 III design)
and the new-core group (Ace 68 GT, Turbo 16K) are tracked separately.

## Models

| Model (HallJoy name) | VID:PID | USB product | Reviewed FW | Keys | Writer reboot rule | Layout preset |
| --- | --- | --- | --- | --- | --- | --- |
| Jet 75 II (Supported, unchanged) | 41E4:211A | Jet75-II | 1.16 | 79+Fn | fresh-flash bit | MCHOSE Jet 75 II ANSI |
| Ace 68 (Ace68-II) | 41E4:2116 | Ace68-II | 1.21 | 67+Fn | fresh-flash bit | MCHOSE Ace 68 ANSI |
| Zero75X | 41E4:211C | ZERO75X-II | 1.14 | 79+Fn | fresh-flash bit | MCHOSE Jet 75 II ANSI |
| Jet 75 I | 41E4:2118 | Jet 75-I | 1.09 | 79+Fn | byte 4 change only | MCHOSE Jet 75 II ANSI |
| Ace 68 (Ace68-I) | 41E4:2114 | Ace68-I | 1.09 | 67+Fn | byte 4 change only | MCHOSE Ace 68 ANSI |
| Ace 68 Air II | 41E4:2120 | Ace 68 Air-II | 1.17 | 67+Fn | byte 4 change only | MCHOSE Ace 68 ANSI |
| Ace 60 Pro | 41E4:2103 | Ace 60 Pro | 1.18 | 60+Fn | byte 4 change only | MCHOSE Ace 60 ANSI |
| Ace 60X I | 41E4:2126 | Ace60X-I | 1.05 | 60+Fn | byte 4 change only | MCHOSE Ace 60 ANSI |
| Ace 60X II | 41E4:2112 | Ace60X-II | 1.04 | 60+Fn | byte 4 change only | MCHOSE Ace 60 ANSI |
| Ace 60 Pro Nordic | 3837:3002 | Ace60 Pro Nordic | 1.07 | 61+Fn (ISO) | byte 4 change only | MCHOSE Ace 60 Pro Nordic ISO |
| Mix 87 I | 41E4:2122 | Mix 87-I | 1.12 | 86+Fn | byte 4 change only | MCHOSE Mix 87 III ANSI |

Not included: Ace 60 (`41E4:2101`, ARM, different build), every ARM board
(Mix87 III design: Ace 68 III / Air III / Air 2 / V2 III / Turbo 8K, Ace 75 8K),
Ace 68 GT and Turbo 16K (new core), and catalog models without an image.

## Evidence per image (static, official M HUB images)

Script `.local/research/mchose-mhub-20261002/verify_group_a.py`
(output `verify_group_a.json`) compares each image with Jet 75 II 1.16:

- `04` / `05` handlers: 16/16 normalised instructions identical in all images.
- `03`: stores the reply length; version halfword + `Mmm dd yyyy` + `,` +
  `hh:mm:ss` (22 bytes) in every image.
- `06` writer: reloads the active profile (stride 64) into config RAM; the
  service gate is exactly reload destination + 7 in all 11 images. New
  generation reloads 64 bytes, older (Jet 75 I, Ace 68 I, Ace 60X I, Mix 87 I)
  32 bytes, which still include byte 7. Reboot rule: settings byte 0 bit 0 for
  Jet 75 II, Zero75X and Ace68-II; otherwise only when profile byte 4's low
  nibble changes, which the one-bit write never does.
- Readers of config byte 7: bit 3 is tested only by the A0 service in every
  image; other readers test bits 1 and 2.
- A0 report: marker, descriptor 1..3, travel 6..7, maximum 14..15, ≤9 → 0 and
  ≥max−5 → max in every builder (Ace 68 Pro, Nordic and Ace 60X II builders
  differ only in instruction order / table stride; checked by hand). Nordic 1.07
  takes switch maxima from a RAM table; the runtime 200..600 range check is
  the guard there.
- USB: every image has the same interface-1 descriptor (usage page 1, usage 0,
  64-byte IN/OUT, no report ID) and its PID equals the catalog entry.
- Factory settings in every image: 4 profiles with `AA BB` at bytes 2..3.
- Mix 87 I: `04` also sets a RAM byte that nothing reads; no effect.

Layouts (`layouts_group_a.py`): each official M HUB layout has exactly the
firmware's A0 key set + Fn. Geometry is shared by Jet 75 I/II + Zero75X,
Ace 68 I/II + Air II, Ace 60 Pro + Ace 60X I/II, Mix 87 I + Mix 87 III, so
those reuse one preset with several product tokens (the catalog forbids
identical same-brand presets). Shared preset names were kept so saved
selections of the Supported models keep working. Nordic is a new ISO preset
with a notched Enter (M HUB draws it as one rectangle over the key to its
lower left).

## Implementation

- `mchose_jet75_protocol.h`: 11-model table with keys, reviewed version,
  names, layout identity and `freshFlashReboot`; `ChangeFlag` takes the
  model's reboot rule (older writers keep the normal readback path).
- `mchose_jet75_backend.cpp`: model passed to `LoadMode` / `ChangeMode` /
  `ModeLease`; everything else unchanged (same lifecycle, no version binding).
- Layout tool: Ace 60 and Ace 60 Pro Nordic sources/reports; extra product
  tokens on the Jet 75 II, Ace 68 and Mix 87 III reports
  (`layout_pipeline.py integrate MCHOSE`, backup
  `.local/backups/layout-integrate-hecp4qie`).
- Notice group `MchoseFamily` (flag 33554432, protocol 29): the five MCHOSE
  preset tokens, with Jet 75 II (`0AC93D48E90B5C30`, PID 0x211A) excluded as
  Supported. Banner title "MCHOSE: hardware testing incomplete".
- README / SUPPORTED_HARDWARE updated.
- Backups: `.local/backups/before-mchose-family-2026-10-02.tgz`,
  `build_supported_gap_layouts.py.before-mchose-family-2026-10-02`.

## Tests

- `mchose_jet75_protocol_test`: 11-model table (exact PIDs, no duplicates,
  key counts, ARM/new-core PIDs absent), Nordic/Mix87 I keys, per-model
  reboot rule with readback: PASS.
- `mchose_jet75_session_test`: previous scenarios + Ace68-II + Jet 75 I with
  the fresh-flash bit set (no reboot path, readback, stream, cleanup): PASS.
- No physical test of any model in this table except Jet 75 II.

## Sheet (synchronized 2026-10-02, after the Google Sheets connector was added)

Live `Main`, MCHOSE block rows 526..542 (both families):

- Status → Implemented; awaiting hardware testing: Ace 60 Pro, Ace 68
  (was No usable analog found), Ace 68 Air, Ace 68 V2.
- Renamed and set Implemented: Ace 68 Turbo → Ace 68 Turbo 8K, Ace 75 → Ace 75 8K.
- Inserted Implemented: Ace 60X, Jet 75 (I revision), Mix 87 (I revision), Zero75X.
- Inserted Not investigated: Ace 68 Turbo 16K, Ace 75 16K (so the renames do not
  drop these products).
- Unchanged: Ace 60 (Research incomplete), Ace 68 GT, GOD 60 (Not investigated),
  Jet 75 (II revision) and Mix 87 (III revision) (Supported, green).

Method: fresh native snapshot (`.local/research/mchose-sheet-20261002/
sheet-before.json`, structure PASS), offline plan with
`plan_keyboard_sheet_batch.py` (`plan.json`), applied as revision-guarded
batches (insert + format/validation copied from an MCHOSE model row + 32 px
heights; one value write for A526:C542; border and base-colour repairs
generated by `check_keyboard_sheet_structure.py --plan`). Fresh final read
(`sheet-after.json`): rows equal the plan, `check_keyboard_sheet_structure.py`
PASS (148 blocks, 0 issues), `support_notice_catalog.py --sheet` PASS (281
yellow, live), `check_supported_layouts.py --sheet` PASS (68).


## Ace68 Air II Supported — 2026-11-16

The owner relays the positive ACE68 Air report (works correctly, testing notice
still visible) and explicitly requests Supported. The message does not supply a
new USB identity; accept the model report and the established reviewed path,
without inventing a log. Air III41E4:2132 was already Supported; promote Air II
41E4:2120 using the existing67+Fn map and built-in MCHOSE Ace68 ANSI preset.
The runtime exception uses protocol29, its exact layout token and2120 PID.
Other RISC-V models keep their notices. Distinct Air2/3837:300A is unchanged;
this screenshot does not mention it. Existing source cutoff/snap and persistent
mode-flag lifecycle are retained. See
[1.6.16 preparation](RELEASE_1.6.16_PREPARATION_2026-11-16.md).
