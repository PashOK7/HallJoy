# AULA enabled families and yellow support synchronization — 2026-09-22

## Owner contract and delivered result

Yellow means a complete enabled path: detection, independent analog input,
bindings and virtual gamepad. Remaining range/firmware nuances are allowed;
unfinished integration is not. A physical tester is NOT an admission prerequisite.
No new physical-device test is claimed here. Firmware was read/extracted only;
vendor updater executables were not run. No flashing or GitHub publication.

Nine additional wired AULA models are enabled in the normal local candidate:

| Model | Native family / exact admission |
|---|---|
| HERO 68 HE | GEEHY UUID 110000000003 |
| HERO 68 Air | GEEHY UUID 110000000014 |
| HERO 68 MINI | GEEHY UUID 11000000000F |
| WIN 68 HE Ultra | GEEHY UUID 110000000015 |
| HERO 99 HE | GEEHY UUID 110000000012 / 11000000003F |
| WIN 60 HE PRO | RM 1CA2:1902 / board 0A021902; exact product name distinguishes PRO from MAX |
| WIN 68 HE PRO / WIN 68 HE MAX | RM 1CA2:1901 / board 06021901 |
| HERO 68 HE PRO | RM 1CA5:0409 / board 06050409 |

GEEHY admission keeps 372E:103E, FF60:61/report09 and exact UUID negotiation.
The existing HERO84 backend now installs each model's factory and live map,
including modifiers/Fn, publishes its layout token and reads addressed travel
through the existing bounded worker. Observed-range normalization is retained.
All models feed the ordinary configured XUSB builder; no digital depth synthesis.

RM support retains the full capability proof: board identity, precision and
min/max, default matrix, two matching active-map generations, both travel halves,
checksums and response correlation. WIN60 PRO uses existing WIN60 MAX geometry;
the two RM68 board families use the exact 68-key WIN matrix and K68 geometry.
Receivers/Bluetooth are outside this support claim.

## Sources and reproducibility

- Pinned `docs/research/aula-additional-layout-sources/hero-app.js`, official
  https://heb.aulacn.com/app-B6-MiDbc.js,
  SHA256 `16030c913fe6f6a4b0326a346ec9920a6a33ba16fe569bfcb4c5b6e9ce6bbaab`.
  `ARM32_HID_68` has 68 positions; `AULA_2836` has 101 vendor entries, retained
  exactly despite the HERO99 retail name. No duplicate HID or position.
- `python tools/check_aula_family_sources.py` re-extracts and checks all 169
  positions and geometry against compiled headers. Existing HERO84/KP layout
  source tests also pass.
- Existing `docs/research/AULA_HERO84HE_FIRMWARE_2026-08-31.md` records the
  HERO68 V3.23 / HERO99 V1.4 read-only 94/02 current/min/lock-bit semantics.
- Official RM driver snapshot in `docs/research/aula-max-layout-sources/`:
  `index-C7aUVaaC.js` and `index-lKmrC98y.css`, K68 matrix/CSS.
- Download receipts and extracted images: `.local/aula-family-20260922/`.
  Official lookup `https://hubapi.aulacn.com/user/EXE/getFile/<product>` and
  payloads `https://app.aulacn.com/commonAssets/<filename>`.
  Extraction used isolated evbunpack 0.2.6, aplib 0.6 and pefile 2024.8.26;
  upstream https://github.com/mos9527/evbunpack. No updater execution.

| Extracted firmware | SHA256 |
|---|---|
| WIN 60 HE PRO 1.1.6, 20260204a | 687bc7a7f022dca841a5a60f887135895dcf8a76d5b1ce20263e09dfdfa8efba |
| WIN 68 HE PRO 1.1.4, 20260204a | abef665120983904cc585c8382952bc45001ce170bcf4c947e5886a4a438f8a0 |
| WIN 68 HE MAX 1.1.4, 20260204a | 7d88121f0f4ab4f2ac50493b4047a8eaac10b2ddb6f45e2db6e088a8f94b99b8 |
| HERO 68 HE PRO 0.1.5, 20250808a | 7e47eca2bb199588873e42366850a9c9a91ae8636ed4fc00096f73af19e0ce23 |

