# RongYuan fourth large support batch — 2026-09-24

## Result

14 models / 24 revisions / 11 brands added locally. The experimental yellow
path is enabled for wired USB detection, independent key analog, bindings and
virtual gamepad output. Manual layout selection is available. No physical-device
tests, firmware emulation, flashing or GitHub publication occurred. K4 onboard,
ordinary logging policy and disabled camera latency UI remain unchanged.

## Exact profiles

| Brand | Model | Board | VID:PID | Source | Parent | Normalization, um |
|---|---|---|---|---|---|---|
| ANTGAMER | AGK75 PRO | 2651 | 3151:502F | 4a27ee63.js | 7a5b12c9.js | 4000 |
| ANTGAMER | AGK75 PRO | 2425 | 3151:502F | a1939cea.js | 7a5b12c9.js | 4000 |
| ANTGAMER | AGK75 PRO | 2328 | 3151:5030 | 6da02bdb.js | 7a5b12c9.js | 4000 |
| ANTGAMER | AGK75 PRO | 2629 | 3151:502F | 1a1d5093.js | 7a5b12c9.js | 4000 |
| ANTGAMER | AGK75 U2 | 2281 | 3151:502F | d154716d.js | 7a5b12c9.js | 4000 |
| ANTGAMER | AGK75 U2 | 3064 | 3151:502F | bcf93e9b.js | 7a5b12c9.js | 4000 |
| ANTGAMER | AGK75 U2 | 2642 | 3151:5030 | 5cf6faaa.js | 60ee4367.js | 4000 |
| ANTGAMER | AGK75 U2 | 3005 | 3151:5030 | eca864a5.js | 7a5b12c9.js | 4000 |
| ANTGAMER | AGK75 U2 | 3512 | 3151:5030 | 760a509b.js | 7a5b12c9.js | 4000 |
| ANTGAMER | AGK87 | 2114 | 3151:5029 | 8b6002e6.js | 7a5b12c9.js | 3400 |
| ANTGAMER | AGK87 | 2330 | 3151:5029 | 26838fb8.js | 7a5b12c9.js | 3400 |
| Veekos | Shine60 HE | 2836 | 3151:5029 | 64998e4f.js | 3dd2d1f8.js | 4000 |
| Veekos | Shine60 HE | 2832 | 3151:5030 | 2dd617f5.js | 3dd2d1f8.js | 4000 |
| EDRA | EK368RT | 2835 | 3151:5030 | 7f6686db.js | 7a5b12c9.js | 4000 |
| IDEEZ | SWIFT X85 | 2756 | 3151:502F | f05df4e0.js | 7a5b12c9.js | 4000 |
| IDEEZ | SWIFT X85 | 3481 | 3151:502F | f05df4e0.js | 7a5b12c9.js | 4000 |
| G TUNE | GMK82 | 2912 | 3151:5029 | 622281b0.js | 7a5b12c9.js | 3300 |
| MICROPACK | K-68M | 2546 | 3151:502D | 353b2d65.js | 7a5b12c9.js | 4000 |
| ROYALAXE | X68 | 2485 | 3151:502D | de10742f.js | 7a5b12c9.js | 4000 |
| SARU | KX69HE | 3166 | 3151:5029 | 4b737911.js | 7a5b12c9.js | 3300 |
| SARU | KX78HE | 3243 | 3151:5030 | 6b973192.js | 7a5b12c9.js | 3400 |
| URX | Core68 HE | 3513 | 3151:5029 | 72384e0e.js | 7a5b12c9.js | 3400 |
| LinkerFoo | LF67R1 | 3068 | 3151:5029 | a98e2158.js | 7a5b12c9.js | 4000 |
| Titan Nation | TITAN68HE | 2116 | 3151:5029 | f90deee3.js | 7a5b12c9.js | 3400 |

## Admission and limits

Pinned Womier 3.2.15 OEM records provide exact magnetic-board USB identity,
class loader and factory matrix. 23 additional source files and their hashes
are retained under docs/research/rongyuan-stream; profiles and admission records
are audited against compiled runtime data. All 512 factory-matrix bytes are
checked. Source audit now covers 110 RongYuan models / 170 revisions.

All admitted revisions use the existing reviewed report 5 / command 1B path.
Veekos inherits the already reviewed 3dd2d1f8.js parent also used by Akko/MonsGeek;
ANTGAMER board2642 uses precision-enum parent60ee4367.js. Others use7a5b12c9.js.
The auditor now accepts an explicit parent only from these three reviewed files
and checks its precision behavior. No vendor JavaScript is executed.

