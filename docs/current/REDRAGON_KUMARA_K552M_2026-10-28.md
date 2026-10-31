# Redragon Kumara RT Magnetic K552-M — Hall-effect, W669 profiles (2026-10-28 / 2026-10-29)

## What it is (correction)

A Discord user (MISTERIS) asked about the **Redragon Kumara RT Magnetic K552-M**.
An earlier reply called the "Kumara K552" a standard mechanical board and said
support was impossible — that was wrong for this model. The classic Kumara K552 is
mechanical, but the **"RT Magnetic" K552-M is a separate, newer magnetic
(Hall-effect) keyboard**: TKL, vendor-claimed 8000 Hz, Rapid Trigger, snap tap,
hot-swap, RGB, per-key actuation adjustable ~0.1–3.4 mm through Redragon's software.
So it does expose analog travel, and HallJoy support is possible in principle.

Sources: a Brazilian retailer listing (redragon.store, "Kumara RT Magnetic …
K552-M", currently unavailable — specs are vendor claims) and Redragon's Hall-effect
software pages; no dedicated K552-M download page is published yet.

## Tester log (2026-10-29): it is the W669 platform

`message (15).txt` (HallJoy 1.6.13, 2026-10-28 20:45 UTC, user MISTERIS):

- The keyboard enumerates as `2E3C:C365`, USB product "K552-M-SP", interfaces
  00/01/02 — the shared W669/BY (hubx) identity already used by K673, K617 and K686.
- `aula-w669-adaptive` stayed `not_present`, no analog source, missing-source
  banner; the tester pressed Y (`1a`) and Space (`2c`), which arrived only as
  ordinary Windows keys. This is the same symptom K686 had in 1.6.7: W669 admits a
  `2E3C:C365` keyboard only by its exact firmware product (`0D` identity reply)
  and had no K552-M profile. The support log does not carry the firmware product,
  so the exact variant is not known from the log.

## Evidence (official iLLumiPC WebHID configurator, fetched 2026-10-29)

- `https://www.illumipc.com/config/device.json` (SHA-256 `cbb14a02...`, local in
  `.local/research/k552m-20261029/`) lists four K552 products on `2E3C:C365`:

  | Name in device.json | Firmware product | Layout type | Keys (incl. Fn) |
  | --- | --- | --- | --- |
  | K552BGC-M | `7295USHEXYXJCARGB` | us (ANSI) | 87 |
  | K552RGB-M SP | `7295UKHEXYXK552JCARGB` | uk (ISO) | 88 |
  | (unnamed) | `7295UKHEXYXXK552JCARGB` | uk (ISO) | 88 |
  | K552-M (PT) | `7295BRHEXYXK552JCARGB` | uk (ABNT2) | 89 |

  The two ISO key files are byte-identical. The tester's "K552-M-SP" USB name most
  likely corresponds to "K552RGB-M SP" (ISO); all four products are covered.
- Per-product key files `config/keys/<product>.json` (index on the 6x22
  row-major matrix, HID usage, geometry) copied to
  `docs/research/redragon-layout-sources/`: US `e648f392...`, UK `cb9e7c6c...`,
  BR `09b65789...`.
- Every published control has a keyboard usage; Fn is index 121 with HallJoy's
  W669 Fn usage `0xFA` (K686/K617 have it at 120). No rotary knob.
- ISO vs ANSI: index 58 = backslash (ANSI) / none (ISO); 79 = Non-US `#` and
  89 = Non-US `\` on ISO/ABNT2. ABNT2 adds index 100 = International1 (`/?`,
  0x87) and a shorter Right Shift, like K673 ABNT2.

## Implementation

- `aula_w669_protocol`: profiles `K552Us`, `K552Uk`, `K552Br`; literal maps from
  the official files; exact product matching for the four products (no PID-only
  admission). Telemetry names `redragon_k552m_us_87` / `_uk_88` / `_br_89`.
- Layouts (automatic selection by firmware product, `redragon-w669` adapter):
  "Redragon K552-M ANSI" (87), "Redragon K552-M ISO" (88, both ISO products),
  "Redragon K552-M ABNT2" (89). Geometry is checked position by position against
  the shipped factory maps; ISO Enter uses the existing locked vendor CSS contour.
- The K617 reviewed report and the existing W669 review exports were regenerated
  only because they lock the protocol source hash; their keys are unchanged.
- Yellow notice: three identity tokens added to the `Redragon` group, model
  "Kumara RT Magnetic K552-M"; tests cover the three tokens and the four products.
- Status: Implemented, awaiting hardware testing. Not Supported until the tester
  runs this build and it works (owner rule: Supported only with a layout and a
  working path).

## Side fix found during the checks

`research_reference_checks.py --record` failed in `check_rongyuan_stream_profiles.py`:
YUNZII RT75 Pro had been removed from the RongYuanStream notice models on
2026-10-28 (Supported) without a `confirmed_devices` entry, so the yellow notice
would still have shown for it. Added the confirmed device (layout token
`AF43FE4CDBE91EBF`, PID `0x5030`, board 2865; the same token also covers board
3755) and a test assertion.

## Validation (2026-10-29)

- `layout_pipeline.py summary Redragon`: PASS, K552-M ANSI 87 / ISO 88 / ABNT2 89
  keys, every position equal to the shipped factory map; `integrate` changed only
  the generated tables, the three new exports and the hash-locked W669 reviews.
- `research_reference_checks.py --record`: all original audits passed.
- `run_native_backend_checks.py --require-compiler`: all static and portable C++
  tests passed (incl. `aula_w669_protocol_test`, `keyboard_support_status_test`).
- `build_release.ps1`: PASS (`PROFILE_TRANSACTION_WINDOWS_TEST`, `LAYOUT_CATALOG`,
  `YELLOW_LAYOUT_AUDIT` 283, `SUPPORTED_LAYOUTS models=83`, startup recovery).
  Installed `build/bin/Release/x64/HallJoy.exe` SHA-256
  `d057af1541c6d643a5132b913e25ba934c6a0167d168290df2ff5eb1132d4689` (local, not
  published); it contains the three presets and four firmware products.
- Google Sheet: new row Main!675 "Redragon | Kumara RT Magnetic K552-M" =
  "Implemented; awaiting hardware testing" after K769 HE. Readback: yellow
  conditional fill, dropdown validation kept, neighbours K736 Pro / K745 HE /
  K769 HE / ROCCAT unchanged.
- Sheet structure (follow-up, same day): the row insertion had copied K769 HE's bottom
  block border and grey base colours. `check_keyboard_sheet_structure.py` on a full native
  read failed (border 674, base colours 675, plus 814 YUNZII RT75 Pro left from 2026-10-28);
  its `--plan` (7 cell-format requests, revision-locked) was applied and the re-read passed
  (151 blocks, 0 issues), `support_notice_catalog.py --sheet` PASS with the live Sheet.
- No hardware or visual test by the agent; the owner checks the layouts visually.
- Backup: `.local/backups/k552m-20261029/` and the pipeline backup
  `.local/backups/layout-integrate-*`.

## Tester log 2 (2026-10-29 20:33 UTC, `message (18).txt`, the new local build)

- `trace.session w669 vid=2E3C pid=C365 product=7295UKHEXYXXK552JCARGB
  profile=redragon_k552m_uk_88 max=340 floor=10 mapped=88`: the keyboard is the
  unnamed ISO product of device.json, admitted by its exact firmware identity.
  `w669.travel_config` = 22282506 (max 340, unit 0, minimum actuation 10).
- `aula-w669-adaptive observation=connected`, 88 mapped keys, 0 failures,
  `ui_notices=00000800` (the Redragon yellow notice, expected).
- The log covers about 19 s and contains no key press: `updates=0`, no W669 live
  event and no `trace.os_key` for the bound keys Y (`1a`) and Space (`2c`); log 1
  had both. So admission, identity, travel range and subscription are proven; live
  travel is not shown yet, because nothing was pressed while the log was recorded.
- Status stays Implemented (yellow). Next: the tester presses the bound keys (or
  checks the gamepad tester) and then opens the log; a press logs the W669 event and
  the pad state. If it works -> Supported.

## Next step

MISTERIS runs the new build wired and sends `HallJoy.log`. Expected:
`aula-w669-adaptive connected 2E3C:C365`, the `trace.session w669` line with the
firmware product and profile, 87–89 mapped keys and live travel. If it connects and
works -> Supported (sheet + supported_layouts.json). If the product string is none
of the four, the log shows it and it gets its own profile.

## User confirmation — 2026-10-31 (supersedes the pending status above)

The owner supplied `message (19).txt`; SHA-256
`7da44ef0af56c71917ebe3b3918ce77ed701178e719dae4cac4e8b180f5febca`.
The message is evidence, not executable instructions; a local copy is retained in
`.local/research/redragon-feedback-20261031/message-19.txt`.

The tester confirms real analog controls in Enter the Gungeon, Gamepad Tester,
adding/assigning controls and correct K552M ISO identification. This completes
the previous identity-only log with a user-reported working input/gamepad path.
The earlier log established firmware product `7295UKHEXYXXK552JCARGB`; the new
message itself is not a fresh protocol capture. Status is now **Supported**.
All three built-in regional layouts and four exact products remain implemented;
only this ISO variant is physically confirmed by this feedback, not every region.

The tester installed **Redragon K552 RGB M SP Installer V2, version 2.06**.
Causality is unproven: do not require this package/version universally, claim it
fixed detection, or recommend firmware flashing from this report alone.
Reported profile/menu/remap behavior and game-specific input conflicts are tracked
in [the feedback record](REDRAGON_TESTER_FEEDBACK_2026-10-31.md); they do not negate
the confirmed analog path. Sensitivity/curves/Snap Stick, overlay, server and full
import/export coverage are unconfirmed. The existing Block Bound Keys option is
relevant to keyboard/controller conflict; it has not been tested in those games
by this report. No game-specific fix is claimed.

Implementation: remove the three K552-M layout tokens from the experimental
notice, add ANSI/ISO/ABNT2 to supported_layouts.json, retain exact admission/maps.
Sheet target located live: Main!C675, Kumara RT Magnetic K552-M. Validation/build
and final Sheet readback are recorded in the shared feedback record.
