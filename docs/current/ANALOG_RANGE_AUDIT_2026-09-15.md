# Analog range audit — 2026-09-15

Read-only source audit prompted by the owner asking where HallJoy estimates
normalization bounds. No code or EXE changed. Findings are source behavior,
not physical validation of scales on every firmware.

Confirmed observation-derived native normalization:

- HERO84: aula_hero84he_backend.cpp Publish learns per-position observed high/low;
  a span greater than 32 enables output. A shallow first excursion may become
  100%. Range expands as new extrema arrive. Enabled with amber warning.
- Generic non-IPI Addressed fallback: addressed_analog_backend.cpp Normalise uses
  first raw value as released, seeds bottom to released minus 8400 (or 500), then
  expands observed endpoints; accepts raw values between 500 and 20000. The exact
  eight IPI UUID profiles take the separate stored-calibration branch and do not
  use this heuristic. Do not attribute the fallback to all IPI/Addressed models.
- SparkLink/XD: backend_sparklink.inc SparkNormalizeRouteToMilli divides by an
  observed per-HID maximum clamped to 3000..5000; larger observations up to 5000
  update the denominator. This is bounded adaptation, not HERO84's free extrema.
  Precise affected model names depend on the actual native routing; the protocol
  family must not be expanded to a whole brand by inference.

Other inspected routes differ:
- IPI exact profiles read per-key calibration endpoints through the existing
  calibration-data read (not calibration entry).
- W669 and Hex80 decode the maximum from device travel-info responses. The W669
  status wording 'adaptive stream' does not establish adaptive range estimation.
- NA87 uses the protocol travel-byte conversion through TryTravelToMilli, with
  fixed nominal maximum 40; no observed extrema in this conversion.
- Sayo native has fixed raw full-scale 4000. Inspected UAP AnalogueKeyboard.cpp
  routes use protocol/model divisors, including Keychron 235. Fixed constants
  are distinct from learned bounds; this audit does not prove each constant
  correct for every hardware/firmware revision.

Scope inspected: enabled native backend normalizers, backend_sayo.inc,
backend_sparklink.inc, backend input conversion, and the local bundled UAP overlay
AnalogueKeyboard.cpp. This is not a complete firmware-by-firmware scale audit.
