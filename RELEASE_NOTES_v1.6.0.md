# HallJoy 1.6.0

Released on 2026-09-21.
Includes IROK NA87 and AULA MINI 60 HE Pro support introduced in 1.5.3.

## Keyboard support and layouts

- Ordinary ATTACK SHARK X65 Pro support with automatic layout and factory Fn
  handling; its testing notice is removed. Analogue input and gameplay are
  tester-confirmed. Other compatible ATTACK SHARK models remain experimental.
- Experimental IROK MG75 Pro native support, independent key depths including Fn,
  automatic layout and an orange Discord testing notice. Hardware validation is pending.
- Improved HERO84 automatic layout, key maps and physical Fn handling; support
  remains experimental. GravaStar V75 / Pro / Lite legacy revisions gain exact
  automatic selection and 79-key layouts with session remaps.
- Wooting 60HE v2, Split and UwU layouts. Split variants keep separate physical
  Space/Fn channels; their hardware validation remains pending.
- SayoDevice O3C always shows three physical keys automatically, using device
  bindings when readable and Key 1/2/3 otherwise. Manual layout uses Z/X/C.
- Additional Razer ANSI/JIS/ISO variants and Redragon K673 BR ABNT2 geometry.
  The catalog contains 121 source variants and 98 visible variants; identical
  layouts are grouped within their brand, preserving saved aliases and edits.

## Fixes

- Corrected hidden Configuration controls reappearing after layout changes.
- Fixed Pause / failed-Resume cleanup resetting shared input state before the
  realtime producer had stopped.
- Reused the UI tick's analogue telemetry snapshot instead of collecting it again
  for Configuration / Gamepad Tester diagnostic hashing.
- Removed raw stack/register memory from ordinary crash reports. Optional logging
  remains separate from mandatory crash, missing-keyboard and failure reports.

## Runtime and logging

- The bundled analogue DLL now lives in AppData instead of beside the EXE.
- With **Enable logging** on, HallJoy.log is saved in AppData and beside the EXE.
  If one destination is unavailable, the other continues independently.
- With logging off, bounded in-memory failure history remains; automatic incident
  and crash reports use the app data folder. Portable user-data storage is unchanged.

## Known limits

ATTACK SHARK uses factory assignments; custom remaps/macros and wireless receivers
are not enabled. Normalization still has provisional and switch-bank limits.
Broader family investigation remains deferred. One tester reported input-mode
switching in Forza/Roblox, then confirmed normal operation without a HallJoy
change. The cause is unknown; no specific fix is claimed.

MG75 Pro, HERO84, GravaStar and new O3C/Wooting paths retain the validation limits
in the [hardware table](SUPPORTED_HARDWARE.md). NA87 Pro, ND75 and ROG Azoth 96 HE
remain disabled. A visual preset does not prove support for every device revision.

See [release readiness](docs/current/RELEASE_1.6.0_READINESS_2026-09-20.md) for the
exact artifact and automated checks.
