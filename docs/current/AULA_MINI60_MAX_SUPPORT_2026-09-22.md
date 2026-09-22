# AULA MINI 60 HE MAX — native wired integration

## Scope and evidence

Owner requested full model support. Implemented existing native analog features for wired USB only; receiver and Bluetooth support are not established and are not claimed. Physical hardware was not available to this agent; lack of per-model hardware testing alone is not a support gate under owner policy. No firmware was flashed, and no hardware probe was sent.

Firmware acquisition, hashes and first execution evidence: [catalog audit](AULA_CATALOG_GAPS_2026-09-21.md). Official MAX V1.52 firmware resource SHA256 06a1a47432a6ca7ec060103378e2d984a46f9afa98f19fccd99219eaea4a88e0. Source registry and common HFD layout select the same 61-key physical map as PRO.

Additional review: MAX device-info stored header at 0x9000 has VID/PID 0C45:80A1, version 0152, manufacturer 0166. Command 10 at 0xf07e overwrites product/work-mode reply bytes from MCU register 0x00400180; no arbitrary fixed MAX product value was invented. Admission requires the exact supported descriptor PID, matching reply VID/PID, manufacturer 0166, FF68:0061 and 65-byte input/output reports. PRO retains its established product 110c check. Alt PRO revision 80B2 and receivers FEFE/FEFC remain excluded from commands.

Actual MAX reporting tail uses the same LE16 travel/stroke units and 34 stroke constant; exact dispatcher/reporting checks PASS in previous audit. Additional Unicorn execution with simulation enabled and calibration disabled reaches ordinary key-processing entry 0x5d28 from 0x37d4. This proves that branch remains reachable; it is not a whole-device or timing test. Native stale-release behavior remains 50 ms, matching PRO; no zero-latency claim.

## Implementation

- Whitelist exact MAX wired identity alongside PRO, retaining descriptor and device-info checks.
- Carry verified product through worker shared memory and publish model-specific telemetry.
- Separate MAX layout token and manual ANSI preset, automatic selection, assigned-key mapping and cleanup. Existing backend stable identifier remains unchanged for compatibility.
- Reuse existing independent depth normalization, duplicate-assignment aggregation, bindings/curves, gamepad output, pause/close cleanup, containment and per-key expiry.
- Extend existing linked native self-check through both products, including identities, telemetry/token, remapping, gamepad construction, expiry and reconnect-style model switch. No new diagnostic build or forced logging.
- Same-family multiple simultaneous wired boards remain subject to existing one-device admission; this task does not implement concurrent MINI keyboards.

Backup: .local/backups/mini60-max-before-20260922-074927.zip.
Build must preserve HallJoyKeychronOnboardExperimental=true for owner's installed K4 r5; ordinary build without it was cancelled before delivery. Use the existing delivery path, not a second output folder.

## Completion

Release build and four linked-image gates PASS (MINI60 exercises PRO and MAX; ATTACK SHARK, NA87, embedded ViGEm resource). Encoding audit PASS. Delivered build/bin/Release/x64/HallJoy.exe SHA256 592fda0cdbcc6dfe13208d342d3094ff1e73e2f015b90b4e32e86f6f64acc4cb. K4 onboard compile flag preserved. No forced diagnostic logging. Existing third-party ViGEm PDB warning only; no build failure. No GitHub publication.

Sheet Main C123 changed Known protocol; integration required -> Supported. Readback A122:C124 verified exact MAX model, strict validation, green RGB (0.65882355,0.8666667,0.70980394), unchanged neighboring MINI base/PRO statuses, zero notes. Supported scope is wired USB as documented in README and SUPPORTED_HARDWARE. Current known protocol integration is complete for that transport; wireless remains unsupported. Agent performed no GUI/hardware test.

Factory mapping cross-check: all plain-key assignments at MAX flash 0x9600 match the common physical map (61 keys). Fn at position85 uses record 02 00 AF 00; native Assigned now converts that vendor action to internal Fn 0x409, including remapped Fn, while rejecting modified/multi-action combinations. Existing linked self-check covers this case for both admitted products.
