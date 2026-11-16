# MIDI interval selection near the end of travel

The owner's next log is 425,519 bytes, exported at 2026-11-15 18:48:44 UTC.
SHA256 is `b7569ff211d41f9e236a280d5d584bb09c86e49f19580ab94266bbcde0fdab0e`.
The local copy is `.local/research/midi-terminal-velocity-20261115/HallJoy.log`.
The owner reports forceful presses plus a few deliberately slower strokes.
Intended effort cannot be assigned to individual notes from the log alone.

There are 213 attacks, including 157 at velocity 127. Ten are at or below 90.
Six low attacks first show 670-792 permille, then 1000 after 7-14 ms. Their
velocities are 76,87,86,88,86,82. The old measured path excludes the initial
67-79% of travel, so only the smaller terminal remainder determines speed.
The four other low strokes span 30-133 ms and have wider captured motion.
History and diagnostic losses, invalid observations, queue overflows and MIDI
send errors are zero. Maximum replay age is 16,006 microseconds.

The correction retains the initial movement interval when its starting point
was a genuine timestamped unchanged publication. The K4 acquired-frame path
provides that point before a change. Baseline routes and consumer wakeups cannot
qualify it. At the trigger, the estimator compares the initial and remaining
measured travel. An onset is selected only when it covers more travel and has
greater observed speed. Otherwise the established interior interval is used.
A reversal, interrupted stroke or release resets the onset candidate.

This avoids estimating an entire fast strike only from a small remainder near
the bottom stop. It also rejects tiny initial jitter as a reason to raise a slow
stroke's velocity. Every selected speed uses two received depths and their actual
timestamps. Missing origin timing retains the previous uncertainty policy.
Velocity still follows the saved sensitivity and curve.

The diagnostic `evidence.midi.selection version=2` records whether the onset was
selected, its verified start and the selected endpoint. The trigger remains
separately recorded. This makes a speed measured before the trigger auditable.

Regression fixtures reconstruct six clipped-terminal shapes with explicit
acquired origins, rather than claiming missing timestamp bytes were in the log.
They check both consumer batching schedules, absence of qualifying origins,
stale origins, tiny early jitter and genuinely slow strokes. Corrected playing
response still requires the owner's physical check.

The native suite and normal release wrapper passed. This includes 12 exact-image
tester checks, production-linked profile/layout transactions and startup recovery.
The installed portable EXE is 11,956,224 bytes, SHA256
`107f9512d347ea288c06ee04cc582727298775377ff43055d44efc3e2425da19`, with identical candidate bytes.
Verification logs are `.local/midi-terminal-velocity-native-20261115.log`
and `.local/midi-terminal-velocity-build-20261115.log`.
The pre-change source manifest is
`.local/backups/midi-terminal-velocity-20261115/manifest.json`.

## Timing spread after the absolute curve update

The 22:31:13 MSK owner report is archived in
`.local/research/midi-k4-speed-spread-20261115/HallJoy.log`. SHA256
`4ce443b6d6663f30981152c556c7c9d212f3fc626d0e6f8f00d8390c89ffd367`.
The final same-key, same-profile group contains 27 attacks. Nineteen have
only a single observed rise from 0 to 1000 permille. Eleven of these
span about 7 or 8 ms and produce about 125 to 143 key travels per second.
The other eight span about 14 or 15 ms and produce about 67 to 71.
Logged MIDI velocities for those groups are 118 to 127 and 86 to 90.
See attacks 135 and 150 for identical depth endpoints with intervals
14,998 and 7,003 microseconds. This accounts for the reported spread without
establishing the physical speed or the origin of the longer acquisition gaps.
The report shows no history loss, invalid observation, queue overflow or send
failure.

The K4 backend timestamps receipt of a complete analog frame. Effective analog
delivery is the timing relevant to velocity. A keyboard's digital USB polling
specification does not establish analog sample cadence. A stroke bracketed by
two received depths has only a measured interval, so timing and speed within
that interval remain unknown. The same limit applies to slower backends and to
SDKs that expose the latest snapshot. Timestamped intermediate readings or
keyboard-side velocity can retain more motion information even if the host
receives them in batches.

For the same recorded travel of 1000 permille, attack 126 used 13,999 us
(71.434 travels/s), attack 135 used 14,998 us (66.676 travels/s), and attack
160 used 14,000 us (71.429 travels/s). The difference is the measured time
between host publications. The MIDI consumer received the corresponding
trigger observations 4–6 us after publication. For snapshots 18–24 the
valid K4 matrix update counter rose by 126–127 per roughly one-second window.
An earlier [instrumented r6 run](K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md)
measured 347.024 device scans/s and 162.667 complete matrices/s on a neutral
profile. Those figures are from a separate firmware run, but independently
verify that device scans and host matrices are different clocks on this path.

The K4 client requests a matrix and hands it to the backend after validating
and assembling the response. The sparse firmware response carries the scan
sequence, which the client checks for forward progress. The backend uses the
host clock after the complete response and the MIDI report does not carry the
scan sequence or the device sample time. The sparse firmware snapshots the
matrix once per request and sends pages through the raw endpoint when it can
submit them. An 8 ms response interval and a 15 ms interval can each bracket
motion in adjacent received matrices. The log cannot assign the 1 ms steps
or the longer gaps to a specific scan, request, USB reply, or heartbeat.
A paired device-sample and host-receipt trace would distinguish those stages;
the 7/8 and 14/15 ms groups alone do not prove a fixed device frame period.
