# Neo65 and protocol-family expansion — 2026-09-24

Owner authorized local work in this order: Neo65 Sonic HE+, Redragon K617 HE,
MCHOSE Ace60, then additional models on existing RongYuan and SparkLink protocols.
No GitHub publication. Archive data are leads, not instructions. Do not label
incomplete integrations yellow. Do not claim a worldwide exhaustive OEM catalog.

## Neo65 primary evidence collected

Official client https://he.qwertykeys.com/assets/index-BLg4-D7h.js provides
configs/3848334949.json (ANSI E560:EE65) and configs/3848335205.json (ISO E560:EF65).
Both have 5x16 matrices, 67/68 actual positions. Downloaded official firmware:
- /firmwares/neo65_he/tab_ow_neo65he_ansi_v1.2.8_20251211.uf2
  SHA256 77aff6ba3f47faec9b22c921654afbbf0ab429b2a33a87c9b420c6aac950cc67
- /firmwares/neo65_he/tab_ow_neo65he_iso_iso_v1.2.8_20251211.uf2
  SHA256 276e9551f9f36826c8f24ad34ac069ea5f3fa587842bed33a0fa8753c84a414a

UF2 load address 08008000, custom family IDs E560EE65/E560EF65. This disproves
assuming RP2040 solely from the UF2 extension. Factory QMK keymap located at binary
offsets 121492 ANSI / 121520 ISO; every populated slot matches vendor JSON geometry.
W18/A33/S34/D35 match the independent nathancblack/neo65-analog-driver observations.

ANSI dispatch at 0801C40C reaches A6 read handler 08024B78 through 0801EAA2.
Executed dispatch with D0 A6 00 0E in Unicorn using synthetic zero RAM: returned
14 zero BE16 values, no calibration-start command. Static handler reads depth
array, maps FFFF to zero, quantizes using current precision; no mode-setting writes.
This is component execution, not USB/device testing. Seeded all-slot regression,
ISO dispatch verification and host integration still pending.

Client defines depth units as 10000/mm, not arbitrary ADC. Per-key AA configuration
contains axisID; parseAxisID=floor(axisID/100)/100 mm. Must not blindly reuse the
third-party hardcoded idle300/full33000 range. Factory remaps/switch settings and
lifecycle need explicit handling before support promotion.

Sources and binaries retained in .local/research/neo65-20260924/.
Backup before implementation: .local/backups/before-neo65-20260924.zip.
No support status changes yet.

## Neo65 implementation and follow-up correction

Native backend24 enabled: exact ANSI/ISO caps, read-only A9/AA configured per-key
ranges and six A6 pages, matched response headers, bounded/cancelled transactions,
100ms stale-input guard and neutralization on failure/stop. Uses factory HID keys
including HallJoy Fn409; manual layouts, no device-remap import. Synthetic firmware
execution now covers all80 slots in both binaries and precision1/10/100, including
FFFF sentinel and no RAM writes outside response/stack. Local verify_firmware.py
retains the executable check. Host all-key parser/range/stale regression PASS.
First ordinary build and full native suite PASS; follow-up builds/tests pending.
Sheet C422 updated only, validation and neighbors unchanged, yellow read back.
All68 yellow rows reconciled in .local/neo65-sheet-readback.json.

Found and fixed a prior manual-layout integration defect: generated identity Token
returns0 without geometry. RongYuan Bind previously rejected native_layout::Publish(0),
blocking most manually mapped models; it now skips optional layout publication.
NativeNotice previously rejected token0 globally, hiding Tartarus/manual-model
warnings. Notices now require a connected backend and any group-specific identity
filters; protocols with exact internal admission do not require automatic geometry.
Tartarus/RongYuan/Neo65 regression now explicitly uses real token0. This supersedes
previous assumptions that nonzero dummy-token notice tests covered manual models.

## K617 official protocol review and implementation

