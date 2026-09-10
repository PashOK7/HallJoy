# RM-29 — AULA HERO84 HE release-scope freeze

Date: 2026-09-06

## Owner decision

The only available owner stopped responding before a real-keyboard test. Keep
the implementation for future evidence work, but disable it now. When another
owner volunteers, create the separately named experimental test executable,
collect its log and physical behavior evidence, then decide whether release
support is appropriate.

## Implemented boundary

- Ordinary `HallJoy` builds neither compile the HERO84 production module nor
  place its descriptor in the native backend catalog.
- The retained route needs the explicit MSBuild property
  `HallJoyAulaHero84HeExperimental=true`. That property alone selects the
  separately named `HallJoy-AULA-HERO84HE-Experimental` target and provides its
  trace instrumentation.
- The pre-existing diagnostic-only target remains separate and read-only. It
  still cannot own or transform gameplay input.
- The support manifest labels the device frozen, not supported or merely
  temporarily absent.

## Required evidence before reconsidering release scope

1. A consenting owner runs only the explicitly named test image.
2. Review the produced trace/log for exact identity admission and transport
   behavior; do not infer success from a matching VID/PID.
3. Confirm analogue and ordinary letters arrive together, release becomes zero,
   reconnect is safe, and stop leaves no retained output.
4. Run the applicable parser/session/XUSB and physical gates, then obtain a new
   explicit release-scope decision. No firmware, calibration, map, lighting or
   profile writes are permitted for this evidence collection.

## Verification

`src/HallJoyProject/tests/aula_hero84he_backend_static_audit.py` verifies the
opt-in catalog guard, explicit test target and normal-build exclusion. It is a
source proof only; no executable was built or run for this card.
