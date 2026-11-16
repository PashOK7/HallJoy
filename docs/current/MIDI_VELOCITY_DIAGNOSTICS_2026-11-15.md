# MIDI fast-strike velocity investigation

[Owner trace and K4 timestamp correction](MIDI_K4_SPARSE_VELOCITY_2026-11-15.md).

The owner reports intermittent soft notes during rapid presses and requested
diagnostic logging before a physical comparison. The existing estimator uses a
sparse interval when only one rising sample reaches the trigger. That interval
can include dwell since the last accepted released sample. This is a candidate
explanation, pending the owner's trace.

## Captured measurements

Only MIDI-bound inputs participate. Both the publication-history and compatibility
snapshot engines emit one diagnostic record for every attack, including all
attacks replayed in the same consumer wake. Each contains the physical source,
connection and key, MIDI note/channel, input sequence and consumer time, selected
start/prior/anchor and trigger depths/times, observation count, timing quality,
speed, velocity and effective sensitivity/curve/threshold settings.

The motion record preserves the first four and most recent twelve depth changes
before the attack. The total point count makes omission explicit. Points encode
age before trigger in microseconds, depth in permille and timing flags.
This diagnostic does not alter velocity calculation. The output owner also records
queued and attempted MIDI Note On/Off messages, including the driver's result.

Owners copy fixed-size records into a 256-record ring using a try-lock.
The control owner allocates optional per-key motion storage when MIDI starts.
Formatting and log writes happen on the existing support-log worker.
No allocation or formatting occurs for an attack on the realtime owner.
Overwrite and contention counters identify incomplete capture. The log exporter
keeps the report's existing bounded first/newest evidence retention. Open log
drains current records and includes input-history loss, queue overflow and output
error counts. Capture is event-driven, with no required holding or idle interval.

Enabling MIDI starts capture automatically in this build. Disabled MIDI retains
its startup fast path. Cached diagnostic records remain exportable after MIDI
stops. Capture is scoped to the owner's requested velocity investigation and
contains MIDI-bound per-key travel. It includes no keyboard text, other unbound
input, device paths, serials or port names.

Revision 2 persists MIDI events automatically through the existing snapshot
writer. Every recorded attack or output note requests a snapshot with a bounded
atomic operation. The final MIDI batch is saved on normal application exit.
The producer performs no file I/O. A tester can play and close HallJoy, then
the agent reads the ordinary HallJoy.log. Open log still exports the current data.

The owner's first physical trial was closed before Open log. Revision 1 retained
that session in process memory only, so its measurements were lost at exit.
The surviving app-data log was from 2026-11-15 00:19 local time and contains no
MIDI velocity trace. No strike diagnosis can be made from that stale report.

## Validation

Tests exercise measured and sparse strokes, multiple strikes in one batch,
unchanged MIDI output with capture, point-ring ordering, bounded diagnostic loss,
and the production output owner's queued/driver measurements with a fake driver.
The ordinary Win32 support writer checks that Open log includes MIDI evidence.
Physical acceptance awaits the owner's input.

The native backend checks passed with normal caching. The final release wrapper
passed the exact-image 12-family tester gate, embedded resources, compiled
profile/layout transactions and startup recovery. The initial revision 1 ordinary
portable EXE was `build/bin/Release/x64/HallJoy.exe`, version 1.6.15.0,
11,951,616 bytes. SHA256 is `995ca0dffdc851153ad1f5a765d4171715a7e3c833809916eaaebf0b2a9e876c`.
The revision 1 candidate and delivered bytes matched.

Logs are `.local/midi-velocity-focused-20261115.log`,
`.local/midi-velocity-native-retry-20261115.log` and
`.local/midi-velocity-build-retry-20261115.log`.
The original files and their hashes are in
`.local/backups/midi-velocity-diagnostics-20261115/manifest.json`.

## Revision 2 delivery

The ordinary EXE now saves MIDI measurements automatically, including a complete
final writer pass after Stop is observed. The regression test uses disabled
continuous logging and immediate exit with no manual Open log request, then
verifies MIDI evidence, the trace header and session.end in the saved file.

The MIDI output-owner checks passed with a fake driver. The final real Win32
writer gate passed, as did all 12 exact-image tester checks, production-linked
profile/layout transactions and startup recovery. Local logs are
`.local/midi-velocity-autosave-final-writer-20261115.log` and
`.local/midi-velocity-autosave-build-final-20261115.log`.

Current `build/bin/Release/x64/HallJoy.exe` is 11,951,616 bytes with SHA256
`cc3e4912ae43080b57d010e8250c7d30abb7dfda56705cbdf3c544ee075f83aa`. Candidate and delivery match. Backup originals are in
`.local/backups/midi-velocity-autosave-20261115/manifest.json`.
The owner needs one new physical playing session because revision 1 did not
persist the first session before exit.
