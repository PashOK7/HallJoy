# MIDI M2 touch estimator, version 1

[Fast-strike velocity investigation and logging](MIDI_VELOCITY_DIAGNOSTICS_2026-11-15.md).

Implementation contract for the owner's requested M2 stage, 2026-11-15.
Software verification and musical measurement have separate receipts below.

## Observations and units

Depth is normalized keyboard travel in permille (0..1000). Timestamps are captured
host microseconds, with the source's existing publication or snapshot provenance.
Speed is permille of full travel per second. Velocity is MIDI 1 integer 1..127.

The estimator uses the first significantly rising sample of a continuous stroke
and the sample crossing the attack threshold. Released dwell before that first
moving sample is excluded. A downward reversal exceeding the configured depth
noise starts a new stroke. Changes smaller than the noise amount accumulate
against the last accepted depth. A gap over 50 ms discards the old motion anchor
when another sample arrives. This is an uncertainty boundary, with no timer or
wait before emitting a note. A sounding note lasts until the separate release
threshold, source loss or explicit reset. Release must be below attack.

Two rising observations with positive elapsed time produce a measured secant.
A single rising observation uses the last timestamped accepted depth as a
lower-confidence interval, which can include unobserved dwell. Missing or equal
timestamps use one microsecond as a quantization bound and are labelled timing
unresolved. Their velocity is a bound derived from the observed depth change,
not a measured strike speed. Such attacks cannot train the optional sensitivity
setup. Measured snapshot strokes are distinguished from publication motion in
the quality display; sparse/unresolved timing takes precedence over that label.
The normalized 1/1000 representation does not certify sensor resolution.

## User controls

Light, balanced and firm use full-velocity references of 15, 35 and 65 full
travels per second. Full velocity can also be set explicitly. Use last speed
matches the reference to a captured two-observation stroke. No calibration
sequence is required. A monotone five-knot velocity curve maps normalized speed
to velocity, with independently editable 25%, 50% and 75% points. Endpoints
remain 0 and 100%. Defaults approximate the previous square-root response.

Trigger, release and noise are separate depth controls. Hysteresis supports
repeated notes after partial release when that release crosses the chosen
threshold. Pressure continues to represent depth. Profile version 1 saves these
controls, and legacy profiles supply their previous defaults. Invalid profiles
are rejected before live state changes.

## Delivered implementation and software evidence

Shared `midi_touch.h` is used by both snapshot and observation engines. Production
output publishes the latest touch report as a coherent bounded atomic snapshot;
no new timer, worker or input history is created by M2. Disabled MIDI retains M0's
fast paths. The MIDI card exposes release/noise/full-velocity speed and curve
knots. Selecting a named touch preset restores its reference and default curve.
Use last speed accepts a measured publication or snapshot stroke only, and saves
its speed as the full-velocity reference. Edits currently retain the existing
Panic/rearm behavior; sounding-voice edit refinement belongs to M5.

Portable trace tests pass: identical messages under different consumer batches,
90-second pre-motion dwell, 1..65 travel/s response, two source cadences, chords,
reversal, noise accumulation, partial release, sparse/equal/unknown times,
backward-clock rearm and monotone curve encoding. Windows profile tests pass
roundtrip/custom controls, legacy defaults and rejection of malformed versions,
thresholds and curves before state mutation. The production MIDI owner test uses
a fake driver and verifies measured velocity after an entire captured stroke
arrives before the consumer wake. Existing ownership, alias, history-loss,
output-independence, disabled-work and cleanup tests pass.

`tools/run_native_backend_checks.py --require-compiler` passed (normal cache).
`tools/build_release.ps1` passed: candidate full-catalog and embedded-resource
checks, 10-family tester admission, production-linked profile transactions, layout
aliases, 92 Supported layout entries and startup recovery (18 scenarios plus
repeated startup). The wrapper installed the verified candidate and restored the
previously running HallJoy. No additional delivery folder was introduced.

Ordinary portable EXE: `build/bin/Release/x64/HallJoy.exe`, version 1.6.15.0,
11895296 bytes. SHA-256:
`9f625fadde318cb65cdf794c0ef373ca10fea470c7652dfe9f6540790fd956df`.
Candidate and delivery hashes match. Exact source/log hashes and remaining gates
are recorded in local `result.json` alongside the two logs.
Tests are synthetic; no real instrument, DAW, receiver or physical touch measurement
was run. The owner evaluates visuals; the agent did not launch a visual UI test.

## Known limits and next stage

Host capture timing is not the sensor's own clock. Upstream coalescing, scarce
samples, aliased physical inputs or keyboard depth cutoffs cannot be recovered.
An unresolved one-step strike uses the documented one-microsecond bound and can
saturate at velocity 127; this is explicitly labelled unresolved, not certified
expressive response. A low-confidence interval may include unobserved dwell.
The last-stroke setup cannot remove these source limitations. Default noise and
sensitivity are starting presets, not measured per-keyboard calibration.

M2 software implementation is complete; physical musical response acceptance
remains open. Next engineering stage is M3: safe expression coalescing/queue age,
actual receiver evidence and usable DAW connection capability. WinMM output still
requires an explicitly chosen existing port; M2 creates no virtual MIDI endpoint.
Backups: `.local/backups/midi-m2-20261115/manifest.json` (hash-verified originals).
Evidence/logs: `.local/research/midi-m2-20261115/`. No GitHub release was published.