The actual HERO PRO archive contains 0.1.5; a general updater configuration
mentions absent 0.1.6, which is not used as evidence. WIN68 WIN factory matrix
starts at file offset 0xA20; HERO PRO matching matrix is at 0x20CD8. The MAC map
is different and was not substituted. These new RM images were not emulated.

## Notices and live Sheet reconciliation

`docs/development/keyboard_support_notices.json` is the shared exact-model
yellow catalog; `tools/support_notice_catalog.py` generates runtime matching
by verified protocol/token. Release build checks reject a stale header.
It covers 47 models: ATTACK SHARK 27, AULA GEEHY 6, AULA RM 4, GravaStar 2,
IPI 6, IROK 1, Redragon 1. Ordinary supported variants are explicitly excluded.
Incomplete research notices are neutral gray rather than yellow.

Live Main sheet read before edits; only `userEnteredValue` changed in column C:

- C111,112,113,115,119,130,132,133,134: Research incomplete →
  Implemented; awaiting hardware testing (the nine new models above).
- C262 Aurora 75: implemented/yellow → Supported/green. It shares the existing
  QBZ75-compatible exact UUID/layout route (11000000002C / 110000000013,
  token 0FEFA7117D763FE5). This is an alias/protocol conclusion, not a newly
  performed Aurora physical test.
- C425,427,428,429: NuPhy BH65, Field75 HE V2, Halo65 HE, WH80:
  Known protocol; depth scale unverified → Research incomplete/gray.
  The earlier review found unresolved report fields (vendor depth bytes 6..7
  versus UAP 4..5); this is not just uncertain endpoints. Existing generic
  runtime admission is unchanged, with a gray compatibility advisory.

Readback of ALL A1:C1055 completed. Updated validation lists still accept each
value; no notes/comments were added. Effective colors verified: yellow
1/.9019608/.6392157, green .65882355/.8666667/.70980394, gray
.92156863/.93333334/.9490196. Full readback is retained in
`SUPPORT_SHEET_READBACK_2026-09-22.json`.

`python tools/support_notice_catalog.py --sheet docs/current/SUPPORT_SHEET_READBACK_2026-09-22.json`
PASS, all 47 yellow model/status pairs exactly match runtime catalog.
This snapshot proves this task's readback only; future tasks must read live data.

## Validation and artifact

- Portable support-status test: PASS, all flag combinations and green exclusions.
- RM sanitizer suite: PASS (health control, protocol, oracle, end-to-end,
  aggressive end-to-end, metrics, session policy). Added exact new board admission
  and wrong-board rejection checks; sanitizer include path corrected.
- HERO linked publication test: PASS for every admitted model, complete maps,
  independent travel, release and configured XUSB output.
- Source geometry check and existing two AULA layout tests: PASS.
- Release build and all five linked checks: PASS. Early candidate attempts failed
  or timed out while introducing the self-test command; corrected Win32 argv
  parsing and shellapi include, then final full build/check sequence passed.
  Previous delivered EXE was preserved until final success.
- Delivered `build/bin/Release/x64/HallJoy.exe` SHA256
  `e48db29d077b8de264c025bf40c7599734a1666ddaacf4cf1c79dc5892883718`.
- K4 onboard compile option retained. Camera latency test remains compiled out;
  no forced logging. No agent GUI validation; visual evaluation belongs to owner.
- Pre-change source backup: `.local/backups/aula-yellow-1790054716.zip`.

## Remaining concrete boundaries

No model is held back merely because a tester is absent. MINI68 base/PRO/MAX
firmware shares a command family but observed streaming chooses a selected/deepest
key; independent simultaneous travel delivery is not established. Do not enable
calibration command 64 as a workaround: it changes device behavior.

HERO68 HE Ultra (RM 1CA5:0407) lacks exact board proof; F75 HE HFD lacks a qualified
payload. HERO68 MINI Air/XS and HERO75 HE need exact newer-HUB UUID/layout evidence.
AG models and BOX63 are not established compatible routes. These remain gray;
the nine additions do not imply support for every AULA model.
