# Everglide SU75 Pro: experimental support (2026-10-02)

> Revision evidence updated: [2026-11-14 exact firmware review](EVERGLIDE_SU75_REVISIONS_2026-11-14.md).
> V1/XS117 = 1CA6:3002; the seller's pictured V2/SU75 Ultra/XS125 = 1CA6:3007,
> already admitted by the enabled generic SparkLink v2 path. Earlier unknown-PID
> conclusions below are historical; neither model currently has automatic layout selection.

Owner request: analyse the tester log `message (12).txt` (HallJoy 1.6.6.0,
2026-10-02 17:27 UTC) for an Everglide SU75 Pro. Static work only; research
files in `.local/research/everglide-20261002/` (local only).

## Identification

- Log HID candidates: `1CA6:3002` (interfaces 00, 01 with four collections, 02)
  plus unrelated devices (`1915:2346` Nordic receiver, `045E:028E` virtual
  gamepad, `26CE:01A2`, `31B2:1132`). No HallJoy backend saw the keyboard;
  SparkLink reported `not_present`.
- Official Everglide driver: `https://www.xsyd.top/connect` (Everglide product
  pages and keebforce guide). Site name 星闪悦动; API host
  `api.sparklinkplayjoy.com`, i.e. the SparkLink PlayJoy platform.
- Official device catalog `api/v1/getAllDevices` (AES-CBC, key in the public
  page; decrypted copy `catalog.json`, 507 entries): `7334-12290` = `1CA6:3002`
  → usage page `0xFFB0`, usage 1, type `public`, protocol `v2`. Every SparkLink
  PID HallJoy already supports (IROK, CAROTMAS, EWEADN) that is in this catalog
  has the same class (`public`, `v2`, `FFB0`). No `1915:*` entry exists.

## Model identity NOT confirmed (2026-10-02 follow-up)

The owner noticed the tester talks as if he does not own an SU75 Pro yet.
Checked: the log has exactly one SparkLink keyboard, `1CA6:3002`, and it is on
the official SparkLink V2 keyboard list (`getKeyboardListEncrypt`, class
public). But neither official list contains model names: the driver shows the
USB product string (`device.productName`), and HallJoy's support log does not
record product strings. Nothing ties `1CA6:3002` to "SU75 Pro" except the
tester's message. The admission itself is protocol-based and correct for
whatever keyboard reports `1CA6:3002`; only the model NAME (Sheet row, notice
model, README) is unverified until the tester confirms the keyboard and/or the
product name shown for `VID_1CA6&PID_3002` in Device Manager.

## Protocol (v2 app `/v2-app/js/index-D-Vi6Arc.js`)

Command enums resolve to: Device=1/DeviceInfo=2 (`01 02`), LayoutAndKey=3 /
GetKeyLayout=1 (`03 01 layer row`), Performance=4 / AxisData=3 / Route=1
(`04 03 01 row`, `getRoute`, little-endian 16-bit values per row);
`sendReport(0, …)`. These are exactly the commands of HallJoy's SparkLink V2
path (`backend_sparklink.inc`, pinned by `check_eweadn_sparklink_sources.py`).

Why HallJoy ignored the keyboard: SparkLink admission is an exact identity list
(`sparklink::ProbeIdentity`, 2026-09-24 safety change so that `01 02` is never
sent to unknown devices); `1CA6:3002` was not on it.

## Implementation

- `sparklink_model_profiles.h`: `case 0x3002` (Everglide SU75 Pro).
- Notice group `SparkLinkV2`: token `1CA63002`, model Everglide / SU75 Pro.
- `sparklink_model_profiles_test.cpp`: PID added to the identity/notice loop.
- The backend still proves the device-info, layout and travel replies before
  claiming the device (fails closed); key assignments come from the keyboard.
