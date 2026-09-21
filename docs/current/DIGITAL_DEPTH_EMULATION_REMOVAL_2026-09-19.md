# Removal of digital keyboard depth emulation

> Follow-up: [O3C config review](SAYO_O3C_CONFIG_2026-09-19.md) found residual digital-edge 1000/zero substitution in backend_sayo.inc outside this earlier removal. It is now removed along with digital binding inference; O3C publishes only validated travel depth. Earlier broad removal claims did not cover that path.

Owner explicitly reiterates that digital-to-analog keyboard emulation is prohibited
in production. This supersedes any historical compatibility/fallback description.

## Finding and provenance

DigitalFallbackInput was a hidden Main INI setting, false by default. No settings
control was found. It enabled ReadDigitalFallback01, using GetAsyncKeyState and
elapsed time to ramp up over approximately 100 ms and release over 80 ms. It
did not identify the physical slot of ATTACK SHARK analog values.

The available publication checkout history already contains this implementation
in abd0eeb (2026-09-10, preparation of HallJoy 1.5.0). This establishes presence,
not the original author/request or owner authorization. Do not invent provenance.
The new Forza report has no incident log; this finding does not prove that the
tester enabled or encountered fallback.

## Removal

- Removed generator, simulated key state and Windows digital query from backend.
- Removed runtime setting, API, INI read/write and profile validation requirement.
  Old DigitalFallbackInput keys are ignored, including invalid values.
- Removed digital source from shared native/provider arbitration, not just the
  caller's enable flag.
- Removed obsolete compatibility-mode warning and enable-related qualification
  counter. Historical qualification report fields remain zero; their historical
  rejection rule remains for compatibility, with no emulation implementation.
- Native/provider absence or stale data yields zero rather than invented depth.
- Isolated test simulator remains compile-time guarded by HALLJOY_ANALOG_SIMULATOR.
  The ordinary delivery is built without that switch.

## Verification

Added regression checks for absence of production generator/settings and digital
arbitration input; arbitration behavior test covers absent/stale sources returning
zero. Updated existing native-route audits from a per-key fallback guard to the
stronger no-emulation invariant. Added profile transaction fixtures that load
legacy DigitalFallbackInput=1 and invalid values without reviving the setting.

Source backup: .local/backups/before-digital-emulation-removal.zip.
Build/test evidence and final EXE hash are recorded below when complete.
No GitHub publication is authorized by this cleanup.


## Completed verification

- Ordinary MSVC Release PASS; existing ViGEm PDB warning only.
- Full static audit gate PASS (.local/no-digital-static-final.txt).
- Native/provider arbitration behavior test PASS.
- Exact delivery EXE: Shark/MINI60/NA87/embedded installer self-tests PASS,
  no generated logs in the isolated test directory.
- Profile transaction and startup recovery suite PASS, including the obsolete
  setting fixture, complete profile save/load, 100 concurrent transitions,
  failure injection and 16 recovery scenarios plus repeated startup.
  Evidence: .local/no-digital-profile-tests.txt.
- Release link map contains neither ReadDigitalFallback01/SimulatedKeyState/
  setting API symbols nor AnalogSimulator_ReadIsolated.

Delivery: build/bin/Release/x64/HallJoy.exe, version 1.5.4.0.
SHA256: a6372216ba9ab7fc584f6f95a0468f791636e4a91df0d099df3879616fc02554
Size: 9287680 bytes. Evidence: .local/no-digital-verification.json.
No physical hardware or game reproduction was performed; the tester's Forza
incident remains unproven. No GitHub publication.