Downloaded https://www.illumipc.com/app.js and exact config/keys files:
7153USHEXYXCPARGB.json SHA256 e5ada069dbc3655fbad06147c4e01b9e81643f66e1aa9b18c7276ae6d6ff23e8
7153BRHEXYXCPARGB.json SHA256 e0d9486fb36c93faec8419b325d8fb5a93c9604b3c6c3aa872e4273e00aadcc8
Official openTriggerTest builds 22 bytes of per-key subscription bits from matrix
positions (6x22), command21/sub02; close uses sub03. Thus the archived wake bytes
are a subscription mask, NOT saved configuration: earlier suspicion is resolved.
Current driver confirms independent row/col live events and readable travel range.
Existing HallJoy W669 backend implements this transport. Added exact factory maps
61 ANSI /63 BR positions and firmware-product matching; no broad PID-only promotion.
Manual layout identity tokens distinguish these products for yellow notices.
Generic W669 publication now preserves physical key aliases: releasing one remapped
key no longer releases another held key sharing its HID. Manufacturer presets,
calibration and polling-rate settings remain unchanged. All-key mask/event/release
and identity regression PASS. Final build, catalog/Sheet synchronization pending.

## Ace60: processed analog proven, production activation blocked

Official firmware downloaded from https://cdn.mchose.com.cn/configCenter/static/binaries/update_ace60.1424ea73_9a5d091b2432.bin
SHA256 ac7707b421f88677017122e80d7bc727b3ce78c940b2a8c12026b94410196835,
version1.20, ARM image at08004000. Exact41E4:2101 report descriptor at0800D534:
UsagePage1/Usage0,64-byte input/output payload, no report ID. Report producer
0800B090 uses immutable factory identities (61 populated slots of84). A0 bytes6/7
contain processed travel,14/15 its switch maximum (170 or175); firmware clamps
values<=9 to0 and >=maximum-5 to maximum. No host ADC auto-calibration needed.
Component Unicorn execution verifies all61 keys *7 switch types *8 depths;
.local/research/ace60-20260924/verify_report.py PASS. This is not hardware testing.

CRITICAL: official debug activation modifies config byte7 bit3 with command06.
Dispatcher080087F4 routes06 to0800CC70; at0800CC90 it calls08005B58 (flash sector
read/modify/erase/program,08005BD8/08005BE8), then reloads active configuration.
Thus this is NOT a harmless volatile subscription: repeated start/stop would
rewrite saved configuration and interruption could leave debug enabled. The
third-party mapper's unconditional set/clear is unsuitable for silent integration.
Do not ship this path or mark yellow until a volatile alternative is established
or an explicit, recoverable configuration workflow is designed. No device writes
were performed. Header prototypes retained only in local research, not compiled.
Ace60 Pro/Mix87 are not inferred compatible. Continue existing protocol families.

K617 Sheet C471 read back yellow with validation preserved. All69 yellow models
reconciled in .local/neo65-k617-sheet-readback.json. Combined ordinary Release and
six linked gates PASS (.local/neo65-k617-build.log); final full suite still pending.

## Existing protocol families — implemented second stage

Added EPOMAKER HE68 Lite, exact boards2761/2762/2883/3664 and pinned full matrices.
Source classes inherit7a5b12c9 unchanged; manufacturer Clear Mag range3.4mm used.
Native RongYuan stream now18 models/30 exact board revisions. No new firmware writes.
Added exact experimental identification for IROK MG68 Plus, ND63 Ultra, ND68 Pro,
RA68 (JingTai V2 only) through existing native SparkLink row reader. Full live
layout/depth queries, alias handling and existing gamepad output already implemented.
Official SDK confirms same03/01 layout and04/03/01 travel commands and3.5mm scale.
Manual layouts. Distinct HuoChaiRen RA68 and old JingTai V1 are not included.
No physical tests claimed. MG75 Max confirmed status remains unchanged.

