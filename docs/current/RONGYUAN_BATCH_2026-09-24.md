# RongYuan batch integration — 2026-09-24 (local only)

Owner requested a substantial batch instead of successive three-model waves.
17 models /22 exact revisions /10 brands enabled. This is source-backed USB
integration through the existing backend22, not physical testing or firmware
emulation. Discovery, independent per-key analog, bindings and gamepad output
are enabled; manual layouts and yellow notices. No calibration/config/flash
writes added. K4 onboard retained; no forced logs or camera test activation.

## Admitted list

| Brand | Model | Board | USB | Matrix source | Layout expression | Range um |
|---|---|---|---|---|---|---|
| AIM1 | MATATAKI (US) | 2939 | 3151:5029 | 37ee7e82.js | c.Common82_NBD_IK75 | 4000 |
| AIM1 | MATATAKI (US) | 3026 | 3151:5029 | c68f6ce9.js | c.Common82_MK25058 | 4000 |
| EvoFox | Ronin HS65 | 3669 | 3151:5029 | d9697262.js | c.Common66_RoninHS65 | 4000 |
| FL ESPORTS | Blend HE | 2699 | 3151:5029 | 7feb160e.js | c.Common108_K2402 | 4000 |
| FL ESPORTS | GP75 HE | 2669 | 3151:5030 | 8266ac9b.js | c.Common81_K242UK | 4000 |
| FL ESPORTS | MK870 HE | 2703 | 3151:5030 | c846f814.js | c.Common87_MK25022B | 4000 |
| FL ESPORTS | NX108 | 3012 | 3151:5030 | 52b536f6.js | c.Common108_K2402V2 | 4000 |
| FL ESPORTS | NX108 | 3336 | 3151:5029 | bddf8b20.js | c.Common108_K2402V2 | 4000 |
| FL ESPORTS | NX68 Pro | 2447 | 3151:5030 | 68cfa7f0.js | c.Common68_K80AWQ68 | 4000 |
| FL ESPORTS | X80 HE | 2988 | 3151:5030 | 824b9a60.js | c.Common87_MK25022B | 4000 |
| FL ESPORTS | X80 HE | 3335 | 3151:5029 | 3e594e14.js | c.Common87_MK25022B | 4000 |
| GAMEPOWER | Tirus HE80 | 3108 | 3151:5030 | 27e879a6.js | c.Common83_TIRUSHE80 | 4000 |
| GAMEPOWER | Tirus HE80 | 3725 | 3151:5029 | f1efeffa.js | c.Common83_TIRUSHE80 | 4000 |
| KYSONA | KM82 HE | 3340 | 3151:502D | fe7610c4.js | c.Common82_SG9000 | 4000 |
| MEETION | Magic A68 | 3362 | 3151:502D | cb239e53.js | c.Common68_MK230 | 3300 |
| MEETION | Magic A75 | 3367 | 3151:5029 | d9dc3e0a.js | c.Common82_SG9000 | 3300 |
| Nyfter | Nyfboard HE 61K | 2960 | 3151:5029 | a9018ad4.js | c.Common61_DK61HE | 4000 |
| Nyfter | Nyfboard HE 82K | 2752 | 3151:502F | 24abefa9.js | c.Common81_DK82UKDE | 4000 |
| OUSAID | HG68 HE | 3316 | 3151:5030 | 50dc2a5e.js | c.Common68_DK68HE | 4000 |
| OUSAID | HG68 HE | 3414 | 3151:5029 | 540c288a.js | c.Common68_kc68rt | 4000 |
| SAVIO | ASTRAL | 3151 | 3151:5029 | 95c9ab28.js | c.Common82_SG9000 | 4000 |
| Syntech | Chronos 68 | 2446 | 3151:502D | ff0ca9f8.js | c.Common68_ZAP68 | 4000 |

## Evidence and scope

Pinned Womier3.2.15 manufacturer software records provide exact identities,
loader-to-class association and factory matrix. Sources retained in
../research/rongyuan-stream with SHA256, admission records and source-lock.
Every admitted class is data-only with no method override. OUSAID3316/3414
inherits already-reviewed precision-enum parent60ee4367.js; the other20 use
7a5b12c9.js. All are routed only after exact board/VID/PID matching.

