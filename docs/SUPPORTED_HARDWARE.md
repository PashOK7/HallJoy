> 2026-09-24 visual layout update: [41 existing experimental models](current/ALL_YELLOW_LAYOUTS_2026-09-24.md) gain exact ANSI presets and automatic selection. Some revisions/regions remain manual; protocol support statuses below are unchanged.

# HallJoy hardware compatibility

Current release: **1.6.3**. The additions previously marked local or unreleased
in the historical sections below are included in this release; experimental
statuses and model-specific requirements still apply.
See [release notes](releases/RELEASE_NOTES_v1.6.3.md).

## How to read this list

Implemented support does not mean every model, firmware and connection has been
physically tested. Native backends validate their expected interface and protocol;
a brand name or shared USB ID alone is insufficient. A layout describes physical
geometry and is not evidence of analogue support. Digital presses are not used
to synthesize analogue depth.

- **Tester-confirmed:** evidence exists for the named model and tested setup.
- **Implemented / protocol-reviewed:** a route exists, with limits recorded below;
  this does not claim a hardware test for each model.
- **Experimental:** enabled in the ordinary candidate where stated, with testing
  notices; not synonymous with a disabled diagnostic-only backend.
- **Bundled runtime:** supported through the pinned private analogue runtime;
  users do not need a separate Wooting SDK or Universal Analog Plugin install.
- **Disabled:** retained research or detection notices do not enable analogue input.

## Implemented model inventory

