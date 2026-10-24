# RongYuan stream: FUN60 Ultra TMR release sticking, IROK Mars68 Pro, Womier SK75 TMR (2026-10-21)

Owner rules for this work: never derive analog values from digital key events (diagnostic
reference only); no ad-hoc timeouts that fit one case and add artifacts elsewhere; find the
correct mechanism.

## 1. MonsGeek FUN60 Ultra TMR (board 2307): keys stay slightly pressed after a multi-key release

Tester log `HallJoy (11).log` (tester EXE, version string 1.6.10, board 2307 on 3151:5030,
`evidence.rongyuan` press records, Block Bound Keys later on, so many records have `os=0`).

- Press records with `max_gap_ms` of 2..14 s inside one press: the key kept a non-zero value
  and the stream sent nothing for it until it was touched again.
- `trace.pad` holds of >= 1 s at small values: F 78 (2.8 s), E 75 (9.2 s), `;` 78 (4.2 s),
  A 75/58, W 75 (6.3 s), each time while one or two other keys were changing (for example
  E 443 -> 175 -> 75 while A and S moved). Reader backlog <= 2: no loss inside HallJoy.

Stock firmware 2307 v306 (local `.local/research/monsgeek-firmware-20261010/extract/2307_v306/`):
the stream producer (around 0xE140) keeps per-key travel in a RAM table
(base 0x20009050, field +0x10BC). Travel >= 20 units: write the value to the table and send one
0x1B report. Below 20 units: write 0 and send a single zero report. A report that is not
delivered (several keys changing at once) is never repeated, but the table keeps the value the
firmware meant to send. Command E5, sub FE (dispatcher 0x124CC -> handler 0x6568, sub byte at
request+3, page at +5) copies 64 bytes = 32 travel values of that page from the same table
(handler 0x68A8: base + 0x10BC + page * 64). Our existing request `Request(0xE5, 0xFE, 1, page)`
matches this layout and `ParseTravel` reads the reply from byte 1.

Fix (`rongyuan_stream_reconcile.h`, `rongyuan_stream_backend.cpp`):
- The reader records every published value per slot with a sample counter (`Stream`).
- A reconciler thread sleeps while every key is zero or confirmed. When a key has a non-zero
  value that the table has not confirmed, it reads only that key's page with E5 FE (vendor
  exchange timing, 10 ms before write and read; no other timer).
- A differing table value goes back to the reader as a correction and is applied only if that
  key got no newer stream sample since the read started (`Accept`), so the stream always wins.
  Equal values confirm the key; nothing is read again until its next stream sample.
- Corrections are logged: `evidence.rongyuan reconcile slot= hid= stream= table=` (first 64 kept).
- A board that does not answer E5 FE logs `reconcile_unavailable` once and keeps the plain stream.
- Unit test (`rongyuan_stream_protocol_test.cpp`, "reconcile"): lost zero, held key confirmed,
  newer stream sample wins, mid-release convergence, missed press.

## 2. IROK Mars68 Pro (board 2438): "keyboard unverified"

Tester log `HallJoy (10).log` (1.6.11, USB product "CAROTMAS Mars68 PRO", 3151:5029):
`rongyuan.identity_pair_rejected value=2438 detail=0x31515029`, FamilyCandidate banner.

Board 2438 = `ry5088_gk8157gpro_8k_dm_spaceled` ("GK8157G PRO") in the RongYuan CN and IOT
client registries, `magnetism:!0`. CN client class `ZEa` is a data-only Babel class
(constructor assigns only defaultMatrix/defaultFnMatrix, no method tables) on parent `Epa`;
the tester-confirmed FUN60 Ultra TMR 2352 class is `AYa -> zda -> Epa`. The matrix differs from
2352 in 12 slots: own profile, not an alias. 68 factory keys including Fn.