Machine-readable OEM inventories now in docs/research/protocol-catalogs/:
479 RongYuan magnetism-flag records;4 explicitly excluded ->475 candidates across
106 company identifiers;419 records inherit known stream parents. These are OEM
revisions, not475 retail keyboards or enabled models. Current official IROK client:
48 RongYuan/JingTai memberships for44 unique symbols, including receiver duplicates.
No claim of a complete worldwide OEM customer list. Retail-name/protocol matching
remains necessary, e.g. HE75 V2 is not automatically HE75 V2 TMR.

Sheet refreshed before writes, metadata/conditional rules inspected. HE68 Lite and
ND63 Ultra changed to yellow; inserted MG68 Plus/ND68 Pro/RA68 alphabetically within
IROK with existing formats/dropdown copied. Ace60 changed Not investigated -> Research
incomplete (gray). Readback A1:C653: exactly those six changes, all old validations
preserved, unrelated model/status pairs unchanged; new rows all have validation.
Final affected rows C212/C287/C296/C297/C299 yellow, C378 gray. All74 yellow models
reconcile with runtime catalog via .local/protocol-expansion-sheet-final.json.
No cell notes/comments. Neo/K617 remain yellow after the three inserted rows.

First final full-suite attempt found stale AULA/Redragon layout evidence hashes
following W669 source changes. Only sources fields differed, geometry unchanged.
Regenerated five review JSONs using guarded layout pipeline; backup retained at
.local/backups/layout-integrate-vojf16cg. Second full suite and final build pending.

## Final ordinary build

Final build .local/protocol-expansion-final-build-r2.log PASS, all six linked gates
passed before installation. An initial compile caught an include placed inside an
anonymous namespace; moved it to global include scope and rebuilt successfully.
Installed build/bin/Release/x64/HallJoy.exe,9678336bytes,
SHA256464a685ced0c87a94f630653293110da2a7a2a09091544e77d82c553edbd88ee.
Ordinary flags: K4 onboard retained; camera tester disabled; targeted/forced
logging disabled. No hardware test, flashing, GitHub commit/upload or release.
Full-suite protocol cases for Neo65, SparkLink notices and all30 RongYuan revisions
have passed; remaining Windows regression cases are still running.

## Completion

Full native suite PASS: .local/protocol-expansion-final-checks-r2.log ends with
all static and portable C++ tests passed, process exit0. Includes Win32-specific
lifecycle/configuration/provider tests, all new protocols and physical alias cases.
Final production build+six gates PASS as recorded above; no remaining sync item.
Seven new experimental model entries this task: Neo65 Sonic HE+, Redragon K617 HE,
EPOMAKER HE68 Lite, IROK MG68 Plus/ND63 Ultra/ND68 Pro/RA68. Ace60 remains gray for
the concrete flash-writing activation issue; other OEM catalog entries remain leads.
Physical validation is pending but is not a prerequisite for these seven enabled
paths. All changes remain local and unpublished.


## Follow-up: HE75 Mag and NS75 (2026-09-24)

Added EPOMAKER HE75 Mag (board2520,3151:502F,eabe304d.js) and GamaKay NS75
(board2808,3151:5030,f0f99765.js) to the ordinary enabled USB stream backend22.
Both manufacturer classes inherit 7a5b12c9.js with no protocol-method overrides.
Pinned exact factory matrices, admission records and SHA256 sources; nominal
range4000um. HE75 Mag metadata explicitly specifies travel.max=4; NS75 retains
the common nominal range, with switch-specific full travel unverified. Independent
physical analog, bindings and gamepad output use the existing complete stream
path; manual layout selection and yellow notices, no hardware-test claim.
Official retail-name evidence inspected:
- https://epomaker.de/products/epomaker-he75-mag
- https://gamakay.com/products/gamakay-naughshark-ns75-hall-effect-8k-rapid-trigger

