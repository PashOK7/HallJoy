# Keydous NJ98-CP V4S: revision3 feedback and table ordering — 2026-11-06

[November 7 revision 4 retest](KEYDOUS_NJ98_V4S_LOG31_2026-11-07.md) records two hardware zero corrections, intermittent startup and the near-full output ceiling.

Owner supplies two latest logs and tester feedback: revision3 worked immediately,
but depth sometimes differs between presses and can remain stuck until other keys
are pressed. This follows the [liveness correction](KEYDOUS_NJ98_V4S_LIVENESS_2026-11-06.md).

## Exact logs and observations

| File | Bytes | SHA256 |
|---|---|---|
| HallJoy_wireless_v3.log |265646|5698d9a43b59bd1dc19a6883324459d6fc8be20a9c6aae04737cc33553a1deb2|
| HallJoy (30).log |495147|67b414296829b2d09c766faed49d649dfae9d8182df0d5f1a5a3f13cc78154ab|

Both are schema2, complete and summary-consistent, catalog27, with no queue loss.
Both prove receiver3151:5038, remote3793, USB0410, precision0,100/mm,4mm scale,
97 mapped positions, actual32-byte delta stream, mode revision3 and no fixed delay.
No inference of an exact EXE hash or8k analog delivery from version/name alone.

First: session begins1416984; first four genuine depth pages and stream enable
finish1417453:469ms. No exclusive-open failures; latest snapshot19684 successful
updates /0 failed updates. All connected snapshots have communication-warning0.
Initial eight sweeps average45.244ms. Later maximum full sweep31.047288s despite
continuing stream; latest retained packet event2798, stream I/O error0.

Second:13 error32 opens precede a transient read_ready timeout on identity8F;
identity then succeeds and initial depth/enable finish1621812,17.125s after begin.
Connection remains established in later snapshots; the initial communication
warning clears by1637734 and stays clear in captured snapshots. Latest combined
updates22575, failures0; retained stream event12560, I/O error0. Maximum table
sweep23.791241s. F7 send0/read1 is explicitly observed at1631593 and1632390.
This retains a real initial access issue; no holder is identified by these logs.

Bound-key traces retain residual native depths for seconds. Example S/HID16 stays
175/1000 from1751656 to1755890 (~4.234s), after an OS key-up at1751625. A/HID4
stays103/1000 for~4.297s. Some smaller residuals are suppressed by the existing
configured deadzone; some larger residuals affect the gamepad. An OS key-up is
NOT proof of zero physical depth (Rapid Trigger can release digitally at depth),
so it is never used to manufacture analog release. No raw per-key packet timeline
or packet-loss sequence is present: missed radio zero vs firmware behavior cannot
be established conclusively from these traces. Tester reports actual sticking.

## Two reproducible host defects

1. Client Prepare waited for send-ready BEFORE consuming an unsolicited/stale
   readable inbox. A legal send0/read1 state can therefore remain blocked until
   another action clears the inbox. The captured status establishes this state
   occurs, not that it accounts for every busy interval. A standalone replay with
   the exact prior production header reproduces page_ok0/send_ready (exit3);
   the corrected client succeeds on the same fake transport (exit0).
2. The table snapshot sequence ticket was captured BEFORE the complete Page call,
   including a readiness wait lasting up to200ms per attempt. A stream event during
   that pre-send wait made the later table value look older, even though the table
   request had not yet been sent. This could reject a genuine current zero when
   trying to reconcile a retained nonzero stream value. The ticket must instead
   be captured after readiness/draining, immediately before the actual E5 write.

## Revision4 correction

Before a new command, drain a readable inbox with the existing official FC read
mechanism and recheck F7. A response owned by an active Query is never discarded:
read-ready and pending late-response paths retain their ownership. F6 target
selection, exact remote identity and exclusive channel policy are unchanged.
No speculative new opcode, reset, calibration, configuration save or flash.

The actual production Page/Query calls a no-allocation pre-send hook after
Prepare, before command write. Under the publication mutex it records per-slot
stream sequence. Stream activity before that point cannot invalidate the newer
table; stream activity after that point still prevents an older table overwriting
new data. Only actual table values change publication. No digital fallback,
interpolation, synthetic release, expiry of quiet genuine holds or polling cap.

The existing independent reader and revision3 heartbeat/hold semantics remain.
New aggregate diagnostics count table adjustments, actual table-zero corrections,
superseded table slots and consumed inbox/late replies. These counts do not claim
lost packets. First eight zero-correction reports and later30s summaries are
bounded. No key IDs/values or raw payloads added; ordinary HallJoy.log/Open log,
no forced logging. bootstrap/mode revision4 and table_order=send_time identify
this code; unchanged stream/travel producer components retain their own revisions.

## Validation and delivery

Native suite PASS with cache. New regressions cover send-blocking initial AND
mid-session inboxes, failures during FC write/read, command ownership, motion
before readiness vs motion during reply, applying an actual table zero, and no
pre-send callback on failed admission. All previous29 I/O/cancellation positions,
quiet30s hold/idle, remote loss, late replies and stream/table tests remain.
blocked-inbox-proof.json records the prior-production failure and corrected pass.
These are host tests; the tester must confirm the physical sticking improvement.
Do not claim that all missing releases or initial error32 are now solved.

Ordinary tools/build_release.ps1 EXIT0. Exact-image full catalog, all backend
self-tests, embedded resources/licenses, production profiles/autoselection,
layout catalog and startup recovery18 scenarios plus repeated startup PASS.
All87 Supported layouts also pass against the fresh Sheet; compiled V4S ANSI
preset contains97 keys. Installed bytes equal the gated Release candidate.

Delivered build/bin/Release/x64/HallJoy.exe, version1.6.14.0,
11337728 bytes, SHA256 `952eab2e98f0c88f947d172c4f7bf855b620e12754c865d634f4a5def5f8473c`.
Supersedes the priorb78bd90c local build. Signed full-range axis fix, Keydous
liveness/bootstrap and every approved backend retained. No GitHub upload.
No hardware or visual test by the agent; actual sticking improvement is pending.

Tester: use this one ordinary EXE for repeated partial press/hold/release,
simultaneous keys and quiet intervals. Return normal HallJoy.log/Open log,
including a failed-start log if it recurs. mode revision4/table_order=send_time
and reconciliation counts identify the new code without requiring scripts.
Initial error32 contention is explicitly not claimed fixed.

## Support synchronization

2026-11-06 live Main/id0/grid1301x26 and exact search locate NJ98-CP V4S TMR472.
A471:C473 readback preserves Implemented; awaiting hardware testing, strict
validation and yellow base/effective colors. All283 yellow notices match the
fresh snapshot. Keep Experimental: successful genuine input is accepted, but
sticking and initial access remain under investigation. Runtime notice remains;
README scope and supported hardware/NEXT notes are reconciled with this result.

Private originals, machine analyses, repro binaries/source, gate logs and Sheet
readback: .local/research/keydous-nj98v4s-reconcile-20261106/.
Verified pre-change source/docs/EXE backup:
.local/backups/keydous-nj98v4s-reconcile-20261106.zip; hashes in backup-manifest.json.
No GitHub publication authorized.