| Brand | Models | Notes |
|---|---|---|
| AJAZZ | **AK820 MAX RGB** | Wired AJAZZ x NACODEX version; native raw analog with automatic per-key ranges. Published in 1.6.2 after tester confirmation. |
| Aula | **WIN 60 HE MAX**, **WIN 60 HE**, **WIN 68 HE**, **KP-TE153**, **MINI 60 HE**, **MINI 60 HE Pro**, **MINI 60 HE MAX** | MINI 60 models: wired USB. PRO support was added in 1.5.3 with tester-confirmed independent depths. Base MINI 60 HE (V1.18) and MAX (V1.52) are included in 1.6.1 using their verified common protocol. The 2.4 GHz receivers are not supported. |
| Aula (experimental, wired USB) | **HERO84 HE**, **HERO 68 HE**, **HERO 68 Air**, **HERO 68 MINI**, **HERO 99 HE**, **WIN 68 HE Ultra**, **WIN 60 HE PRO**, **WIN 68 HE PRO**, **WIN 68 HE MAX**, **HERO 68 HE PRO** | Complete input paths enabled with yellow notices. Known protocols, exact identities and automatic layouts; remaining range/firmware behavior awaits hardware feedback. No tester is required to use them. |
| ATTACK SHARK | **R85 HE**, **X65 Pro** | R85 HE: tester-confirmed wired analog, enabled in 1.6.2 with shared HID access; no testing notice. R85 Ultra remains experimental. X65 Pro: tester-confirmed analogue input and gameplay, including external gamepad output with Block Bound Keys enabled. |
| Chilkey (experimental, wired USB) | **Slice75 HE** | Native full-matrix travel,80 keys including Fn, automatic ANSI layout and live base assignments. Official1.1.7.3 firmware/protocol review; no physical test. Default3.3mm scale, alternate switches unverified. |
| EPOMAKER (experimental, wired USB) | **G84 HE** | Two exact board revisions; native read-only depth snapshots,84 keys including Fn, automatic ANSI layout and base assignments. Default3.5mm scale; hardware/switch validation pending. |
| MonsGeek (experimental, wired USB) | **M1 V5 HE** | Exact HE board admission; native read-only depth snapshots,82 keys including Fn, automatic ANSI layout and base assignments. Default3.6mm scale; hardware/switch validation pending. TMR and wireless receivers are not included. |
| Irok | **MG75 Max**, **MG75 Pro (experimental)**, **NA87** | NA87 support added in 1.5.3, with tester feedback and logs. **NA87 Pro is a different model; exact JingTai V1 USB revisions now have local experimental support (see below).** MG75 Pro uses native SparkLink V1 travel reads with automatic layout and base-layer remaps; hardware testing is pending and an amber Discord testing notice is shown. |
| Redragon | **K673RGB-M**, **K673WB-RGB-M** | Exact W669 profiles exist for K673RGB-M BR/UK and K673WB-RGB-M US. BR has physical evidence; UK/US have source-profile and automated evidence. Other compatible magnetic models may work but are untested. |
| MADLIONS | **MAD 68 Pro R**, **MAD60HE**, **MAD68HE**, **MAD68R** | — |
| ATK | **Hex80** | — |
| SayoDevice | **O3C** | O3C depth is tested. Automatic layout always has three independent keys, labelled from device bindings or Key 1/2/3 if unavailable. Manual layout uses Z/X/C. New binding reads await device validation. Other SayoDevice models are unconfirmed. |
| IPI / QBZ | **QBZ75**, **Aurora 75**, **Aurora75 PRO**, **QBZ65**, **AURORA65**, **AURORA65W**, **RAIN65**, **flash68** | QBZ75 and its Aurora 75 protocol/layout alias are supported; physical evidence belongs to the QBZ75-compatible route. Other listed profiles have complete implementations with yellow notices; this does not claim individual hardware tests. AURORA65W receiver support is unverified. Plus variants are not included. |
| Razer | **Huntsman V2 Analog**, **Huntsman Mini Analog**, **Huntsman V3 Pro**, **Huntsman V3 Pro Mini**, **Huntsman V3 Pro Tenkeyless** | — |
| Keychron | **Q1 HE**, **Q2 HE ANSI**, **Q3 HE**, **Q4 HE ANSI**, **Q5 HE**, **Q6 HE**, **Q12 HE**, **Q1 HE 8K**, **Q3 HE 8K**, **Q5 HE 8K**, **Q6 HE 8K**, **K2 HE**, **K3 HE**, **K4 HE**, **K6 HE ANSI**, **K8 HE**, **K10 HE** | Requires [custom firmware](https://analogsense.org/firmware/). Ready-made images are available for some models. For other Keychron HE models, the linked source changes can be applied to Keychron's published firmware source and built for the exact model and ANSI/ISO/JIS variant. |
| Lemokey | **P1 HE ANSI**, **P1 HE ISO** | — |
| NuPhy | **Air60 HE**, **Air75 HE**, **Field75 HE** | — |
| DrunkDeer | **A75**, **A75 Pro**, **G60**, **G65**, **G75** | — |
| Wooting | **60HE**, **60HE+**, **60HE v2**, **80HE**, **80HE+**, **One**, **Two**, **Two HE**, **UwU / UwU RGB** | Bundled analogue runtime. New v2 / Split / UwU layouts are in the 1.6.0 development build; see below. |

## Local additions after 1.6.2 (unreleased)

The following wired USB integrations are enabled with yellow notices in the
local candidate. They are not included in the published 1.6.2 executable.
Detection, per-key analog values, HallJoy bindings and virtual gamepad output
are implemented. No physical testing of these models is claimed.

| Brand | Models | Exact reviewed boards |
|---|---|---|
| Akko | TAC75 HE | 2782 |
| GamaKay | TK75 TMR | 3590 ANSI, 3591 ISO |
| Keydous | NJ80-CP V3 HE, NJ81-CP V3 HE, NJ98-CP V4 HE | 3466, 3459, 3496 respectively |
| MonsGeek | FUN60 Pro, M1 V5 TMR | 2600/2785; 2949 respectively |
| Womier | SK75 TMR | 2518, 3804 |
| YUNZII | RT75 Pro | 2865, 3100, 2445, 3755 |

Second local batch (same enabled stream, distinct factory maps):

| Brand | Models | Exact reviewed boards |
|---|---|---|
| Akko | MOD 007 V5 HE; Ray68 HE | 2453; 2743/2924 |
| GamaKay | NS68; TK75 HE | 2572/2638; 2334/2501 |
| MonsGeek | FUN68 HE; M2 V5 HE; M3 V5 HE | 2811; 2845; 2874 |
| Womier | M68 HE Pro | 3719 |

These eight additions use manual layouts/factory-position bindings. They retain
the same yellow status, wired-only scope and provisional 4 mm full-travel scale.
No automatic geometry or physical-device test is claimed for this batch.

The new native RongYuan event stream is separate from the existing M1 V5 HE/G84
snapshot route. It requires matching board identity and USB collections, reads
base assignments without changing them, and preserves stationary key holds.
Full-travel normalization is provisionally 4 mm; exact switch ranges and firmware
behavior remain untested. Keydous requires its advertised precision enum.
Automatic geometry is supplied for TK75 TMR ANSI/ISO and M1 V5 TMR ANSI;
other models can use manual layouts and factory-position bindings. Magnetic
switch positions only; mechanical switches do not gain analog sensing.
Wireless receivers/Bluetooth are not included.

Evidence and validation: [integration record](current/RESEARCH_STREAM_INTEGRATION_2026-09-24.md).

## Local Tartarus Pro addition (unreleased)

Razer Tartarus Pro (1532:0244) has a native passive reader for RID6 and its20
physical analog channels (256 raw levels). Razer Synapse must remain running to
provide the analog stream. The backend opens only the matching report collection
for shared read access and never sends a mode/configuration/lighting command.
Connected status requires a valid analog packet, not merely a matching USB device.
Tartarus V2/Chroma/original models are not included.

Bindings use factory-position labels (1..5,Tab/Q/W/E/R,Caps/A/S/D/F,
Shift/Z/X/C/Space); Synapse remaps are not imported. Manual layouts/custom presets
can display these keys. Wheel/d-pad/thumb digital controls are not promoted to
analog. No automatic geometry or physical test is claimed. Yellow notice is enabled.
See [implementation record](current/TARTARUS_PRO_INTEGRATION_2026-09-24.md).

## Additional routes and validation evidence

| Route | Current behavior and evidence | Remaining limits |
|---|---|---|
| ATTACK SHARK RY5088 | 37 exact reviewed board profiles enabled over wired USB, plus the R68 HE alternate USB identity established by log31 (local fix; analog retest pending); 16 automatic ANSI layout groups. X65 Pro tester reported independent depth and gameplay; R85 HE tester log confirms changing wired depth and successful shared access. | Other admitted revisions are enabled with yellow notices; no tester prerequisite. Factory/Fn maps only; custom remaps and receivers are not enabled. Provisional normalization and per-bank limits remain unresolved. X65 Pro and R85 HE have ordinary support; other admitted models retain a yellow testing notice. |
| IROK MG75 Pro | Separate native SparkLink V1 route, independent travel for 81 keys including Fn, automatic layout/base assignments, vendor 3.5 mm scale. Enabled with orange notice. | No physical tester result yet; not the MG75 Max V2 protocol. |
| AULA HERO / WIN experimental families | Exact identity, automatic ANSI layout, live/base maps, analog bindings and virtual gamepad enabled. HERO uses addressed travel; WIN PRO/MAX and HERO PRO use the existing RM6x21 full capability proof. | Yellow notices cover remaining range/firmware nuances. No tester prerequisite. See the [2026-09-22 integration](current/AULA_EXPERIMENTAL_SUPPORT_SYNC_2026-09-22.md). |
| GravaStar Mercury V75 / Pro / Lite | Legacy identities 1CA5:2201, 1CA5:2202, 1CA2:2201 plus board proof; 79-key geometry and session remaps. Owner confirms a user reported working support (2026-09-20). | The new confirmation does not identify every tested revision; do not extend it to all Pro/Lite/newer revisions. Earlier source/firmware evidence is retained. |

ATTACK SHARK X65 Pro ordinary support was approved on 2026-09-21. The isolated
Forza/Roblox input-switching report is no longer reproducing; no specific fix or
root cause is claimed. Broader family investigation remains deferred; model
support does not establish hardware testing of every revision or switch bank.

## Validation details and authoritative implementation records

- [NA87 and AULA MINI 60 HE Pro](current/NA87_MINI60_STANDARD_SUPPORT_2026-09-19.md):
  ordinary wired support with tester logs. NA87 Pro is a different model.
- AULA WIN 60 HE MAX and IROK MG75 Max have earlier physical test evidence.
  Redragon K673RGB-M evidence applies to the exact BR firmware, not every
  Redragon magnetic keyboard. Detailed original test records are preserved in
  the [historical hardware snapshot](archive/HARDWARE_STATUS_BEFORE_2026-09-20.md).
- [ATK Hex80](current/ATK_HEX80_NATIVE_FIXES_2026-09-14.md): corrected matrix
  slots, 87 factory keys including Fn, 32/128-byte payload handling and freshness.
- [IPI / QBZ](current/IPI_NATIVE_SUPPORT_2026-09-14.md): eight exact UUID
  profiles, complete maps, device calibration and Fn. Existing physical evidence
  covers the QBZ75-compatible route; other models await individual hardware testing. AURORA65W receiver forwarding is unverified.
- [MG75 Pro](current/MG75_PRO_INTEGRATION_2026-09-19.md),
  [HERO84](current/HERO84_INTEGRATION_2026-09-19.md), and
  [GravaStar](current/GRAVASTAR_LAYOUTS_2026-09-19.md): use these current
  integration records instead of earlier disabled/manual-only descriptions.
- [ATTACK SHARK exact revisions](current/ATTACK_SHARK_FAMILY_SUPPORT_2026-09-20.md),
  [all automatic layouts](current/ATTACK_SHARK_FAMILY_LAYOUTS_2026-09-20.md),
  and [remaining scale questions](current/ATTACK_SHARK_BANK_INIT_2026-09-20.md).
- [SayoDevice O3C](current/SAYO_O3C_CONFIG_2026-09-19.md): depth is tested;
  new config readback awaits hardware validation. Automatic mode always keeps
  three physical keys, with real labels when readable or Key 1/2/3 otherwise.
  Manual mode uses factory Z/X/C.
- [Keychron catalog](current/KEYCHRON_HE_LAYOUT_CATALOG.md): custom firmware
  is required for the discussed HE routes; do not promise stock-firmware support.
  Ready-made images cover some models; other exact variants require applying
  the published source changes and building matching firmware. Layout additions
  do not require repeating this already established protocol investigation.
- [Wooting split layouts](current/WOOTING_EXTENDED_LAYOUTS_2026-09-19.md):
  independent physical Space/Fn channels implemented; split hardware validation
  remains pending. Region/split selection is manual.

NuPhy BH65, Field75 HE V2, Halo65 HE and WH80 remain **Research incomplete** (gray). Their unresolved report-field mapping is not merely a range endpoint uncertainty, so they do not qualify for the implemented yellow category.

The complete [yellow model catalog](development/keyboard_support_notices.json) generates runtime notices and is checked against the live Sheet.

## Disabled research

IROK ND75 and ROG Azoth 96 HE are not enabled in ordinary builds. The earlier NA87 Pro hold is superseded for the exact JingTai V1 identities documented below.
Their retained research and device notices are not working support claims.

See the [complete layout catalog](KEYBOARD_LAYOUTS.md) for exact regional
variants and the [HallJoy Discord](https://discord.gg/5FQ297yZh) for reporting a
specific device result. Historical audits are evidence at their recorded date;
later integration records and owner decisions supersede their status statements.


## AJAZZ AK820 MAX RGB (local candidate, 2026-09-22)

Ordinary Release now enables the tester-confirmed wired RGB route. Exact
admission is SG8994HERGB V1.13.17, including its trailing firmware padding.
No-light/Ultra/wireless variants are not covered. Raw sensor values use the
same session-local dynamic bounds and median filter as tester revision5;
normalized percentages are not calibrated millimetres. No forced logging,
calibration writes or diagnostic depth subscription. Live device assignments
and Fn are retained; no new automatic geometry preset is claimed.

See current/AJAZZ_AK820MAX_REVIEW_2026-09-20.md for evidence and validation.

## Local Neo65 Sonic HE+ addition (unreleased)

Native wired E560:EE65 ANSI / E560:EF65 ISO admission on FF60:61, 33-byte native
reports. All 67/68 factory positions including Fn are bound; six read-only D0 A6
pages cover the 80-slot matrix. Uses per-key switch travel and enabled top/bottom
dead zones read from D0 A9/AA, without changing calibration, mode or saved settings.
Unknown range/packet shape is rejected. No Synapse or manufacturer app required;
close NeoFlux while HallJoy owns the vendor collection. Curves/bindings/ViGEm are
normal HallJoy paths. Manual layouts use factory positions; device remaps and
automatic geometry are not imported. Validated by official firmware component
execution and host regression, not physical hardware. Firmware/switch behavior
remains experimental with a yellow notice.

Experimental Redragon K617 HE wired input now uses the existing W669 transport,
with exact manufacturer ANSI (7153USHEXYXCPARGB) and Brazilian
(7153BRHEXYXCPARGB) identities and 61/63-position factory maps. Normal keymap
reads, live independent travel events, firmware-reported range, start/stop and
HallJoy bindings/gamepad output are enabled; hardware validation is pending.

### Further native protocol additions (local, unreleased)

- EPOMAKER HE68 Lite: exact RongYuan boards2761/2762/2883/3664, factory matrices,
  existing1B event stream and precision handling. USB only; manual layout.
  Default3.4mm normalization matches the manufacturer's Clear Mag switch travel;
  switch replacements may require range validation. All keys, bindings and virtual
  gamepad enabled; experimental yellow notice. No physical-device test.
- IROK MG68 Plus, ND63 Ultra, ND68 Pro and RA68: official JingTai V2 row protocol,
  read-only device info/layout/travel queries through existing native SparkLink.
  Exact1CA6:052A/0531/052C/0528 FFB0:1 identities; live protocol admission required.
  Default3.5mm scale per official SDK, live base assignments, manual layouts.
  Enabled analog/bindings/gamepad with yellow notice. No physical-device tests.
  RA68 on the separate HuoChaiRen protocol is NOT included. MG75 Max stays confirmed.
- MCHOSE Ace60 remains research-only: investigated firmware1.20 activates analog
  diagnostics by writing flash configuration. No automatic production activation
  or experimental support claim is made for it.


## Additional local experimental USB support (2026-09-24)

EPOMAKER HE75 Mag and GamaKay NS75 use the enabled RongYuan stream backend,
with exact board identities and manufacturer factory key maps. Detection,
independent analog values, bindings and virtual gamepad output are enabled.
Both show yellow notices: physical testing and switch-specific full-travel
normalization remain unverified (4 mm nominal range). Wired USB only; manual
layout selection. These additions are not in the published 1.6.2 release.


## Further local experimental USB profiles (2026-09-24)

EPOMAKER HE68 Mag (board2465), MonsGeek FUN75 (2648), MSI STRIKE 700 HE
(3409/3595): exact board+USB admission, factory-position analog, bindings and
virtual gamepad through backend22. MSI queries firmware precision. Nominal4mm
normalization remains switch-dependent and unverified. Other HE68 USB variants
are not inferred. FUN75 name comes from the pinned OEM catalog, not proof of a
retail launch. All use manual layouts and yellow notices.

IROK catalog entries Mars75 / Mars75 Pro / Mercury68 Max correspond to the
official IROK client CAROTMAS Mars75 / Mars75 PRO / Mer68_Max classes. Enabled
JingTai V2 notices for1CA6:052B/052D,FFB0:1 after existing semantic admission;
live key-map and independent row-depth reads,3.5mm nominal scale. Pro shares PID
with base Mars75 and uses the same protocol; no profile-specific behavior is
guessed from the PID. Firmware/range testing pending. Mercury68 SE is excluded
from this promotion because the catalog contains several incompatible protocols.
These are local additions, not published1.6.2 support.


## HE60 / HE75 V2 / Mercury68 SE additions (local, 2026-09-24)

EPOMAKER HE60 Wired (3691), HE60 Wireless (3692) and HE75 V2 (3518)
use exact RongYuan USB identities and factory maps. Wireless is a model name;
HallJoy support here requires a USB cable. HE75 V2 uses3.5mm nominal travel
from the official Creamy Jade specification; TMR is not included. HE60 uses
a provisional4mm nominal range; its official reservation page and OEM catalog
do not constitute a hardware test or confirmation of retail availability.
IROK/CAROTMAS Mercury68 SE (JingTai V2) uses1CA6:0540,FFB0:1 and the existing
3.5mm SparkLink live-row path. Other Mercury68 SE protocol variants remain
unpromoted. All four entries have enabled analog/bindings/gamepad, manual
layouts and yellow testing notices.


## Keydous / Skyloong additions (local, 2026-09-24)

Keydous NJ68 Pro-CP (2515), Skyloong GK61 HE (2674), GK68 HE (2681),
GK75 HE (2450/2507) have enabled exact USB admission, factory-position analog,
bindings and virtual gamepad via the existing RongYuan stream backend.
Manual layouts and yellow notices; no physical testing claimed. NJ68 Pro-CP
uses3.5mm nominal travel for stock Gateron Jade Pro. Skyloong uses4mm nominal
travel, with switch variants unverified. Mechanical/optical namesakes, MIX
variants and unlisted firmware identities are not inferred from these profiles.


## Earlier Keydous USB profiles (local, 2026-09-24)

NJ81-CP (board2454,3151:5030) and NJ98-CP (2576,3151:502F) use enabled
RongYuan stream input with exact factory maps, bindings and virtual gamepad.
Manual layouts, yellow notices. These names follow the OEM catalog; no claim
is made that every historical revision shares this protocol. NJ98-CP V3 is
not promoted by name alone. NJ81 uses provisional3.5mm normalization for Jade
Pro; NJ98 uses the vendor3.3mm travel setting. Switch/firmware scale validation
remains pending. No physical tests or wireless support claimed.


## Akko V3 profiles (local, 2026-09-24)

MOD007S V3 HE (boards2683/2704) and MOD007B V3 HE (2871/2872), ANSI/ISO,
have enabled exact RongYuan USB admission, factory-position analog, bindings
and virtual gamepad. Manual layout selection and yellow notices. Nominal4mm
travel; switch range and hardware operation remain unverified. These exact V3
identities do not establish coverage of all MOD007B HE or Year of Dragon editions.
No firmware changes or wireless transport support are required/claimed.


### Local EPOMAKER HE65 Mag / HE108 extension (2026-09-24)

Experimental USB RongYuan stream support: HE65 Mag board2376 (3151:502F),
HE108 board3365 (3151:5030), exact factory matrices from 1f2b67b9.js and
87c45d0d.js. Both inherit the reviewed common stream unchanged. Independent
key analog, bindings and gamepad enabled; manual layouts, yellow notices.
HE65 Mag nominal4mm; HE108 stock Creamy Jade total travel3.3mm. Switch-specific
range and hardware validation remain pending. No wireless claim, HE65 Mec/V2
alias expansion, physical-test claim or GitHub publication.
Sources: https://epomaker.com/blogs/manuals/epomaker-he65-mag-manual and
https://epomaker.com/products/epomaker-he108 .


### Local RongYuan batch: 17 models / 22 revisions (2026-09-24)

USB analog, bindings and virtual gamepad enabled; yellow hardware/range notices,
manual layouts. Exact admitted revisions below; namesakes and wireless transport
are not implied. No physical-device test or firmware emulation is claimed.

| Brand | Model | Board | USB VID:PID | Nominal range |
|---|---|---|---|---|
| FL ESPORTS | GP75 HE | 2669 | 3151:5030 | 4000 um |
| FL ESPORTS | MK870 HE | 2703 | 3151:5030 | 4000 um |
| FL ESPORTS | NX68 Pro | 2447 | 3151:5030 | 4000 um |
| FL ESPORTS | Blend HE | 2699 | 3151:5029 | 4000 um |
| FL ESPORTS | X80 HE | 2988 | 3151:5030 | 4000 um |
| FL ESPORTS | X80 HE | 3335 | 3151:5029 | 4000 um |
| FL ESPORTS | NX108 | 3012 | 3151:5030 | 4000 um |
| FL ESPORTS | NX108 | 3336 | 3151:5029 | 4000 um |
| Syntech | Chronos 68 | 2446 | 3151:502D | 4000 um |
| Nyfter | Nyfboard HE 82K | 2752 | 3151:502F | 4000 um |
| Nyfter | Nyfboard HE 61K | 2960 | 3151:5029 | 4000 um |
| AIM1 | MATATAKI (US) | 2939 | 3151:5029 | 4000 um |
| AIM1 | MATATAKI (US) | 3026 | 3151:5029 | 4000 um |
| SAVIO | ASTRAL | 3151 | 3151:5029 | 4000 um |
| GAMEPOWER | Tirus HE80 | 3108 | 3151:5030 | 4000 um |
| GAMEPOWER | Tirus HE80 | 3725 | 3151:5029 | 4000 um |
| KYSONA | KM82 HE | 3340 | 3151:502D | 4000 um |
| OUSAID | HG68 HE | 3316 | 3151:5030 | 4000 um |
| OUSAID | HG68 HE | 3414 | 3151:5029 | 4000 um |
| MEETION | Magic A75 | 3367 | 3151:5029 | 3300 um |
| MEETION | Magic A68 | 3362 | 3151:502D | 3300 um |
| EvoFox | Ronin HS65 | 3669 | 3151:5029 | 4000 um |

Ranges are provisional except vendor explicit travel bounds; replaced switches
and firmware endpoint differences may need adjustment. AIM1 covers US only.
Nyfter scope is the exact factory profiles, without blanket 1.0/2.0 alias claims.
See current RONGYUAN_BATCH_2026-09-24.md for evidence and deferred candidates.

## RongYuan stream expansion: second batch (2026-09-24)

Experimental wired USB support; detection, analog input, bindings and gamepad output enabled. No physical-device test is claimed. Exact factory matrices and board identities are pinned in `research/rongyuan-stream/`. Wireless transport is not admitted.

| Model | Board revisions | Normalization (mm) |
|---|---|---|
| AJAZZ AK680 MAX HE | 2255, 2336, 2343, 2371 | 4.0 |
| AJAZZ AK680MC | 2605, 2609, 2608 | 4.0 |
| AJAZZ ALUX60 | 2621, 2622, 3218 | 3.3 |
| AJAZZ ALUX68 AIR | 3025 | 4.0 |
| AJAZZ ALUX68 PRO | 2592, 2593, 2599 | 3.4 |
| AJAZZ NS67 | 2810 | 4.0 |
| AJAZZ NS67 PRO | 2746 | 4.0 |
| AJAZZ NS87 | 3065, 3112 | 4.0 |
| ARDOR GAMING Radiant | 2957 | 4.0 |
| ASTROMEDA AMGK80-001 | 3536, 3537 | 4.0 |
| DSPIXEL DS KEY | 2788, 3189 | 3.3 |
| DSPIXEL Magic 80 | 3714 | 3.3 |
| HATOR Skyfall 65 MAG Ultima 8K Wireless | 2876, 2919, 2920 | 3.5 |
| HATOR Skyfall 65 MAG Ultra 8K | 2917, 2918 | 3.5 |
| HATOR Skyfall 80 MAG Ultima 8K Wireless | 2800, 2857, 2858 | 3.5 |
| HATOR Skyfall 80 MAG Ultra 8K | 2853, 2856 | 3.5 |
| KiiBOOM Cybrix29 | 2886 | 4.0 |
| MAMBASNAKE M82 HE | 2335 | 3.4 |
| MAMBASNAKE X60 HE | 2368 | 4.0 |
| PIIFOX DEFENDER 68 | 2499 | 4.0 |
| PIIFOX ER75 PRO | 3473 | 4.0 |

Ranges without a vendor limit remain provisional (4 mm); they do not imply measured sensor travel. HATOR uses the stock 3.5 mm switches; ALUX60 and DSPIXEL use vendor 3.3 mm limits, ALUX68 PRO uses 3.4 mm. M82 HE uses the advertised 3.4 mm actuation span. Cybrix29 support covers analog keys, not analog knob depth. See `current/RONGYUAN_BATCH_2_2026-09-24.md` for admission evidence and exclusions.

## RongYuan stream expansion: third batch (2026-09-24)

Experimental wired USB support, enabled detection/analog/bindings/gamepad with yellow notices. Exact magnetic revisions only; no wireless or physical-test claim. Factory maps and identities are source-pinned. Full-scale normalization remains provisional 4 mm; switch-specific endpoints need confirmation.

| Model | Board revisions |
|---|---|
| Blackstorm Renegade HE | 3355 |
| EWEADN DEEP68 HE | 2578, 2710, 2711, 2955 |
| EWEADN DEEP80 HE (magnetic version) | 2574, 2652, 2653 |
| EWEADN DEEP80 Pro HE (magnetic version) | 2906 |
| EWEADN SEEK75 | 2799 |
| EWEADN ZAP68 HE | 2348, 2426, 3035, 3036 |
| EWEADN ZAP68 Ultra HE | 2554, 2510 |
| EWEADN ZAP87 HE | 3011 |
| FREEWOLF F68 | 2634 |
| FREEWOLF F68 PRO | 2594 |
| GAMEBOOSTER RAPID HE | 3278 |
| Oniverse Maegnus | 3267 |
| Rampage KAISEL | 3114 |
| Rampage ZENITH PRO | 2310 |
| Valkyrie VK Mag68 | 2320, 3390 |
| Valkyrie VK Mag68 Max | 3194 |
| Valkyrie VK Mag75 | 2246 |
| Valkyrie VK Mag75 Lite | 2234 |
| Valkyrie VK Mag75 Max | 2398 |
| Valkyrie VK Mag75 Pro | 2227 |
| Valkyrie VK NB68 | 2560 |
| Valkyrie VK NB68 Max | 2757, 3111 |

Valkyrie MAG/NB and EWEADN variants are selected by exact board IDs, not shared VID/PID or retail names. DEEP80 Max, ZAP68 Pro and SMART875 are not implicitly admitted. Oniverse uses factory physical positions despite AZERTY legends. Details and deferred candidates: `current/RONGYUAN_BATCH_3_2026-09-24.md`.

## RongYuan stream expansion: fourth batch (2026-09-24)

Experimental wired USB support, full detection/analog/bindings/gamepad path, yellow notice, no physical test claimed. Exact factory maps and revisions only; manual layout selection.

| Model | Board revisions | Normalization um |
|---|---|---|
| ANTGAMER AGK75 PRO | 2651, 2425, 2328, 2629 | 4000 |
| ANTGAMER AGK75 U2 | 2281, 3064, 2642, 3005, 3512 | 4000 |
| ANTGAMER AGK87 | 2114, 2330 | 3400 |
| EDRA EK368RT | 2835 | 4000 |
| G TUNE GMK82 | 2912 | 3300 |
| IDEEZ SWIFT X85 | 2756, 3481 | 4000 |
| LinkerFoo LF67R1 | 3068 | 4000 |
| MICROPACK K-68M | 2546 | 4000 |
| ROYALAXE X68 | 2485 | 4000 |
| SARU KX69HE | 3166 | 3300 |
| SARU KX78HE | 3243 | 3400 |
| Titan Nation TITAN68HE | 2116 | 3400 |
| URX Core68 HE | 3513 | 3400 |
| Veekos Shine60 HE | 2836, 2832 | 4000 |

3300/3400 values follow vendor setting limits or stock switch specifications, not our measurement. 4000 remains provisional. G TUNE GMK82 uses its JIS factory matrix. ANTGAMER AGK75 PRO/U2 are not a claim for every unqualified AGK75; AGK87 requires magnetic switches. TITAN68HE does not cover a TITAN60 replacement case or MADLIONS TITAN68 Turbo. See `current/RONGYUAN_BATCH_4_2026-09-24.md`.


## Local RongYuan batch 5 — 2026-09-24

Seven additional experimental models, 13 exact revisions. Wired USB analog,
bindings and gamepad output enabled; physical hardware untested.

| Brand | Model | Exact revisions |
|---|---|---|
| BOYI | H60 Pro | 2764, 2875 |
| MageGee | AIR68 | 3146 |
| MageGee | Captain87 JIS | 2789 |
| Sunsonny | N-J100 | 2780, 3294 |
| Royal Kludge | A72HE | 3332, 3333 |
| XINMENG | Zero 68 | 3611, 3587, 3598, 3609 |
| COLORFUL | QY98 Ultra | 3257 |

Captain87 scope is JIS only. BOYI includes ANSI/UK; XINMENG includes the four
listed wired/tri-mode revisions via USB. QY98 Ultra does not include mechanical
QY98/Pro. A72HE has two independently mapped revisions; its launcher/knob
controls are not depth keys. BOYI and Captain87 normalize to3300um from vendor
settings; other ranges provisionally4000um. Full evidence and tests:
[current checkpoint](current/RONGYUAN_BATCH_5_2026-09-24.md).


## Local reviewed RongYuan batch

Wired USB; experimental, no hardware-test claim.

| Brand | Model | Board | Evidence and normalization |
|---|---|---|---|
| ATWO | GK7 MX | 3475 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; OEM travel setting max3.3mm |
| HAVIT | KB900L | 3570 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; OEM travel setting max3.3mm |
| HAVIT | KB904L | 3588 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; OEM travel setting max3.3mm |
| UluGames | Howl 75 | 3437 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| UluGames | Howl 75 | 3403 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| GamePro | MK160B MAX | 2930 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| LOMZ | 75S | 2737 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| LOMZ | 75S | 2828 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| M4G | MAG 68 HE | 3017 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| Fuego | GKB904 | 3439 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; OEM travel setting max3.3mm |
| XINMENG | Beat65 | 2326 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| XINMENG | Beat65 | 2436 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| XINMENG | Beat65 | 2535 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| XINMENG | Beat68 | 2589 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| XINMENG | Beat68 | 2590 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| XINMENG | Beat68 | 2680 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| XINMENG | Beat75 | 2770 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |
| XINMENG | Beat75 | 2797 | Exact OEM magnetic record, loader and pinned factory map; wired USB; see RONGYUAN_BATCH_6_2026-09-24.md; Provisional4mm normalization; switch-specific full travel not measured |


## Local reviewed RongYuan batch

Wired USB; experimental, no hardware-test claim.

| Brand | Model | Board | Evidence and normalization |
|---|---|---|---|
| MechLands | M75 | 2496 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| Womier | SK61 HE | 3708 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Official SK61 HE actuation maximum3.3mm |
| Nova Gaming | GK505 Eon | 3302 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| EPOMAKER | G84 HE JIS | 3703 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| EPOMAKER | HE60 Lite | 3727 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| EPOMAKER | HE60 Lite | 3759 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| EPOMAKER | HE60 Wired | 3746 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3400um |
| MonsGeek | M2 V5 HE | 2601 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| MonsGeek | M3 V5 HE | 2585 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| CHERRY XTRFY | K5 Pro TMR Compact | 2626 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3500um |
| EWEADN | V99 (magnetic version) | 2279 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| EWEADN | SMART 875 HE | 2637 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| EWEADN | ZAP87 HE | 2527 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| EWEADN | ZAP68 SE | 3225 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| FL ESPORTS | GP75 HE | 2869 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| FL ESPORTS | GP87 HE | 3007 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| FL ESPORTS | GP87 HE | 3539 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| FL ESPORTS | D75 HE | 3542 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| FL ESPORTS | D98 HE | 3556 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| FL ESPORTS | Flame65S | 3547 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| FL ESPORTS | FL750 (magnetic version) | 3003 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| FL ESPORTS | FL750 (magnetic version) | 3524 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| GAMEPOWER | Nexa HE60 1K | 2406 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| JEDEL | KL166 | 2729 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| SALPIDO | SHOT209 | 2932 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| MageGee | MK-BOX (magnetic version) | 2311 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| HAWK Gaming | HK550 | 3760 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| HAWK Gaming | HK610S | 3677 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| DARKFORCE | Fib(68) | 3055 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| IDJ | H60HE | 3078 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| IDJ | H60HE | 3079 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| ANGRYSHARK | Final 75 | 3089 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| Koda | A68 | 3244 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| Funbey | AST V68 | 3292 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| Funbey | Coke V68 | 3291 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| E7 | 68 PRO V2 | 3383 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| XINMENG | X87 TMR | 3486 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| XINMENG | X98 V3 (magnetic version) | 3277 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| JINGSU | KA67 | 3106 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| JINGSU | KB98 | 3520 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| JINGSU | KCC04A | 3193 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| JINGSU | KE87 | 3711 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max 3300um |
| Ninjadog | Varna Atlas | 3076 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| PSYCommu | PSY P1 | 3425 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| NOS | C800 ALU (UK) | 3663 | Exact OEM magnetic identity, loader and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; no physical measurement |
| Fury | Kanabo K6 | 2763 | Exact OEM magnetic record and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Official full travel4mm |
| Titan Nation | TITAN60 PCB | 2220 | Exact OEM magnetic record and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max3.4mm; factory map has60 keyboard positions |
| Titan Nation | Storm68 | 2816 | Exact OEM magnetic record and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; OEM travel.max3.4mm |
| Valkyrie | VK 99 Gaming (Naruto) | 2831 | Exact OEM magnetic record and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm normalization; exact VK99 Gaming revision only |
| ASTROMEDA | AMGK80-001 | 3389 | Exact OEM magnetic record and factory map; wired USB; RONGYUAN_BATCH_7_2026-09-24.md; Provisional4mm; MiningBase company matches ASTROMEDA ownership |


## EWEADN SparkLink expansion (local, 2026-09-24)

Wired USB only; native SparkLink V2 live layout/travel queries, ordinary HallJoy bindings and virtual gamepad output. Experimental, not physically tested. Source: official E HUB 3.3.2; pinned catalog/SDK in `research/eweadn-sparklink/`. Device-info type1 is keyboard; the magnetic subtype is a separate byte. Factory configuration is not modified. Current base-layer key codes are read from the device.

| Model | PID (VID 1CA6) |
| --- | --- |
| EWEADN X87HE | 1C0A |
| EWEADN DEEP68 Pro HE | 1C14 |
| EWEADN DK68 HE | 1C12, 1C1F |
| EWEADN DK68 Star HE | 1C2B |
| EWEADN DK63 Star HE | 1C2B |
| EWEADN DK68 V2 HE | 1C3C, 1C1A |
| EWEADN DK80 HE | 1C3D |
| EWEADN DK68 Pro HE | 1C23, 1C2C |
| EWEADN ES68 | 1C24 |
| EWEADN ES68 EVO | 1C2F, 1C4C |
| EWEADN ES68 Lite | 1C4A |
| EWEADN DK63 HE | 5E01 |
| EWEADN DK75 Pro HE | 2709 |
| EWEADN DK75 E HE | 2708 |
| EWEADN DK75 HE | 270A |
| EWEADN Gamma75 HE (EXX collaboration) | 1C37, 1C45 |
| EWEADN DEEP80 Max HE (magnetic version) | 1C0C |

Travel is micrometres. The existing per-key 3.5mm initial / observed-upward-to-5mm normalization remains provisional for switch variants. Manual visual layout selection may be needed. DK63 Star and DK68 Star share a PID: runtime layout comes from the device, not a hardcoded model matrix. ES68 EVO includes the Full revision. Gamma75 covers only 1C37/1C45 revisions; Alpha87, PID1C2D X75/Gamma75 and shared ES68 boot identity remain outside this declaration. No blanket support for mechanical namesakes, other controller families or wireless transport.


## Local reviewed RongYuan batch

Wired USB; experimental, no hardware-test claim.

| Brand | Model | Board | Evidence and normalization |
|---|---|---|---|
| Game Arena | GKX68 MAGNUM | 2628 | Exact GKX68 MAGNUM magnetic OEM catalog, reviewed Common stream parent and complete factory matrix; Game Arena own-brand GKX68 MAGNUM product page. See NEXT_PROTOCOL_REVIEW_2026-09-24.md.; Provisional 4mm normalization; switch endpoint not physically measured. |
| Game Arena | GKX68 MAGNUM | 2790 | Exact GKX68 MAGNUM magnetic OEM catalog, reviewed Common stream parent and complete factory matrix; Game Arena own-brand GKX68 MAGNUM product page. See NEXT_PROTOCOL_REVIEW_2026-09-24.md.; Provisional 4mm normalization; switch endpoint not physically measured. |


## JingTai V1 expansion (local, 2026-09-24)

IROK NA87 Pro, ND63, Mercury68 / Mercury68 Pro (CAROTMAS Mer68 names), and IYX MU68 Pro use the enabled native V1 path. Exact VID/PID plus product strings, pinned per-key factory maps, base-layer assignment reads and independent half-matrix travel. Cyan/TTC names explicitly listed in the official client are included for ND63/MU68 Pro; Ultra, Polar75, MG75 and unrelated namesakes are excluded. Source audit rejects ambiguous maps.

Experimental wired USB; firmware-derived4mm normalization for ordinary NA87 Pro/MU68 Pro/ND63,3.6mm for identified Cyan/TTC variants; Mercury68/Pro provisionally3.5mm. Newer firmware and physical switch endpoints remain unverified. Exact automatic ANSI presets now cover these five models and the identified Cyan/TTC variants; see current/LAYOUT_EASY_BATCH_2026-09-24.md for the accompanying startup fix. No calibration or configuration writes. See current/JINGTAI_V1_EXPANSION_2026-09-24.md for evidence and validation.


### Local USB identity correction (2026-09-24)

Akko MOD007S V3 HE ANSI and ISO/UK additionally admit the alternate USB identity
found in the independent official MonsGeek client. Exact board proof, original
factory maps and yellow status retained. Existing USB variants remain supported.
See current/USB_IDENTITY_AUDIT_2026-09-24.md for source and held candidates.

Valkyrie VK Mag75 Max also admits an exact alternate USB identity, supported by
the matching official factory matrix and archived v306 analog-stream firmware.
Packet-writer component replay passed; physical validation remains outstanding.
Its experimental status and existing identity are retained.