Backup: .local/backups/before-he75mag-ns75-20260924.zip.
Source/profile audit PASS:20 models/32 exact revisions. All-key protocol test
PASS, including stationary hold, release, aliases, disconnect, malformed packets,
identity, precision and4898 captured fixture packets. Initial standalone invocation
omitted keyboard_support_status.cpp and failed to link; reran with the same source
list as the maintained runner and passed. No product-code fix was needed.
Release .local/he75mag-ns75-build.log PASS and all6 linked gates PASS.
Installed build/bin/Release/x64/HallJoy.exe,9679872bytes,
SHA25678dbb3680524df4c8afdffcf3fd5169253133d9b8519c153416915c85c32df06.
No new targeted/forced logging; K4 onboard retained; camera tester disabled.

Live Sheet added Main!A213:C213 HE75 Mag and A237:C237 NS75 in brand/model order.
Status Implemented; awaiting hardware testing, permitted by preserved dropdown.
Readback A1:C662 verifies yellow RGB1/.9019608/.6392157, validation and all existing
values/formats preserved. No comments/notes. All76 yellow rows match runtime via
.local/he75mag-ns75-sheet-final.json. README/hardware/next patch notes synchronized.
No GitHub publication, flashing or physical testing.

Unpromoted leads: EPOMAKER HE68 Mag versus retail HE68 naming, MonsGeek FUN75 retail
identity, MSI STRIKE700 revision naming and Carotmas/IROK aliases still need exact
identity evidence; a common OEM parent alone is not a retail-model guarantee.


## Follow-up: HE68 / FUN75 / MSI and Mars / Mercury (2026-09-24)

Six enabled yellow model entries, LOCAL ONLY:
- EPOMAKER HE68 Mag: board2465,3151:5029,9eb51218.js,common base.
- MonsGeek FUN75: board2648,3151:502D,ba496d93.js,Akko/MonsGeek base.
- MSI STRIKE 700 HE: board3409,0DB0:32C0,acd665ba.js and
  board3595,0DB0:EBBE,a4ad6ec3.js; precision-enum base60ee4367.js.
- IROK catalog Mars75 / Mars75 Pro: CAROTMAS official product strings,
  1CA6:052B,FFB0:1; Pro secondary16 shares the same analog protocol.
- IROK catalog Mercury68 Max: CAROTMAS_MER68 / Mer68_Max class,
  1CA6:052D,FFB0:1. Existing public umbrella brand/spelling retained.

All four RongYuan classes have data-only matrices, no overriding wire methods.
Exact factory maps and admissions retained and hashed. Nominal4mm normalization
is experimental; switch-specific end travel is not asserted. MSI uses the existing
read-only precision query and rejects invalid identity/precision. HE68 retail name
is corroborated by https://epomaker.com/blogs/manuals/epomaker-he68-manual;
Mag is the OEM name. A user-reported3151:502D HE68 is NOT silently admitted as
board2465: no board reply evidence for that variant. FUN75 is an exact OEM entry;
retail launch was not established. MSI official announcement names Strike700 HE
8K Wireless; support here is only wired USB and the two exact board identities:
https://ru.msi.com/news/detail/Innovate-Beyond-MSI-Unveils-Full-Lineup-of-AI-Products-at-CES-2026-147671

SparkLink evidence pinned in ../research/protocol-catalogs/mars-mer-v2-evidence.json.
Existing semantic device/layout/depth verification remains mandatory; added exact
experimental tokens and notices, no configuration writes. Live row maps,3500-unit
nominal travel, Fn conversion, alias publication, stale-row handling and gamepad
routing are the existing enabled implementation. Mars75 Pro need not be guessed
from PID because both variants use the same path. Mercury68 SE remains gray:
several official protocol identities exist, including V2 and RongYuan/self-dev;
a general model promotion would overstate revision coverage.

Backups before guarded edits: .local/backups/before-he68-fun75-msi-20260924.zip
and before-sparklink-mars-mer-20260924.zip. A guarded README replacement stopped
on an unmatched literal (actual Neo row includes ANSI/ISO); reread and corrected
before updating docs, with no unrelated text loss.

