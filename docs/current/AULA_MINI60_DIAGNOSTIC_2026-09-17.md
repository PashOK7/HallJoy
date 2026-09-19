# AULA MINI60 HE Pro diagnostic build — 2026-09-17

> Superseded for delivery by [the playable native analog build](AULA_MINI60_NATIVE_SUPPORT_2026-09-17.md). The current EXE publishes analog to the normal gamepad path and logs continuously; earlier diagnostic-only completion gates below are historical.

Status: diagnostic EXE built and software checks passed. Ready for the first wired MINI60 HE Pro tester run; hardware behaviour remains unverified.

## Owner decisions

Deliver the ordinary HallJoy.exe with its normal gamepad functionality and one HallJoy.log, in the existing IrokNa87Diagnostic output directory. No separate tester ZIP/tool. UI, logs and code comments are English. On 2026-09-17 the owner questioned fixed-duration testing: completion now depends on collected evidence, not a prescribed 40/60-second run. Timeouts protect I/O and stalled workers; a short released-state observation and idle exit are not a typing schedule. Closing at any point preserves partial evidence.

## Scope and evidence

Source research: ../research/AULA_MINI60_HE_PRO_2026-09-17.md. Only wired 0C45:80A2, vendor collection FF68:0061, 65-byte Windows input/output envelopes is eligible. Receiver 0C45:FEFE is inventoried without commands. Multiple eligible collections stop command traffic rather than guessing. Other revisions and receivers remain unqualified.

The recorder reads device information (0x10) and key assignment table (0x12), enters temporary simulation (0x66), records 0xFB sensor reports, and sends simulation-off (0x67). No calibration, reset, firmware update, configuration writes or macro-body reads. Key assignments are diagnostic metadata, not automatic layout support. Device-info VID/PID mismatch stops the run. Missing info replies are logged; USB identity still allows the already-reviewed simulation commands. WriteFile is tried first; SetOutputReport is a contained fallback. Missing ACKs do not discard unsolicited reports.

Completion requires at least eight analog keys, four varying depths, eight ordinary keys pressed and released, multi-key digital hold with multiple analog keys and changing depth, then release of all digital keys and a short observation. This is diagnostic coverage, not proof of a complete analog protocol. A no-analog result after sufficient digital input is informative and ends with limitations. Missing digital events, 45 seconds without digital transitions, disconnect, short/malformed reports, identity mismatch, permissions, transport errors, child crashes and forced timeout are distinguished. With no eligible USB device the recorder waits for hotplug until the window is closed; there is no initial plug-in countdown. Busy opens are retried for up to 45 seconds, with an explicit close-AULA-software prompt. At most two wake retries are sent if ordinary input appears without analog packets. Reconnect recovery is bounded to three sessions; each session retains its own summary.

Worker runs in a kill-on-close job, assigned while suspended. Only pipe, cancellation, NUL and anonymous counter mapping handles are inherited. Parent remains responsive and drains pipe/checks deadlines even during continuous packets. I/O cancellation drains OVERLAPPED ownership; a packet completing during cancellation is preserved. The supervisor detects stalled output, tries cleanup after interrupted sessions, and reports cleanup uncertainty. Physical unplug or process termination can prevent cleanup: USB reconnect may be needed, and no code can guarantee device cleanup after power loss.

Log records aggregate sensor ranges, endpoints, raw units, increases/decreases/zero counts, gap and quiet-tail observations, key assignments without macro contents, digital press/release/repeat counts, transport and cleanup results. No ordered typed keys, serials or HID paths. The existing public-field sanitizer applies. One direct-append HallJoy.log owner; support logging shares that sink. Logging failure suppresses successful coverage and cancels capture.

## Validation and delivery

- MSVC Release/x64: PASS; only the existing ViGEm debug-symbol LNK4099 warning remains.
- Final EXE `--halljoy-mini60-self-test`: PASS. Request allowlist, report framing/ranges, independent slot aggregation, gap/zero counters, evidence-based phase completion, insufficient-data rejection, repeated press during the release observation, pipe delivery, forced hung-child timeout, cooperative cancellation and nonzero child exit. These tests do not access hardware.
- Actual Windows trace implementation: PASS for readable direct append before close, no NUL tail, private-profile/path redaction, final marker, and a sharing-denied log open that preserves the prior file and reports disabled logging. Registered in the shared Windows test runner.
- Public diagnostic field and HID I/O ownership/cancellation lifecycle regressions: PASS.
- Shared static gate: all checks passed across the initial prefix and resumed suffix. The first run stopped at an old textual assertion permitting direct append only for DrunkDeer; that assertion now explicitly covers this recorder too. Logger, support log, trace, encoding and execution-coverage checks passed after final changes.
- Parent crash handler uses the existing external watchdog to append the terminal code to HallJoy.log; it does not collect the parent memory/register dump for this variant. Forced worker exits and recovery attempts are also retained in the same file.
- No real AULA hardware or visual app test was performed. No claim of complete analog support, correct physical travel units, or lossless release delivery is made.

Build command: MSBuild HallJoy.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:HallJoyAulaMini60Diagnostic=true.

