# Keychron custom FAR firmware / UAP latency review - 2026-09-21

Scope: read-only source review of the AnalogSense FAR firmware at commit
 e21916b695ec3b31fbbbb325e32849be5cdb9e46 (common HE scan, Q1 HE representative),
HallJoy's current local patched UAP and isolated-host input path. Not a benchmark
of a flashed keyboard, not proof that every customized Keychron build has the
same scanner. No runtime edits, firmware flashing, support status changes or
release changes. Google Sheet synchronization is not needed for this review.

## Findings

1. Shared FAR analog_matrix_scan.c:145 waits 40 us for every column. The loop
   at 253 scans every column. For a 15-column Q1 HE matrix this contributes
   600 us to each scan, excluding conversion and execution time. For this SAME
   scanner at 14/16/19 columns it would be 560/640/760 us. Do not transfer these
   numbers to newer 8K or other model firmware without checking its scanner.
   This settling delay is code-controlled but hardware-motivated: reducing it
   blindly can degrade readings. Sequential ADC work and shift-register delays
   add time; their cycle counts are not converted into an invented millisecond
   figure. ADC_GRP_BUF_DEPTH=1: no multi-frame moving-average buffer here.
   ANALOG_DEBOUCE_TIME defaults to 3 conversion attempts when digital state
   changes, NOT a 3 ms timer. update_raw_value runs inside each attempt before
   the digital debounce result, so FAR does not wait for digital actuation.

2. analog_matrix.c update_raw_value rejects raw changes smaller than 5 ADC
   counts relative to last accepted value. convert_to_travel quantizes to an
   8-bit depth; UAP getActiveKeysKeychron ignores travel<5 and divides by 235.
   With this firmware's scale 6 units per 0.1 mm, the first accepted code is
   approximately 0.083 mm (quantization/calibration affect the exact crossing).
   This is a displacement gate, not a fixed temporal delay. At constant key
   speed v mm/s, its nominal contribution from rest is 83.3/v milliseconds:
   about .833 ms at 100 mm/s or .167 ms at 500 mm/s. These are illustrative
   kinematics, not measured finger speeds. A subthreshold motion may never
   appear as active, so no unconditional press-to-active time bound exists.
   The local DEAD_ZONE=30 variable is not used in the returned polynomial
   calculation; do not count it as another implemented threshold/timer.

3. Custom FAR mode (version tail 0x45) requests A9/31: one bulk matrix request,
   30 data bytes per 32-byte reply. Firmware sends a report whenever full and
   sends the final reply unconditionally. Count=floor(rows*cols/30)+1.
   70/75 slots: 3 reports; 90/96/114: 4. Q1's 90 slots therefore include a
   redundant final empty payload. Host waits for ALL fragments before publish,
   delaying early-fragment keys by the remaining response time. This is a
   protocol-design cost even when USB service timing itself is excluded.
   No justified conversion of packet count to milliseconds is made here.
   Bulk firmware reads cached depth; it does not trigger a fresh scan.

4. main.cpp start_device_worker and halljoy_uap_poll_pacing.h target a 1000 us
   START-TO-START period. update_from_keyboard publishes BEFORE sleeping.
   Ideal cycle length is max(1000 us, transaction+publication work time).
   If work takes 200 us, requested sleep is 800 us; at 1500 us it is zero.
   With sub-ms cycles and uniformly distributed arrival phase, the polling
   phase alone contributes 0..<1 ms, mean .5 ms. It is not an extra fixed 1 ms
   added after every received snapshot. Lowering the target cannot help when
   the transaction already takes >=1 ms. sleep_for can overshoot in practice;
   scheduler behavior is outside this requested ideal code-only accounting.