Validation PASS: profile/source audit23 models/36 revisions; all-key stream test
with4898 captured packets, identity/precision/hold/release/alias cases; SparkLink
exact notices/confirmed-MG75 exclusion/Fn and row freshness regression.
Ordinary Release .local/next-protocol-wave-build.log PASS plus all6 linked gates.
Installed build/bin/Release/x64/HallJoy.exe SHA256
 ec9da51201473cd6910e01cbc5a4944c47380d963eef2b5488f8d37199468ee5.
K4 onboard preserved, camera tester/forced/targeted logging disabled.
No physical tests, flashing, GitHub upload or Windows GitHub CI.

Live Sheet added HE68 Mag C213, FUN75 C412, MSI C429; changed Mars75 C283,
Mars75 Pro C284, Mercury68 Max C287 from Not investigated to
Implemented; awaiting hardware testing. New MSI brand has a separator.
Readback A1:C674: all old model values/validation/formats preserved except these
three authorized statuses; all six yellow RGB1/.9019608/.6392157 with dropdown.
All82 yellow models reconcile with runtime via .local/next-protocol-wave-sheet.json.
README, hardware and next patch notes synchronized. No cell comments/notes.


## Follow-up: HE60, HE75 V2 and specific Mercury68 SE (2026-09-24)

Added four enabled experimental entries:
- EPOMAKER HE60 Wired: board3691,3151:5030,2d5290e3.js.
- EPOMAKER HE60 Wireless: board3692,3151:5030,e7ed75d2.js. USB cable only.
- EPOMAKER HE75 V2: board3518,3151:5054,0b695285.js. Not TMR.
- IROK/CAROTMAS Mercury68 SE (JingTai V2):1CA6:0540,FFB0:1.

Three RongYuan classes contain factory matrices only, inherit7a5b12c9 unchanged,
and have exact board/VID/PID admission plus independent physical publication.
HE75 V2 nominal3500um uses official Creamy Jade total travel3.5+/-0.1mm:
https://epomaker.com/products/epomaker-he75-v2
HE60 uses provisional4000um; switch range and real firmware remain untested.
Official announcement is a reservation page, not proof of retail availability:
https://epomaker.com/products/1-reservation-card-for-epomaker-he60
The Wired/Wireless variants come from the exact pinned OEM catalog. Neither is
an inference from another60% keyboard. No wireless transport support is claimed.

Mer68 SE V2 uses the same SDK layout/depth commands as the integrated V2 family.
Its override s7t keeps3500 maximum, changes settings minimum/step/default only;
these configuration controls must NOT quantize live raw row readings. Retained
3.5mm live depth normalization and semantic device/layout/travel verification.
Evidence: ../research/protocol-catalogs/mer68se-v2-evidence.json and existing SDK
wire evidence. General Mercury68 SE remains gray because other protocols exist;
new explicit variant row avoids blanket promotion. All four use manual layouts,
enabled bindings/gamepad and yellow notices. No physical-device tests claimed.
Keydous NJ68 PRO-CP remains a lead: OEM name/geometry differs from the official
NJ68-CP retail page, so these must not be merged without further evidence.

Backup .local/backups/before-he60-he75-merse-20260924.zip; guarded writes.
Profile/source audit PASS26 models/39 revisions. All-key stream test PASS including
4898 captured packets, hold/release/alias/identity/precision. SparkLink notice/Fn
and row-freshness tests PASS. Ordinary Release .local/he60-he75-merse-build.log
and all6 linked gates PASS. Installed EXE9683968bytes, SHA256
4ba1a87dd6c2b73a7304781fca18709de274d885f4bf3e20dbb4484ba0b69f2b.
No forced/targeted logging; K4 onboard retained; camera tester disabled.

