# USB identity cross-catalog audit — 2026-09-24

Owner requested every feasible prevention/fix for R68-style identity mismatches,
without requiring physical keyboards. No publication requested.

## Evidence and disposition

Compared290 implemented RY board identities (37 ATTACK SHARK plus253 stream
profiles) against11 distinct saved JS catalogs containing explicit id/vid/pid
records. Ten additional tuple candidates; ledger and original record extracts
in ../research/usb-identity-audit/catalog-comparison.json. Duplicated source
hashes were deduplicated. Scope is saved catalogs, not a claim to enumerate all
shipping revisions. The parser intentionally does not treat negative boot IDs
as runtime keyboard identities.

Approved three additional exact tuples across two model names:
- Akko ANSI board2683:3151:5030, retaining3151:5029.
- Akko ISO/UK board2704:3151:5030, retaining3151:5029.
- Valkyrie VK Mag75 Max board2398:374A:A222, retaining3151:5030.

Independent official source: https://app.monsgeek.com/js/index.13c08899.js,
a0c3a7d8.js classes f3/x3, common class k ->26f7a06d.js. Both complete512-byte
factory maps match current profiles; leaf classes override matrices only;
parent k supplies data/lighting constants and the shared parent exposes the
same1B enable and8F identification commands. Original source, parent/leaf
extracts, hashes and canonical tuples pinned in approved-aliases.json.
These are USB aliases of existing profiles, not new retail models or hardware tests.
Original normalization, maps, model names and layout tokens retained.

Seven candidates intentionally NOT admitted as identity-only fixes:
- AJAZZ AK680MC board2609: historical row uses YC3123, different controller.
- MAMBASNAKE X60 HE board2368: old factory map differs.
- Fury Kanabo K6 board2763: old record calls it generic OEM MK129; retail binding unresolved.
- FL ESPORTS GP75 HE2669, FREEWOLF F682634, MechLands M752496,
  AJAZZ ALUX68 PRO2592: older Babel client matrices
  match, but older protocol/stream capability remains unestablished; three
  also change internal class names. Do not use matching geometry as proof of
  compatible stream firmware. Older classes reviewed through updater/lighting
  parents; no independent1B enable evidence established in that client.

The legacy catalog also contains actual reused board numbers. Therefore no
universal accept-known-board-on-any-USB fallback was added.

## Other families and limitations

Existing native suite checks JingTai exact identities and source maps, SparkLink
wire/layout sources, IPI firmware catalog, addressed backends and other native
protocol guards. JingTai's saved firmware evidence already records legacy/shared
USB IDs with required product strings. SparkLink already uses semantic live
capability proof and live maps. UAP/SDK detection is delegated and must not be
replaced by a global HallJoy VID/PID guess. Catalog and layout checks do not
constitute new firmware or physical validation for each family.

A literal USB-descriptor search of saved RY firmware images did not establish
new runtime IDs: newer images construct/encode descriptors, and boot/composite
candidates cannot be blindly copied. No identities were admitted solely from a byte-pattern search.
Unpublished/unavailable firmware variants still require later evidence.

## Valkyrie firmware follow-up

Read-only manufacturer API get_fw_version (dev_id2398) returned v306;
https://api2.rongyuan.tech:3816/download/fw_upgrade_file/2398_v306 was downloaded
and raw-deflate decoded. Local archive: .local/research/identity-fw-1790269550.
Decoded SHA256: 1a054de84141c26975219431cff4d5360a3f342106547f794a56372bf027e576.
Other queried candidates2669/2634/2496/2368/2592/2763 returned Record not found;
2683 also returned Record not found. This does not establish absent support.

Official legacy catalog identifies board2398/374A:A222 as MAG75Max/VKMS;
Babel class uGa factory matrix qCa matches the full current profile. Firmware
contains that tuple, command1B enable at0x08012a30, sensor producer gated by
RAM flag0x975, and packet writer0x08011e54 emitting05/1B/depth-low/depth-high/slot.
Pinned excerpts, offsets and hashes are in approved-aliases.json.

The actual packet-writer machine code was executed in Unicorn with synthetic
RAM for five slot/depth cases, including zero and multibyte depth. Payload and
queue bit0x10 passed. Reproduce with tools/review_valkyrie_identity_firmware.py
(requires Unicorn and local firmware). This is a component test, not full MCU,
USB, timing, sensor calibration or physical keyboard validation.

## Prevention and diagnostics

Added tools/audit_keyboard_identities.py:
- default checks exact compiled aliases against pinned independent records,
  full factory maps, parent/stream evidence and source hashes when originals exist;
- --scan-local NEW_JSON rescans saved large JS catalogs, emits conflicts/leads
  without changing support. It requires a new output path.
- ordinary tools/build_release.ps1 runs the evidence check before compilation.

Both RY routes now log rejected board/USB pairs explicitly instead of only
saying identity rejected. Stream logging is deduplicated per board/session.
Both telemetry paths report actual USB identity, including aliases, rather than
the canonical catalog PID. No forced logging, global probing, flash/calibration
commands or relaxed unknown-device admission.

## Status reconciliation

README already lists Akko MOD007S V3 HE and Valkyrie VK Mag75 Max experimentally; no user-facing USB IDs
added. Runtime yellow notice unchanged. Detailed hardware docs and next notes
updated. Live Sheet metadata confirmed Main/id0/1275rows; Main!A36:C40 readback
locates exact Akko MOD007S V3 HE row39. Status remains Implemented; awaiting
hardware testing; strict dropdown and base/effective yellow B:C
RGB1/.9019608/.6392157 verified. Valkyrie Main!A727:C727 was likewise read back:
same experimental status, strict dropdown and matching base/effective yellow. No cell changes or count/promotion changes.

## Completed validation

- Full static and portable native checks passed across the initial and resumed
  runs. Two stale expectations were corrected: the already-added communication
  warning predicate and R68's already-approved alternate identity. Logs:
  .local/identity-full-checks-1790269314.log and
  .local/identity-resumed-checks-1790269517.log (remaining suite exit0).
- Final targeted RongYuan test passed with all three aliases, wrong-identity
  rejection,253 original profiles and4898 captured frames.
- Valkyrie packet machine-code replay: five cases PASS.
- Final ordinary Release build, identity audit (290/11/10/3), notice catalog,
  source encoding audit and all six executable gates PASS. Diagnostic build
  switches disabled. Existing compiler formatting warning and missing third-party
  ViGEm PDB warning are nonfatal; no physical testing claimed.
- Installed executable: build/bin/Release/x64/HallJoy.exe.
  SHA256: 65efa2d73fe5ca4935bacea67b27e5f18a28e410f789768076a6181ef68d320e.
- README model rows and live Sheet statuses remain unchanged; detailed hardware
  notes and next patch notes include both models. No GitHub publication.

Unknown shipping firmware variants cannot be inferred from an absent catalog.
Seven unresolved candidates remain explicit research holds rather than guessed
compatibility. The build gate validates pinned evidence, not future vendor changes.