Admitted (yellow, protocol 22): `tools/extract_rongyuan_irok_mars68pro_2438.py` (pinned inputs,
`--check`), evidence `docs/research/rongyuan-stream/rongyuan-client-models.json`, profile row
(255 revisions), header row, checker support for the client evidence kind, notice catalog
model, automatic layout `ry_layout_2438` "IROK Mars68 Pro ANSI" (68 keys, CN client geometry,
scaled from a 53.4 px vendor pitch). profiles.json hash re-pinned in 39 RongYuan/EPOMAKER layout
reports, catalog hashes and generated headers updated (`layout_pipeline.py integrate`).
No stock firmware for 2438 was checked. Pending: tester log with `rongyuan.connected value=2438`.

## 3. Womier SK75 TMR: Supported

Owner 2026-10-21: a user reports SK75 TMR works perfectly. No log; board not stated.
Supported needs a built-in layout first: `tools/extract_womier_sk75tmr_2518_layout.py` builds
`ry_layout_2518` "Womier SK75 TMR ANSI" (81 keys) from the layout object for
`ry5088_womiersk75he_8k_8k`, identical in the CN and IOT clients, key set equal to the reviewed
factory matrix. Board 3804 (Europe, keyLayout Common81_SK75TMR, RAlt where 2518 has RCtrl) has
no layout object in either client and is not given a guessed layout; it keeps the testing
notice and manual layout selection. supported_layouts.json entry added; notice catalog moves
SK75 TMR to confirmed_devices {token 0xfa1cc577fe4d36fe = ry_layout_2518, pid 0x5030, board 2518}.
README (Supported list) and SUPPORTED_HARDWARE updated.

Backups: `.local/backups/mars68-fun60-reconcile-20261021/`, layout-integrate backups under `.local/backups/`.

## Checks, build and Sheet (2026-10-21)

- Extraction --check (2438, 2518, 2352), KEYBOARD_IDENTITY_AUDIT PASS, RONGYUAN_STREAM_PROFILES PASS (255
  revisions, 168 models), SUPPORT_NOTICE_CATALOG PASS, layout pipeline IROK/MonsGeek/EPOMAKER/Womier PASS,
  research reference record PASS, native suite PASS (RONGYUAN_STREAM ... reconcile), build_release.ps1 PASS
  (SUPPORTED_LAYOUTS=PASS models=74 catalog=True). Tester EXE (local, not released) SHA-256 44aa81d942515e0077fb1402e8ca30e7342829ad77ff67a9d12bec859f25b8be.
- check_supported_layouts.py on the intended Sheet state (73 live Supported rows + Womier SK75 TMR): PASS sheet=True.
- Sheet (live, values and formats written separately): Main!C393 IROK / Mars68 Pro "Not investigated" ->
  "Implemented; awaiting hardware testing", B393:C393 yellow pair (as Mars75); Main!C785 Womier / SK75 TMR
  "Implemented; awaiting hardware testing" -> "Supported", B785:C785 green pair (as WLMOUSE Ying75). Readback of
  rows 392..394 and 784..786: values, ONE_OF_LIST validation and base colors as intended; neighbors unchanged.

## Tester log after the first reconcile build (`HallJoy (12).log`, 07:43 UTC)

Tester: "nothing changed". Board 2307 connected (units 100). Reconcile worked for 29 s: 194 corrections,
139 of them stuck keys returned to 0 (last stream value 20..40 units in most cases, up to 262), 54 small
refinements (table vs stream +-1..10), 1 missed press. Then one E5 FE reply was not ready within the
100 ms exchange window (`reconcile_unavailable page=0 error=13`) and the first build stopped reconciling
for the rest of the session, so the sticking came back. Wrong design decision (a busy firmware treated as a
missing command). Fix: give up only when a board never answered (three failures before any good read);
after good reads a failure leaves the keys unconfirmed and the next pass reads again; failures and
resumption are logged (`reconcile_failed`, `reconcile_resumed`, first 16 lines). build_release.ps1 PASS,
EXE SHA-256 e75c7c0812e0ec655b00621a6c48ba40daa167f0f144a7319f028246ffa9256c (tester build, not released).