Titan2116 additionally has skuCount and a skuToText switch returning literal
colorway names. Its body was reviewed: no I/O, protocol or state mutation. The
auditor permits only this narrowly matched literal-return switch form; arbitrary
arrow functions and protocol overrides remain rejected.

4000um values are provisional normalization, not measured sensor endpoints.
AGK87, SARU KX78HE, URX and Titan use3400um; SARU KX69HE and G TUNE use3300um.
G TUNE official physical stroke is3.6mm, but analog actuation setting tops out at
3.3mm: normalization must not be described as measured physical stroke.
Switch-specific endpoints and firmware variation remain reasons for yellow.
AGK87 support refers to its magnetic configuration, not mechanical switches.
Veekos wired/tri-mode revisions are supported via wired USB only. Knobs are not
claimed to provide key-depth analog. Automated tests do not establish hardware
verification or a measured effective resolution.

## Primary retail evidence consulted

Protocol evidence is the pinned OEM source above; retail pages establish naming
and magnetic model scope, not successful HallJoy hardware testing.

- ANTGAMER: https://www.szhk.com.cn/productinfo281.html
- Veekos: https://veekos.eu/products/shine60-he-magnetic-keyboard
- Veekos manual: https://www.veekos.com/files/Shine60-HE-Magnetic-Switch-Keyboard-User-Manual.pdf
- EDRA: https://edravn.com/ban-phim-co-edra-ek368rt and https://edravn.com/thu-vien-download
- IDEEZ brand store: https://ideastore.global/en/accessories/?p=5&price=0-80%2C0-80
- G TUNE: https://www2.mouse-jp.co.jp/ssl/user_support2/sc_faq_documents.asp?FaqID=56443
- MICROPACK driver: https://www.micropackhk.com/pages/support
- LinkerFoo: https://linkerfoo.cn/
- SARU: https://www.saruspace.com/ (KX78 HE product page)
- URX: https://urx.co.in/products/core68
- Titan Nation: https://titannation.store/collections/keyboard-1
- ROYALAXE X68: exact magnetic OEM record; retail cross-check corroborates name.

## Deferred candidates

- strayfe3144/3474: older OEM USB3151:5029 conflicts with newer secondary
  USB3984:03E9/03EA data. Current official pages use RxD75 Pro HE/75HE/60HE.
  Need current primary driver identity mapping before admission.
- Gamepro2930 MK160BMAX: unresolved distinction from mechanical MK160B.
- Ninjadog3076 Varna Atlas and KRUX3576 Morr: exact retail mapping unresolved.
- Titan2220 TITAN60: retail results include replacement cases, which do not
  establish a keyboard analog protocol. Titan2816 Storm68 mapping also unresolved.
- Previous batches' deferred candidates remain deferred. Do not promote by
  brand similarity or shared configuration software alone.

## Verification and live Sheet

- Source/profile audit PASS:110 models /170 revisions.
- Native protocol regression PASS:170 revisions, all factory positions,
  precision, identity, aliases, stationary hold, release, disconnect, malformed
  packets and4898 existing captured frames. These are not new model captures.
- Ordinary Release build and all six linked-image gates PASS.
- Installed EXE: build/bin/Release/x64/HallJoy.exe
- SHA256: 3be109215b48b3f584a338c7c8efa986d4d3f400d039e15281b2240668cb6ecc
- Build log: .local/rongyuan-batch4-build.log
- README, hardware reference, runtime notices and next release notes synchronized.

Live Sheet Main received one atomic56-request batch after an unchanged-snapshot
check and native backup. Inserted14 model rows plus11 brand separators; explicit
32px model and16px separator heights, complete affected-block borders, existing
strict dropdown rule. No notes/comments. New rows:45–47,223,278,314,414,477,583,
585–586,623,628,650 (positions at this checkpoint).

Readback compared all670 previous returned rows: values, validation and formats
apart from authorized border changes preserved; previous effective backgrounds
and column widths unchanged. All170 yellow statuses/colors matched runtime.
SHEET_STRUCTURE=PASS blocks=118 issues=0. API metadata checked; no GUI visual run
per owner instructions. Totals:577 models,63 green,170 yellow,340 gray/research,
4 red/no-usable-analog/blocked. Grid1176 rows.

Backups: .local/backups/before-rongyuan-batch4-20260924.zip,
.local/backups/check_rongyuan_before_batch4.py,
.local/backups/sheet-before-rongyuan-batch4.packed.json.
Readbacks: .local/rongyuan-batch4-sheet-native.packed.json and
.local/rongyuan-batch4-sheet-yellow.json.