- No built-in layout preset (manual layout selection, as for EWEADN).
- Live Sheet: new brand block "Everglide / SU75 Pro", Implemented; awaiting
  hardware testing. Fresh read: rows equal the plan, structure PASS (149
  blocks), notices PASS (283), supported layouts PASS (68). One write attempt
  returned HTTP 500 and was verified as not applied (same revision) before the
  identical retry. Snapshots `.local/research/everglide-sheet-20261002/`.
- Backup: `.local/backups/before-everglide-2026-10-02.tgz`.
- `build_release.ps1`, `check_support_diagnostics.py`, `run_native_backend_checks.py
  --require-compiler`: PASS. Tester EXE SHA-256
  `064329d3090b49b783a7f4a6b6582d07563622ac7bf329ee754aa988cf4eb4a5`.

## Limits

- No physical test; travel range uses the existing SparkLink V2 normalization.
- SU75 Pro V2 / other Everglide models (SU66 Max, AE64 Pro, AE68 Pro) are not
  added: their PIDs are not established.

## Supported (2026-10-02, owner decision)

Tester log `message (13).txt` (18:17 UTC, build with SU75 Pro admission): SparkLink
`connected`, `1CA6:3002` FFB0:1, 126 mapped keys, 26,217 updates, 0 failures. The
owner accepts the tester's statement that the keyboard is an SU75 Pro (no further
logs requested) and set the rule: when all criteria are met, the model is Supported.

- Built-in layout "Everglide SU75 Pro ANSI" (81 keys): `tools/build_everglide_layouts.py`
  transcribes the official driver key map (keebforce screenshot of the SparkLink
  PlayJoy driver with "Su75PRO" connected; retail top view confirms 81 keys, knob
  right of Up, no right Alt). Images local only:
  `.local/research/everglide-20261002/layout/` (SHA-256 cf04a7fb... / c9fe6f1b...).
  Derived transcription `docs/research/everglide-layout-sources/su75-pro-geometry.json`,
  report `docs/research/everglide-layout-reports/everglide_su75_pro_ansi.json`;
  catalog brand "Everglide" (reviewed-report), pinned in research reference checks.
  Manual selection: the SparkLink path publishes no layout identity.
- `sparklink_model_profiles.h`: 0x3002 moved from `ExperimentalToken` to direct
  admission in `ProbeIdentity` (same precedent as MG75 Max 0x0529); notice token
  `1CA63002` and the Sheet notice model removed. Test asserts admission and no notice.
- `supported_layouts.json`: Everglide / SU75 Pro → "Everglide SU75 Pro ANSI".
- README Supported list, SUPPORTED_HARDWARE row, live Sheet status: see OWNER_CONTEXT.

### Sheet change and readback (2026-10-02)

- Before: native snapshot rev `APfK_pCA1wEE...`. Intended state was checked
  first: `check_supported_layouts.py` PASS (70) and `support_notice_catalog.py
  --sheet` PASS (281).
- Changed: Main!C262 (Everglide / SU75 Pro) and Main!C679 (Royal Kludge /
  RK68 HE) from "Implemented; awaiting hardware testing" to "Supported", with
  B:C base colours set to the Supported green
  (.65882355/.8666667/.70980394, text .09019608/.29803923/.16470589).
- Agent error, corrected: the first batch used a `userEnteredValue` field mask
  over B:C, which blanked B262 and B679. Readback caught it, and the exact
  original names "SU75 Pro" and "RK68 HE" were rewritten immediately.
- Final full re-read compared with the before snapshot: only these four cells
  differ (C value; B/C background and text colour). Properties, merges, row
  metadata and every other cell are identical, and status validation is kept.
  `check_supported_layouts.py --sheet` PASS (70); `support_notice_catalog.py
  --sheet` PASS (281 yellow). Snapshots are in
  `.local/research/supported-eg-rk-sheet-20261002/`.
- `build_release.ps1` PASS. EXE SHA-256
  `85666c247b6bd965e52c1b40db148af8a3a0a91cff97faab50096a596ffde079`; it also
  contains the product-name support-log change.