5. UAP publishes a generation and condition-variable notification; child host
   waitForSnapshotUpdate(...,50) wakes on it. 50 ms is heartbeat, not delay.
   Host writes shared state and SetEvent; parent bridge captures broker state
   and calls RealtimeLoop_NotifyInputChanged. There is no mandatory 8 ms wait
   and no rendering-frame dependency on this path. One distinction: while an
   output deadline has <1 ms left, realtime_loop.cpp:251 uses a timer wait that
   cannot be interrupted by the input address wake. Input is already available
   in the broker, but engine processing can wait for that remainder (requested
   <1 ms, actual scheduler overshoot excluded). Not a further fixed 1 ms for
   every sample; gamepad output coalescing is beyond the requested endpoint.

## Robustness findings (not normal steady-state latency)

- Windows KeychronMtx is common to all Keychron reads and held through the
  transaction. Multiple keyboards or another cooperating app can serialize
  their full transactions. Per-device exclusion with cross-process ownership
  would avoid unrelated-device blocking; simply removing exclusion is unsafe.
- safeReceiveReport checks only A9/31; fragments have neither ordinal nor
  transaction ID. Size/count validation cannot distinguish a missing fragment
  from a duplicated/reordered or stale same-command response. Discarding stale
  reports before request reduces risk but is not a sequence guarantee.
- hwHid receiveReportWithReportId uses blocking GetOverlappedResult(TRUE), as
  does sendReport. Keychron has no transaction deadline here; safeReceiveReport
  can keep discarding unrelated frames. Therefore there is no finite local
  worst-case completion guarantee for an incomplete response. Host isolation /
  stale-worker detection elsewhere is recovery, not low-latency completion.
  Existing FAR mocks returning an empty report do not test an OS read that
  never completes. A deadline/cancellation-safe transaction is a concrete fix.

## Latency model and optimization order

Define S=actual interval between updates of the target key in firmware,
P=actual FAR snapshot cadence, R=time from sampling cached depth into the reply
to receipt of its LAST fragment, H=host publication/IPC processing work.
Steady-state ideal scheduling latency after the chosen depth threshold is
phase_scan + phase_poll + R + H; firmware request-service scheduling is part
of the actual P/R timeline. Phases are not necessarily independent. In a
simplified uniform-phase model their mean is S/2+P/2, not a measured result.
The 40us*columns contribution is a floor on scan work, not an upper bound on S.
Thus 0.6 ms scan waits plus 1 ms polling cannot honestly be advertised as a
1.6 ms worst-case end-to-end figure (nor .8 ms as a measured average).

Most useful changes to consider, without implementing them in this review:
1. Versioned FAR response with snapshot/fragment IDs and exact payload count;
   eliminate the redundant packet for exact multiples of 30 and add deadlines.
2. Optional scan-driven stream or compact changed-key reports with reliable
   release state/resync. Removes request-phase delay and reduces full-matrix
   fragment waiting; requires coordinated firmware/UAP changes.
3. Lower the 1000 us request target only after transaction timings show slack;
   e.g. 250 us would reduce ideal mean phase .5 -> .125 ms (gain .375 ms),
   at up to four times the request rate. Do not uncap by default.
4. Revisit the 5-unit depth gate if tiny initial motion matters; noise stability
   must be preserved. This changes sensitivity, not transport cadence.
5. Tune scanner settling/conversions only against exact board measurements.

## Evidence / verification

Firmware scan/header snapshots and immutable URLs/hashes:
../research/keychron-latency-20260921/sources.json.
FAR matrix implementation snapshot:
../research/known-protocols-20260921/keychron-far.c and sources.json.
Host sources: third_party/UniversalAnalogPluginFixed/main.cpp,
halljoy_uap_poll_pacing.h, overlay/Soup/soup/AnalogueKeyboard.cpp and hwHid.cpp;
src/HallJoyProject/HallJoy/analog_host_client.cpp and realtime_loop.cpp.

Executed: tools.tests.test_keychron_far (2 tests) PASS; production receive loop
with mocked fragments and generated Fn/key counts. uap_poll_pacing_static_audit
PASS across six private targets. Compiled/ran uap_poll_pacing_test.cpp PASS,
10,232 deadline properties; deterministic timing model, not wall-clock latency.
No physical measurement, visual app run or background diagnostics enabled.