Sheet inserted A212:C213 (HE60 variants),A217:C217 (HE75 V2),A293:C293
(Mercury68 SE JingTai V2). Readback A1:C684 confirms four permitted Implemented;
awaiting hardware testing values, preserved dropdowns, yellow1/.9019608/.6392157
and all existing rows/validation/formats unchanged. No comments/notes.
All86 yellow entries reconcile via .local/he60-he75-merse-sheet.json.
README/hardware/next notes synchronized. Local only, no GitHub publication.


## Follow-up: Keydous NJ68 Pro-CP and Skyloong HE (2026-09-24)

Added four enabled yellow entries using five exact RongYuan profiles:
- Keydous NJ68 Pro-CP:2515,3151:502F,b8656a24.js,nominal3500um.
- Skyloong GK61 HE:2674,3151:5029,10c9e8e1.js,nominal4000um.
- Skyloong GK68 HE:2681,3151:5029,49ce0535.js,nominal4000um.
- Skyloong GK75 HE:2450/2507,3151:502D,b3360516.js/2ad63d94.js,4000um.

All model classes inherit7a5b12c9 without wire-method overrides, exact512-byte
factory matrices pinned and audited. Existing USB stream performs board+VID/PID
admission, physical key publication, bindings and gamepad output; manual layouts.
GK75 second board is the vendor no-light revision, same factory map. Do not extend
these entries to mechanical/optical namesakes or GK61/GK68 MIX/three-mode boards
2999/2967 without resolving their retail identity separately.

Primary sources resolving prior naming uncertainty:
- https://www.keydous.com/productinfo/297997.html (explicit NJ68-Pro CP magnetic)
- https://keydous.store/ko/products/nj68-pro-cp-aluminium-alloy-he-magnetic-keyboard
  (stock Gateron Jade Pro; distinct from mechanical NJ68 Pro and non-Pro NJ68-CP)
- https://www.gateron.com/u_file/2505/21/file/GateronMagneticJadeProSwitchKS-20TF10B045NW-Y106PRO1.pdf
  (nominal3.5mm switch travel; not a hardware measurement or firmware calibration)
- https://skyloongtech.com/skyloong-gk61he-aluminum/
- https://skyloongtech.com/skyloong-gk68-he/
- https://skyloongtech.com/skyloong-gk75-he/ (advertised0.1-4mm actuation range)
Skyloong4mm remains nominal with switch-specific full-depth uncertainty. No
physical tests claimed. NJ81-CP/NJ98-CP older revisions remain leads; do not map
them to V3/V4 retail names automatically.

Backup .local/backups/before-keydous-skyloong-20260924.zip, guarded writes.
Profile/source audit PASS30 models/44 revisions. All-key regression PASS including
4898 captured fixture packets, stationary holds, release, aliases, disconnect,
malformed packets, precision, identity and notices. Ordinary Release build and
all6 linked gates PASS (.local/keydous-skyloong-build.log). Installed EXE SHA256
bb0d8e4daa5b390b57b934f97bb83ee0b6adfa0c8f5bc88e5474c1d621ab42e8.
K4 onboard retained; camera tester and forced/targeted diagnostics disabled.

Live Sheet inserted Keydous NJ68 Pro-CP A342:C342 and Skyloong A516:C518 plus
brand separator. Readback A1:C695: statuses Implemented; awaiting hardware testing,
preserved dropdowns, yellow1/.9019608/.6392157 and all old model values/formats/
validation unchanged. All90 yellow entries reconcile via
.local/keydous-skyloong-sheet.json. README/hardware/next notes synchronized.
No comments/notes, flashing, physical testing or GitHub publication.


## Follow-up: earlier Keydous profiles (2026-09-24)