The known stream is report5 / command1B / LE16 travel / physical slot, with
firmware unit negotiation. No inference from a shared product name or USB PID
alone. Every factory matrix byte was audited and the complete set exercised by
the generic all-key regression. AIM1 board3026 contains two Space positions
(slots41/47): existing physical publication correctly merges their values by max.
The old test incorrectly required unique factory HID assignments. It now checks
all physical publications and releases each alias while ensuring remaining
positions stay held; this tests all profiles without keyboard-specific cases.

Range3300 for MEETION follows explicit vendor travel limits (not a measured
sensor endpoint); other ranges remain nominal4000. Switch swaps and endpoints
are experimental uncertainties. Retail throughput/precision marketing is not
an integration timing measurement. EvoFox's published0.1-0.3mm stroke wording
is not taken as total physical travel. FL names use the exact HE catalog entries;
mechanical namesakes are excluded. GP75 factory revision uses Common81_K242UK.
Nyfter uses the two exact factory profiles, without a blanket claim covering
both HE1.0/2.0. AIM1 covers only the two US revisions; the generic/JIS-ambiguous
Sheet row remains gray. No wireless support is asserted for tri-mode boards.

Primary retail corroboration (protocol evidence remains pinned source code):
- AIM1: https://aim1.gg/products/matataki
- EvoFox: https://www.amkette.com/products/evofox-ronin-hs65-hall-effect-wired-mechanical-gaming-keyboard
- GAMEPOWER: https://gamepowerpc.com/keyboard/tirus-he80/downloads
- KYSONA: https://shop.kysona.com/products/kysona-km82-he-rapid-trigger-keyboard-magnetic-switch-with-0-005mm-rt-8k-polling-rate
- MEETION: https://www.meetion.com/magica75.html and https://www.meetion.com.cn/video/magic-a68-pro-grade-65-compact-mechanical-keyboard.html
- Nyfter: https://nyfter.com/pages/software and https://nyfter.com/collections/keyboard?page=2
- SAVIO: https://www.savio.net.pl/produkt/klawiatura-magnetyczna-astral-outemu-white-jade-biala/
- Syntech: https://syntechhome.com/products/chronos-68-rapid-trigger-magnetic-keyboard
FL ESPORTS and OUSAID exact names/identities in this batch are grounded in the
pinned manufacturer catalog, not an independently verified retail-page mapping.

## Deferred during this batch

- FL Q75HE3155/3157 and APEX753248: VID38A9/PID0000 catalog placeholders;
  resolve production USB identity before admission.
- Royal Kludge A72HE3332: different4796d290.js parent;3333 common parent but
  same product name, different maps. Resolve both revision scopes first.
- DELUX RTS1 V22563: hybrid keypad metadata excludes membrane functions,
  including KeyM. Needs explicit analog-position review, not blanket all-key claim.
- ABKO NX1082973: same internal hardware name as FL NX108; separate company/board
  record. Retail alias needs review; FL admission does not imply ABKO admission.
- Earlier Ace60, GamaKay TK75v5/HE V2, YUNZII B75pro identity conflicts remain
  unchanged; no promotion simply to increase batch size.

## Verification and delivery

Backup: .local/backups/before-rongyuan-batch-20260924.zip.
Source audit PASS53 models /74 revisions. All-key regression PASS74 revisions,
4898 existing captured packets, malformed/precision/identity/hold/release/alias
and notice cases. Ordinary Release+6 linked gates PASS, log
.local/rongyuan-batch-build.log. Installed EXE SHA256:
051baf33b4654db8f4bcb5b780374478cbbeafeea5e04fc7b9a59d4a5438791c

README, hardware details, next notes and generated runtime notices synchronized.
Google Sheet added17 model rows and9 separators in one64-request batch after
fresh equality check; no existing model/status value changed. Exact new rows:
AIM1 C6; EvoFox C226; FL ESPORTS C243:C248; GAMEPOWER C261; KYSONA C368;
MEETION C418:C419; Nyfter C469:C470; OUSAID C474; SAVIO C531; Syntech C562.
Readback Main!A1:C1106: all old512 model rows preserve values/validation/colors
and non-border formatting. All113 yellow entries reconcile with runtime.
Structural audit PASS96 blocks, zero issues: borders, minimum model heights,
separator heights, blank dropdowns and full model validation checked.
Backups/readbacks:
.local/backups/sheet-before-rongyuan-batch.packed.json
.local/rongyuan-batch-sheet-native.packed.json
.local/rongyuan-batch-sheet-yellow.json
No notes/comments, physical tests, flashing, GitHub publication or GUI testing.
