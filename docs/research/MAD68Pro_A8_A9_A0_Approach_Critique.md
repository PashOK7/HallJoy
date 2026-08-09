# Critical review of the MAD 68 Pro R `A9 -> A8 -> A9 -> A0` approach

This is the independent pre-resolution critique. It intentionally treats every
claim as unproven until firmware bytes and physical logs establish it. The
later evidence-based resolution is archived at
`../archive/historical-notes/MAD68PRO_R_NATIVE_V3_5_CRITIQUE_RESOLUTION.md`.

## 1. Apparent state-machine contradiction

The proposed sequence assumed that A8 enabled Hall telemetry and A9 restored
ordinary keyboard service without disabling that telemetry. That conclusion
needed instruction-level proof because the two handlers touched nearby global
state and it was initially unclear whether A9 undid the essential A8 state.

Required evidence:

- exact A8 and A9 handlers with resolved RAM addresses;
- identification of the suppression, telemetry, sweep-count, and auxiliary
  variables as distinct state;
- the A0 scheduler's exact gate;
- a hardware capture showing new A0 generation after the final A9 rather than
  only packets accumulated before it.

Without those points, `A9 -> A8 -> A9` could not be treated as a production
state machine.

## 2. Time bound versus a full snapshot

If firmware emits one key per eligible service interval, a complete 68-key
snapshot cannot appear instantly. The implementation must distinguish expected
initial sweep duration from protocol failure and must not declare success from a
partial map. It should report unique descriptors, ordered cycles, report gaps,
and elapsed time, with a bounded but realistic startup deadline.

## 3. Command acknowledgements are not Hall telemetry

`AA/A8` and `AA/A9` prove only that firmware processed a command. They do not
prove that A0 reports contain live Hall values, continue after the final A9, map
to physical keys, or update with physical travel. Command responses and
asynchronous reports require separate parsers and separate evidence.

## 4. Two unrelated meanings of A0

The host command table can contain opcode A0 while asynchronous reports also
start with byte A0. A safe implementation must prove that these are separate
meanings and must not send an unknown or potentially mutating A0 command merely
because captured input reports use that header. Production probing should be
restricted to a proven read-only/volatile allowlist.

## 5. USB/HID queues can imitate a post-A9 stream

Reports received after final A9 are not automatically reports generated after
that A9. Firmware, controller, driver, and Windows queues can retain earlier
traffic. A valid test needs timestamps, a known generation-rate bound, several
complete ordered cycles extending far beyond the possible backlog window, and
fresh physical changes after startup sweeps have ended.

## 6. Suppression versus ordinary keyboard input

A8 appeared to suppress or alter ordinary keyboard reporting. The review
therefore required proof that final A9 restored digital input while leaving the
Hall route active. It also required an automatic all-keys-up guard before A8:
if A8 initializes baseline from the current Hall position, activation while a
key is held could redefine rest and create stuck or inverted behavior.

The implementation also needed a guaranteed best-effort A9 on normal shutdown,
startup rollback, protocol error, timeout, and child-host failure.

## 7. `68/68` is coverage, not freshness

A complete startup descriptor set proves that every key was observed once. It
does not prove that the steady-state branch remains alive or that the retained
values are current. HallJoy must track per-key sequence/age and distinguish:

- initial forced coverage;
- post-sweep change-driven evidence;
- one stale key from total transport death;
- a new report from a duplicated or replayed report.

Fallback should be per key whenever possible; one delayed key must not disable
fresh keys or retrigger a disruptive global activation loop.

## 8. Stock event bandwidth may be too low

Even a correct A8/A0 route can be unsuitable for game input if firmware emits
only one changed key per slow scan. Required measurements include aggregate
report rate, per-key latency by scanner position, fairness under simultaneous
movement, release latency, and whether a lower-index continuously moving key
starves higher-index keys.

The test must cover individual W/A/S/D presses, A+D, W+S, and all four together,
after startup activity has settled. UI smoothness alone is not sufficient;
ViGEm output timing must be measured from the same values.

## 9. The analogue field itself required physical proof

Static packet structure cannot establish which field represents key travel.
Each candidate must be traced backward through firmware to the Hall scan state,
range and scaling, then correlated on hardware with slow press, full press,
hold, and release. Calibration, threshold, state, and baseline fields must not be
mistaken for live travel. The unit should remain “firmware-normalized range”
unless physical millimeters are independently proven.

## 10. Mandatory implementation evidence

Before production acceptance, the package needed:

- an explicit startup/run/recovery/stop state machine;
- exact device/interface/report fingerprint;
- exact serialized command frames and correlated responses;
- a raw hardware log containing timestamps and complete report bytes;
- a negative control showing that wrong/unproven devices are not claimed;
- positive control with physical key correlation;
- ordinary digital-input validation while the Hall route is active;
- per-key freshness and fallback behavior;
- bounded reconnect, shutdown, and A9 recovery;
- confirmation that UAP does not open the same vendor interface;
- no mutating exploratory opcode sweep.

The log needed to identify firmware/product revision, command TX/ACK order,
unique descriptor coverage, ordered cycles, A0 gaps/rate, malformed/checksum
errors, raw values, physical key edges, per-key age, fallback decisions, and
clean shutdown.

## 11. What could make the approach valid

The approach would be credible if disassembly proved independent telemetry and
suppression state, A9 left the telemetry gate unchanged, long post-A9 captures
exceeded every possible queue bound, physical movement changed one proven live
field for the matching descriptor, and per-key freshness prevented stale output.
In that case A8 could be a brief activation/baseline operation followed by A9
restoring ordinary service while asynchronous telemetry continued.

## 12. When stock firmware should be rejected

The stock route should be considered unsuitable when any of these remains true:

- no genuinely new A0 arrives after initial sweeps/catch-up;
- activation cannot safely avoid held-key baseline corruption;
- ordinary digital input cannot be restored reliably;
- report cadence or scanner fairness creates game-visible multi-key latency;
- releases can remain stale long enough to hold ViGEm output;
- UAP/vendor software contention cannot be prevented;
- recovery cannot guarantee a final A9 or bounded process containment.

A research workaround may tolerate restricted keys, low rate, or manual
activation. A production backend may not hide those boundaries. If stock
firmware exposes no sufficiently dense and fair report, the correct long-term
solution is a firmware modification that publishes a full or denser matrix,
with an explicit warning that custom flashing carries a real brick risk.

## Conclusion

At critique time, `A9 -> A8 -> A9 -> A0` was a promising hypothesis, not a
validated protocol. The correct next step was evidence, not additional guesses
or broad opcode probing. The subsequent firmware and hardware investigation
resolved the state-variable contradiction and confirmed the live field while
retaining the bandwidth/fairness caveat; see the archived resolution linked at
the top of this document.
