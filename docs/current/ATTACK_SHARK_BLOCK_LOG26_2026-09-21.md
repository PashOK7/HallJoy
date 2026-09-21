> Later owner-supplied screenshots (2026-09-21) resolve the requested symptom: rapid keyboard/controller switching in Forza and Roblox. Tester confirms both internal and browser gamepad testers work, no other controller attached, Forza not elevated; subsequently reports gameplay now works without knowing what changed. No root cause or software fix established. Owner approved ordinary X65 Pro support independently of this no-longer-reproducing report. See [prerelease record](PRERELEASE_2026-09-21.md). The original log analysis below remains historical evidence.

# ATTACK SHARK Block Bound Keys: tester log 26

## Evidence and scope

Received 2026-09-21. Source: owner's Downloads/HallJoy (26).log; immutable local
copy: `.local/research/attackshark-block-log26/HallJoy-26.log`.
SHA256: `4cd3d4ce6874bee0b6cb99a270cfaca7314f92aa78074f2238098cca8ef1f145`.
100843 bytes, session UTC 2026-09-21T13:25:18Z, duration 94.235 seconds.
Log identifies HallJoy 1.5.4.0, input-path-20260919-r1. The actual EXE hash is not
present, so this is a build-marker identification, not binary hash verification.
Native identity 2308, VID/PID 3151:502F: documented X65 Pro revision.

The owner associates this log with the still-open bound-key blocking bug.
Exact observed symptom/game and incident moment were requested; not supplied yet.
The earlier generic pause awaiting tester does not prevent analysis of this newly
supplied evidence. No wider firmware/scaling investigation is resumed here.

## Observed path

Mode snapshots (seconds relative to session.begin; approximately one-second sampling):

- 0.000: Block OFF; engine not yet running.
- 1.032: engine running.
- 10.063: Block ON; own foreground.
- 40.079: external foreground, Block ON.
- 63.110: Block OFF, external foreground.
- 78.125: Block ON, own foreground; external by 79.125.
- 94.141: engine stopped; session.end at 94.235.

Final aggregate counter banks:

| Observation | Block OFF | Block ON |
|---|---:|---:|
| Source-positive frames | 1353 | 4017 |
| Raw-positive configured reads | 4587 | 14700 |
| Filtered-positive configured reads | 4538 | 14494 |
| Active built controller frames | 4243 | 13402 |
| Accepted publications | 198 | 910 |
| Rejected publications | 0 | 0 |
| Bound hook events passed | 35 | 0 |
| Bound hook events suppressed | 0 | 113 |
| Fn unavailable frames | 20 | 1 |

These are aggregate observations, not unique physical presses or one-to-one USB
transactions. Active frames can also include mouse bindings. The raw/filtered
keyboard counters independently demonstrate nonzero configured analog values.

During both external-foreground Block ON activity intervals (~47-62s and ~80-92s),
source-positive/active/publication counters increase. The child applied sequence
advances concurrently. Ready output stays in generation 1 with zero errors,
restarts/rebuilds and no sampled applied-vs-published gap greater than one.
Final running applied/published sequence: 1109/1109. Native I/O errors: zero;
last capture 93078 ms; maximum exchange 168580 us; expired-page counter 26 overall.
These coarse snapshots do not exclude short stalls or identify the user's exact
incident. Shutdown state zero is expected after the engine is stopped.

## Interpretation and limits

The trace does not support a sustained loss of native analog or virtual-controller
publication caused by enabling Block Bound Keys. The low-level hook records 113
suppressed bound events, none passed in the ON bank. This proves HallJoy's hook
chose suppression for those observed events, not that every game input path was
blocked. Applied publication proves transport acceptance, not consumption by a game.
Foreground privacy is own/external only; this log does not identify the game or
record the inputs it actually consumed. Do not call the reported bug fixed.

`analog_error=-1996` is the retained SDK InvalidArgument diagnostic field, distinct
from the native ATTACK SHARK failure counters. It coexists here with successful
native samples and nonzero configured analog; it is not proof of a stopped native
stream or an established explanation for the block symptom.

Source checks: app.cpp KeyboardBlockHookProc increments BoundBlocked immediately
before returning suppression, with backend notification before filtering;
backend.cpp records configured/raw/filtered and controller publication stages;
vigem_output_process_host.cpp acknowledges applied sequence after transport.Apply.
The historical diagnostic contract remains INPUT_PATH_DIAGNOSTIC_2026-09-19.md.

No production code, executable, settings or public support status changed. No
visual/device/game test was run. A runtime fix without the exact observed symptom
would currently be speculative. Next step: obtain symptom/game/incident timing,
then investigate the implicated boundary (hook delivery, game input mode, specific
bindings, or a short timing interruption) rather than assume firmware depth failed.