Enabled exact OEM NJ81-CP2454/3151:5030/813fb990.js and
NJ98-CP2576/3151:502F/6fcdcaff.js, not blanket historical-model admission.
Both classes are matrix-only children of7a5b12c9; all factory bytes and admission
records pinned. NJ81 provisional3500um reflects listed Jade Pro switch; NJ98
3300um follows explicit vendor travelSetting.travel.max=3.3. Switch/firmware
normalization remains experimental. Exact factory input, bindings and gamepad
use existing backend22. Manual layouts, yellow notices, USB only.
OEM names omit version suffix: do NOT relabel these as V2/V3 on inference.
Published manufacturer naming/reference sources reviewed:
https://keydous.store/pages/models-guide
https://keydous.store/ko/pages/user-manual
These pages do not establish a correspondence between old board IDs and all
retail version names. NJ98-CP V3 row remains unchanged. Manufacturer NJ98 manual
and V3 manual are distinct; mixed mechanical switches do not become analog.

Rejected lead: GamaKay LK75 board2709 has magnetism flag and stream-class parent,
but official product page describes ordinary mechanical LK75; no retail magnetic
identity resolved. Do not promote merely on that flag. Primary page:
https://gamakay.com/products/gamakay-lk75-75-mechanical-keyboard-with-tft-smart-display-knob
TK75 boards3138/3167 still need exact V2 retail mapping; YUNZII generic Gaming
Keyboard/B75pro entries likewise are not silently RT75 Pro.

Backup .local/backups/before-keydous-legacy-20260924.zip, guarded edits.
Profile/source audit PASS32 models/46 revisions; all-key stream regression PASS
with4898 fixture packets, hold/release/alias/precision/identity. Ordinary Release
.local/keydous-legacy-build.log and all6 linked gates PASS. Installed EXE SHA256
8e038c16e1fbbf89e2373841d8a2a4447ce5a9d86899cb9a1d5a11e278be98b8.
K4 onboard retained; forced/targeted logging and camera tester disabled.

Sheet inserted NJ81-CP A345:C345 and NJ98-CP A347:C347. Readback A1:C702
confirms permitted Implemented; awaiting hardware testing, dropdowns preserved,
yellow1/.9019608/.6392157, all old model values/formats/validation unchanged.
All92 yellow entries match runtime via .local/keydous-legacy-sheet.json.
README/hardware/next notes synchronized. No cell comments/notes, physical tests,
flashing or GitHub publication.


## Follow-up: Akko V3; GamaKay/YUNZII identity triage (2026-09-24)

Added exact MOD007S V3 HE2683/2704 (3151:5029) and MOD007B V3 HE2871/2872
(3151:5030), ANSI/ISO factory matrices. Sources6fa249c8.js/f3865947.js and
bc8dc9a2.js/826bb826.js pinned with admissions. Data-only children of reviewed
3dd2d1f8.js, no wire overrides. Existing backend22 enables exact identity,
independent analog, bindings/gamepad. Manual layouts, nominal4000um, yellow
physical/switch validation notices. No blanket promotion of original MOD007B HE
or all Year of Dragon editions. Exact catalog V3 names retained as separate rows.
Retail S V3 name corroboration:
https://akkogear.com.vn/san-pham/ban-phim-co-akko-mod007s-v3-he-year-of-dragon-8k-hz-akko-astrolink-magnetic-switch-rapid-trigger/
B V3 exact identity rests on pinned manufacturer software record; generic global
MOD007B product history is not proof of every earlier board's wire compatibility.

GamaKay3138/3167 still not mapped conclusively to HE V2: internal TK75v5,
retail HE V2 and TMR are distinct names. Official download page lists separate
US and DE/UK firmware, but readable web output does not expose their download
URLs; direct requests timed out and curl failed. No firmware obtained or flashed.
https://gamakay.com/pages/downloads
YUNZII2555 B75pro magnetism flag conflicts with mechanical retail B75 Pro;
2834 generic Gaming Keyboard lacks exact model. Do not promote either.
https://www.yunzii.com/collections/all-products

