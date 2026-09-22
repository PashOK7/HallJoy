# AULA known-protocol review and Camera tester disable — 2026-09-22

Owner scope: disable Camera latency test completely while retaining source; admit additional AULA keyboards only where existing protocol compatibility is justified. No new protocol implementation, physical test, firmware flashing or GitHub publication in this task.

## Delivered changes

- Camera button and its command handler compile out under HALLJOY_CAMERA_LATENCY_TEST_ENABLED=0. The entire implementation, globals, rendering and polling worker compile out; only no-op Open/Stop stubs remain. Normal Gamepad Tester remains available. Source retained. Delivered EXE contains no Camera latency test label.
- Added wired MINI 60 HE, VID 0C45/PID 8032, own model/layout token and ANSI preset. Shared MINI60 stream, factory physical map and normalization; exact descriptor/reply identity matching. Manufacturer 0166; device-info product bytes come from MCU register, as on MAX; PRO restriction 110C retained. Receivers and alternative PRO PID 80B2 remain excluded.
- Existing linked MINI60 check now exercises base/PRO/MAX publication, routing to configured XUSB, layout token, remaps, Fn, expiration and worker containment. These are software checks, not keyboard hardware tests.
- README and SUPPORTED_HARDWARE synchronized. MINI60 MAX prior support retained.

## Source and firmware evidence

Pinned archived official AULA HUB bundles: .local/aula-mini60-index-BZJzwCAk.js, HFD-D07mGRx8.js, hfd-sdk.es-CGV2WPaF.js, version-BsdmHspf.js. Provenance: Aether-HE commit 2179fd768fcdbb86f59c4c240b4d4cfcbc18936f docs/context/issue-6-mini60-pro-raw/driver_src. HUB currently returns 403; archived bundle is not presented as a fresh registry download.

Fresh official firmware lookup: https://hubapi.aulacn.com/user/EXE/getFile/<encoded product name>; file downloads https://app.aulacn.com/commonAssets/<encoded fileName>. API responses, installers and extracted resources retained in .local/aula-family-20260922; manifest.json records hashes. Vendor EXEs were never executed.

Fresh RM6x21 manifest: https://magnet.aulastar.com/update.json, saved magnet-update.json. Driver source docs/research/aula-max-layout-sources/index-C7aUVaaC.js distinguishes board/layout identities, not just USB IDs.

MINI60 base V1.18 firmware resource SHA256 15546f4523fe1d470310513486838f91f9309e3475e4a34518e48f5bf1a7a78a. Actual Thumb dispatcher at E148 accepts 66/67 and changes simulation state only; calibration remains off. Actual reporting tail 2DBE..2DF2 calls USB send 10EC0 with 64-byte 55FB records, independent key indices and LE16 high/low/ADC/travel/stroke. Synthetic W/A depths 170/93/1 at stroke34 decoded correctly; fully released sample emits no packet, handled by existing 50ms expiry. Ordinary key-processing continuation 2E42..2EA0 is gated by calibration flag, not simulation flag. Device-info E2B2 loads MCU register 400180 and overwrites product bytes. Physical 61-key map is pinned HFD layout o/master map y, shared with existing MINI60 implementation.

Reproducible isolated emulator: .local/aula-mini60-base-research-20260922.py (derived from MAX script); PASS. Saved result mini60-base-emulation.json. Synthetic ADC/calibration, USB submission stub, no full scanner/peripheral/timing claim.

## Remaining model decisions

| Models | Existing-family evidence | Why not added as Supported |
|---|---|---|
| MINI 68 HE / PRO / MAX | Official V1.21/V1.81/V1.73 extracted; HFD 55FB builder and 66/67 simulation route | Reporting code differs from MINI60: selected-key comparison and a buffered pending slot. Independent simultaneous-key delivery/drain has not been established; cannot treat identical packet fields as stream compatibility. Own 68-key map and identities also require integration. |
| WIN 60 HE PRO; WIN 68 HE PRO / MAX | SparkPlayJoy/RM6x21 official registry and fresh manifest; WIN60 PID1902, WIN68 PID1901, VID1CA2 | Exact board identity/layout and per-image depth mapping not yet qualified. Fresh V1.1.6/V1.1.4 installers are Enigma Virtual Box containers; firmware not extracted from those wrappers. USB identity alone does not justify widening admission. |
| HERO 68 HE PRO / Ultra | SparkPlayJoy/RM6x21 route, VID1CA5/PID0409 or0407 | PRO V0.1.5 downloaded but Enigma packed; Ultra exact-name API lookup returned null. Board/layout/depth qualification remains. No claim firmware is globally unavailable. |
| HERO 68 HE, HERO 68 MINI, HERO 68 Air, HERO 68 MINI Air, HERO 68 XS, HERO 75 HE, HERO 99 HE, WIN 68 HE Ultra | GEEHY HERO route, VID372E/PID103E FF60:61 | Existing native backend admits HERO84 UUID only. Own UUID/layout and depth semantics required. Prior HERO68/HERO99 audit already found differing raw-depth paths; no blind UUID widening. |
| F75 HE | HFD registry wired38A6:2729 | Exact firmware lookup under F75HE returned null; no proof that its reporting/scale matches MINI60. |
| AG60/63/75 and registry PRO/MAX variants | Separate AG driver route, FFB0:1 | Different family, outside existing-protocol admission scope; no reverse engineering undertaken. |
| BOX63 | Catalogued magnetic model | Exact driver identity/protocol not established. |
| WIN60 HE, WIN68 HE, WIN60 HE MAX, KP-TE153, MINI60 PRO/MAX, HERO84 | Existing implementation | Existing support conclusions retained; this task does not imply new hardware testing. |

MINI68 base selected-key address 20000085, simulation20000485; reporting region5116..51EE. MAX corresponding selected-key20000095, simulation200004B5, region5BB0..5C76. This is an unresolved transport behavior, not a proof the models are impossible to support.

AG registry also contains distinct PRO/MAX variants and AG63 PRO aliases; full marketing-name deduplication/catalog expansion for those variants remains separate from this supported-protocol review. No blanket claim that every newly seen SKU has been fully reverse engineered.

## Sheet synchronization

Canonical Sheet 1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c, Main, sheetId0. Read live metadata and exact cells before changes. MINI60 base C122: Not investigated -> Supported; readback green RGB .65882355/.8666667/.70980394. Reviewed previously Not investigated AULA rows changed to Research incomplete (18 rows), retaining gray and strict existing dropdown. This indicates remaining technical uncertainty, not lack of a mandatory physical tester.

Added missing MINI68 base/MAX/PRO catalog rows125..127 with Research incomplete, copied existing formatting/validation and preserved neighboring models. Final AULA rows106..134, blank135, CHERRY136. Readback A106:C136 confirmed values, validation membership and effective colors; no notes/comments. Existing support statuses preserved. No GUI visual run per owner instruction.

## Build and delivery

Backup before camera edits: .local/backups/aula-expansion-camera-20260922-075753.zip; base runtime files also backed up under .local/backups/aula-base-*.zip.

Built with HallJoyKeychronOnboardExperimental=true via tools/build_release.ps1. Encoding audit and all four linked gates PASS (shark, MINI60, NA87, embedded ViGEm installer). Build retained K4 onboard and no forced diagnostic logging. Third-party ViGEm PDB warning only.

Delivered build/bin/Release/x64/HallJoy.exe SHA256 30be336b7e1e4559d8ea6c017836091275060d216e24b626082bdd7429e2bc8f. No firmware or user calibration changed. No GitHub publication.