Artifact: `build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe` (existing legacy directory), 9,291,264 bytes.
SHA256: `d471d64a3d7544d65780dad2c40f99e4540c9a1f9430bb48dad20efeff71a132`.
Only Windows DLL imports; no extra compiler runtime dependency. No tester archive was produced.
Evidence: `.local/aula-mini60-diagnostic-verification.json`; internal pre-change backup: `.local/backups/aula-mini60-before-20260917-092704.zip`.

Tester flow: connect the keyboard itself by USB, launch HallJoy.exe, follow its English title prompts (eight or more individual keys with varied depths; hold two or more while varying one; release all), then close and send the adjacent HallJoy.log. A limited-data status is also worth sending. Closing early retains partial evidence. Ordinary HallJoy/gamepad operation is preserved; the research samples are not published as a qualified analog provider.


## First tester log — HallJoy (13).log

Reviewed 2026-09-17. Input: C:/Users/PC/Downloads/HallJoy (13).log, 33,644 bytes,
340 lines, SHA256 b852ae9a4ccb807019337c708a7c612d56516e2833cf9b8a0bcf700671ed3415.
No NUL bytes. This is real tester evidence, not a local hardware run.

- Correct wired 0C45:80A2 / FF68:0061 collection selected; 65-byte Windows reports.
  GET_DEVICE_INFO succeeded on WriteFile, version bytes 52:01 = V1.52;
  bcdDevice also 0152. The previously inspected image was V1.55: do not conflate them.
- Full 512-byte assignment read succeeded. Slots 17–21 report type 0 (factory default),
  so their physical key names must not be inferred from zero usage fields.
- Simulation-on acknowledged. 9,605 parsed sensor packets from five slots (17–21),
  all five with changing travel. No malformed/unknown packets or read errors.
  The 185 read timeouts are empty bounded waits, not 185 failed USB commands.
- Per-slot travel spans: 0–369, 0–369, 0–368, 0–361, 0–340, respectively.
  Each slot includes zero-valued samples (21, 20, 20, 25, 19; total 105).
  Reported stroke stays 34. Do not divide travel by 34 or claim millimetres without
  reviewing this version's conversion; observed extrema are not a fixed full-scale proof.
  Reported low endpoint varies, especially slot 21 (1235–1600); high endpoints are stable.
- Ordinary input remained observable: five distinct pressed/released keys,
  five presses, five releases, 14 repeats, no orphan releases, zero held at close.
  This supports coexistence of the sensor stream and ordinary keyboard events.
- No multi-key hold was observed: maximum_held=1, chord_packets=0,
  recent50_max=1. All packets are phase 0. This run does NOT distinguish full
  simultaneous per-key reporting from a selected-key-only implementation.
- The app closed at approximately 7.34 seconds: cancelled=1, worker exit=4,
  forced=0, complete=0, sufficient=0. These indicate early cancellation, not a crash.
  Both normal and redundant recovery simulation-off commands were acknowledged;
  app session ended with exit_code=0. The partial log is useful and intact.

Conclusion: live varying sensor values on real V1.52 hardware and concurrent ordinary
input are now confirmed. Reliable zero on EVERY release, multi-key independence,
physical slot mapping and normalization remain unqualified. Zero samples exist, which
is more positive evidence than the earlier static near-rest gate alone, but unordered
aggregates cannot prove that every release produces a zero. The same diagnostic EXE
can collect the missing chord/release stages; no replacement build is required for that.

## Simultaneous tester run — HallJoy (14).log

The tester clarified that log 13 used one key at a time. For log 14 the tester
explicitly held keys 1, 2 and 3 together for about one second (owner-supplied
Discord screenshot). This removes the previous ambiguity about the physical action.

Real V1.52 capture: 8,906 valid analog packets, slots 17/18/19 all varying.
Digital maximum_held=3, presses=3, releases=3, held_at_close=0, orphan_releases=0.
While at least two ordinary keys were held, chord_summary reports 8,612 packets,
three analog keys and three varying depths. recent50_max=3. This provides hardware
evidence for multi-key reporting during a simultaneous hold, rather than only a
single reported slot for the entire hold. It does not certify every arbitrary chord
or independent response to a controlled depth change while other keys are held fixed.

Per-slot counts are uneven: 7,105 / 895 / 906. All three report maximum host-receipt
gaps of 16 ms and zero gaps exceeding 50 ms. Counts are not proof of a uniform sample
rate; Windows receipt timing is coarse. This asymmetry is consistent with the
previously reviewed busy-endpoint/no-retry concern, but does not identify its cause.
Travel spans are 0–369 / 0–370 / 0–370; zero counts 28 / 5 / 15. Reported stroke=34.
Ordinary release events and analog zeros exist for all three slots; unordered
summaries still do not prove zero delivery on every release.

Malformed/unknown/read_errors all zero. Start and cleanup acknowledged; normal
application exit_code=0, no forced worker termination. sufficient=0/phase=0 reflects
the diagnostic's eight-key/four-varying-key coverage gate and close cancellation,
not failure of this targeted three-key test. Do not ask the tester to extend the run
merely to satisfy that gate: the requested simultaneous-key observation succeeded.
The next implementation work must retain per-key freshness, review normalization
and default physical mapping, and define release handling. No binary was changed
while analysing this log.

Log 14 identity: 32885 bytes; SHA256 `d6aa41436d8fabeb616ec4090e8b0b51b31ba7b1ca305b54c676f85e31efce28`; NUL bytes: 0.
