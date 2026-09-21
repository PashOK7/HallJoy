# HallJoy hardware compatibility

Current source / ordinary **1.6.0 release**, reconciled on 2026-09-21.
See [release notes](RELEASE_NOTES_v1.6.0.md).

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
| Aula | **WIN 60 HE MAX**, **WIN 60 HE**, **WIN 68 HE**, **KP-TE153**, **MINI 60 HE Pro** | MINI 60 HE Pro: wired USB support added in 1.5.3; tester logs confirm independent key depths. The 2.4 GHz receiver is not supported. |
| ATTACK SHARK | **X65 Pro** | Tester-confirmed analogue input and gameplay, including external gamepad output with Block Bound Keys enabled. Ordinary support without a testing notice. |
| Irok | **MG75 Max**, **MG75 Pro (experimental)**, **NA87** | NA87 support added in 1.5.3, with tester feedback and logs. **NA87 Pro is a different model and is not supported.** MG75 Pro uses native SparkLink V1 travel reads with automatic layout and base-layer remaps; hardware testing is pending and an amber Discord testing notice is shown. |
| Redragon | **K673RGB-M**, **K673WB-RGB-M** | Exact W669 profiles exist for K673RGB-M BR/UK and K673WB-RGB-M US. BR has physical evidence; UK/US have source-profile and automated evidence. Other compatible magnetic models may work but are untested. |
| MADLIONS | **MAD 68 Pro R**, **MAD60HE**, **MAD68HE**, **MAD68R** | — |
| ATK | **Hex80** | — |
| SayoDevice | **O3C** | O3C depth is tested. Automatic layout always has three independent keys, labelled from device bindings or Key 1/2/3 if unavailable. Manual layout uses Z/X/C. New binding reads await device validation. Other SayoDevice models are unconfirmed. |
| IPI / QBZ | **QBZ75**, **Aurora 75**, **Aurora75 PRO**, **QBZ65**, **AURORA65**, **AURORA65W**, **RAIN65**, **flash68** | QBZ75 is supported with existing physical evidence for its addressed route. Other exact-model profiles and layouts are implemented but await individual hardware testing. AURORA65W receiver support is unverified. Plus variants are not included. |
| Razer | **Huntsman V2 Analog**, **Huntsman Mini Analog**, **Huntsman V3 Pro**, **Huntsman V3 Pro Mini**, **Huntsman V3 Pro Tenkeyless** | — |
| Keychron | **Q1 HE**, **Q3 HE**, **Q5 HE**, **Q6 HE**, **Q12 HE**, **Q1 HE 8K**, **Q3 HE 8K**, **Q5 HE 8K**, **Q6 HE 8K**, **K2 HE**, **K3 HE**, **K4 HE**, **K8 HE**, **K10 HE** | Requires [custom firmware](https://analogsense.org/firmware/). Ready-made images are available for some models. For other Keychron HE models, the linked source changes can be applied to Keychron's published firmware source and built for the exact model and ANSI/ISO/JIS variant. |
| Lemokey | **P1 HE ANSI**, **P1 HE ISO** | — |
| NuPhy | **Air60 HE**, **Air75 HE** | — |
| DrunkDeer | **A75**, **A75 Pro**, **G60**, **G65**, **G75** | — |
| Wooting | **60HE**, **60HE+**, **60HE v2**, **80HE**, **One**, **Two**, **Two HE**, **UwU / UwU RGB** | Bundled analogue runtime. New v2 / Split / UwU layouts are in the 1.6.0 development build; see below. |

## Additional routes and validation evidence

| Route | Current behavior and evidence | Remaining limits |
|---|---|---|
| ATTACK SHARK RY5088 | 37 exact reviewed revisions enabled over wired USB; 16 automatic ANSI layout groups. X65 Pro tester reported independent depth and gameplay. | Other revisions need hardware testing. Factory/Fn maps only; custom remaps and receivers are not enabled. Provisional normalization and per-bank limits remain unresolved. X65 Pro has ordinary support; other admitted models retain an orange testing notice. |
| IROK MG75 Pro | Separate native SparkLink V1 route, independent travel for 81 keys including Fn, automatic layout/base assignments, vendor 3.5 mm scale. Enabled with orange notice. | No physical tester result yet; not the MG75 Max V2 protocol. |
| AULA HERO84 HE | Enabled with orange notice; exact identity, automatic ANSI layout, live/base maps and physical Fn. | Frozen pending tester evidence; observed-range scaling still requires validation. |
| GravaStar Mercury V75 / Pro / Lite | Legacy identities 1CA5:2201, 1CA5:2202, 1CA2:2201 plus board proof; 79-key geometry and session remaps. Owner confirms a user reported working support (2026-09-20). | The new confirmation does not identify every tested revision; do not extend it to all Pro/Lite/newer revisions. Earlier source/firmware evidence is retained. |

ATTACK SHARK X65 Pro ordinary support was approved on 2026-09-21. The isolated
Forza/Roblox input-switching report is no longer reproducing; no specific fix or
root cause is claimed. Broader family investigation remains deferred; model
support does not establish hardware testing of every revision or switch bank.

## Validation details and authoritative implementation records

- [NA87 and AULA MINI 60 HE Pro](docs/current/NA87_MINI60_STANDARD_SUPPORT_2026-09-19.md):
  ordinary wired support with tester logs. NA87 Pro is a different model.
- AULA WIN 60 HE MAX and IROK MG75 Max have earlier physical test evidence.
  Redragon K673RGB-M evidence applies to the exact BR firmware, not every
  Redragon magnetic keyboard. Detailed original test records are preserved in
  the [historical hardware snapshot](docs/archive/HARDWARE_STATUS_BEFORE_2026-09-20.md).
- [ATK Hex80](docs/current/ATK_HEX80_NATIVE_FIXES_2026-09-14.md): corrected matrix
  slots, 87 factory keys including Fn, 32/128-byte payload handling and freshness.
- [IPI / QBZ](docs/current/IPI_NATIVE_SUPPORT_2026-09-14.md): eight exact UUID
  profiles, complete maps, device calibration and Fn. Existing physical evidence
  covers the QBZ75-compatible route; other models await individual hardware testing. AURORA65W receiver forwarding is unverified.
- [MG75 Pro](docs/current/MG75_PRO_INTEGRATION_2026-09-19.md),
  [HERO84](docs/current/HERO84_INTEGRATION_2026-09-19.md), and
  [GravaStar](docs/current/GRAVASTAR_LAYOUTS_2026-09-19.md): use these current
  integration records instead of earlier disabled/manual-only descriptions.
- [ATTACK SHARK exact revisions](docs/current/ATTACK_SHARK_FAMILY_SUPPORT_2026-09-20.md),
  [all automatic layouts](docs/current/ATTACK_SHARK_FAMILY_LAYOUTS_2026-09-20.md),
  and [remaining scale questions](docs/current/ATTACK_SHARK_BANK_INIT_2026-09-20.md).
- [SayoDevice O3C](docs/current/SAYO_O3C_CONFIG_2026-09-19.md): depth is tested;
  new config readback awaits hardware validation. Automatic mode always keeps
  three physical keys, with real labels when readable or Key 1/2/3 otherwise.
  Manual mode uses factory Z/X/C.
- [Keychron catalog](docs/current/KEYCHRON_HE_LAYOUT_CATALOG.md): custom firmware
  is required for the discussed HE routes; do not promise stock-firmware support.
  Ready-made images cover some models; other exact variants require applying
  the published source changes and building matching firmware. Layout additions
  do not require repeating this already established protocol investigation.
- [Wooting split layouts](docs/current/WOOTING_EXTENDED_LAYOUTS_2026-09-19.md):
  independent physical Space/Fn channels implemented; split hardware validation
  remains pending. Region/split selection is manual.

## Disabled research

IROK NA87 Pro, IROK ND75 and ROG Azoth 96 HE are not enabled in ordinary builds.
Their retained research and device notices are not working support claims.

See the [complete layout catalog](docs/KEYBOARD_LAYOUTS.md) for exact regional
variants and the [support report guide](docs/SUPPORT_REPORT.md) for reporting a
specific device result. Historical audits are evidence at their recorded date;
later integration records and owner decisions supersede their status statements.