Backup .local/backups/before-akko-v3-20260924.zip; guarded writes.
Source/profile audit PASS34 models/50 revisions. All-key regression PASS including
4898 captured fixture frames, identity/precision/hold/release/aliases/notices.
Ordinary Release .local/akko-v3-build.log and all6 linked gates PASS. Installed EXE
SHA2563f999e9cfefd3d2b8036450097deeb46e4a838a5df3bcd5a4596299582c3a6bd.
K4 onboard retained, forced/targeted diagnostics and camera tester disabled.
Sheet inserted Akko A31:C32; readback A1:C712 verifies permitted experimental
status, dropdowns and yellow1/.9019608/.6392157. Existing values/formats/validation
unchanged. All94 yellow entries reconcile via .local/akko-v3-sheet.json.
README/hardware/next notes synchronized. No cell comments/notes, hardware testing,
flashing or GitHub publication.


## Follow-up: EPOMAKER HE65 Mag and HE108 (2026-09-24)

Enabled exact USB stream profiles: HE65 Mag2376 (3151:502F), HE1083365
(3151:5030). Pinned vendor modules1f2b67b9.js/87c45d0d.js inherit reviewed
7a5b12c9.js and contain matrices only, no method overrides. Admission records,
source hashes and all512 matrix bytes retained. Independent analog, bindings
and virtual gamepad use backend22; manual layout selection, yellow notices.
HE65 Mag nominal4000um follows vendor travel range; not a hardware calibration.
HE1083300um follows stock Creamy Jade total travel3.3+/-0.1mm stated by producer.
Replacement switches and actual analog endpoints remain unverified.
Official model sources:
https://epomaker.com/blogs/manuals/epomaker-he65-mag-manual
https://epomaker.com/products/epomaker-he108
No HE65 Mec/V2 alias admission or wireless claim.

Backup .local/backups/before-epomaker-65-108-20260924.zip; guarded writes.
Profile/source audit PASS36 models/52 revisions. All-key native regression PASS,
including4898 recorded packets, stationary holds, release, aliases, malformed
packets, precision, exact identities and runtime notices. Ordinary Release and
six linked gates PASS: .local/epomaker-65-108-build.log.
Installed EXE SHA256 1b5ec538429d681dd268d16ba32056877e1540e31d69e35d484ac29422558945.

Live Sheet inserted HE65 Mag A216:C216 and HE108 A223:C223. Readback A1:C722
verifies allowed experimental dropdown value and yellow1/.9019608/.6392157;
all old model values/formats/validation preserved. All96 yellow entries match
runtime via .local/epomaker-65-108-sheet.json. README, hardware and next notes
updated. No notes/comments, physical tests, flashing or GitHub publication.


## Sheet structure repair after owner screenshots (2026-09-24)

Previous cell/color readbacks did NOT establish correct block geometry.
Cause: insertion inherited16px separator row heights; copying cell formatting
retained obsolete outline positions. Brand labels are conditionally hidden
repeated values, not merged cells. Full native A1:C1080 metadata inspected.

Repaired27 border cells across EPOMAKER, GamaKay, IROK, Keydous, MSI and Skyloong;
heights32 at346,440,522:524. Cleared validation on567 empty cells (separators and
unused trailing rows). No row moves or model/status changes. All existing model
values, dropdowns, effective colors, other cell formatting, column widths and
conditional rules compared unchanged. API structure validation PASS87 blocks,
zero remaining issues. Owner evaluates visual; no GUI test claimed.

Backup .local/backups/sheet-format-before-20260924.packed.json;
readback .local/sheet-format-after-20260924.packed.json;
plan .local/sheet-format-repair-20260924.json.
New tools/check_keyboard_sheet_structure.py reproduces before FAIL599 issues
and after PASS0, and emits a reviewable repair plan without network mutation.
Mandatory future insertion audit documented in KEYBOARD_SHEET_RULES.md and
SUPPORT_STATUS_SYNC.md. Application build unchanged; no GitHub publication.
