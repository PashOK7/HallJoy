# RM-30-HERO84 — AULA HERO84 HE frozen-route protocol review

Date: 2026-09-06

## Scope

This review verifies the already approved freeze rather than widening support.
The normal production catalog excludes HERO84. The retained native candidate
can appear only in the separately named `HallJoyAulaHero84HeExperimental` test
image; the separate diagnostic target has no analogue-input ownership.

## Exact contract retained for a future consenting tester

- Admission is exact: `372E:103E`, `FF60:0061`, 64-byte report ID `09`, and
  the checked `82/01` HERO84 UUID response. VID/PID alone is insufficient.
- The only candidate data reads are identity `82/01`, assignment map `83`, and
  the firmware-reversed `94/02` direct current/minimum record path. Direct
  `94/02` is distinct from calibration entry/exit and reads the normal scan
  state without entering calibration.
- The static firmware evidence rejects `94/00`, `94/03`, `94/04`, `94/05`,
  `98`, feature reports, flashing and configuration operations. The diagnostic
  is single-shot, waits for matching raw-input registration before active
  traffic, and records no input ownership.
- Ordinary typing and vendor replies use separate firmware paths. This supports
  the research hypothesis that analogue and letters can coexist, but it is not
  physical proof of a usable production route.

## Result

The compile-time/catalog boundary, exact endpoint, allow-list and diagnostic
non-ownership rules are internally consistent with D-084 and the firmware
reconstruction. No enabled behavior was changed; no diagnostic or ordinary
executable was built or run. A real owner's input, release, coexistence,
hotplug and log evidence remain required before any re-enable decision.
