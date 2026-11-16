# K4 single-frame MIDI velocity correction

[Next owner trace and terminal-interval correction](MIDI_TERMINAL_VELOCITY_2026-11-15.md).

The owner's second session exported 212,412 bytes at 2026-11-15 18:34:38 UTC.
SHA256 is `3d4854d1bea1a0f3def786b26bcc2d786ab73d01b119360d39b7269fb37cddc7`.
The local immutable copy is `.local/research/midi-owner-velocity-20261115/HallJoy.log`.

The trace contains 56 attacks. All 25 velocities below 40 are sparse one-step
attacks, calculated against the preceding release 96-176 ms earlier. Attack 26
rises from zero to 959 permille and incorrectly uses 175,998 microseconds,
yielding speed 5,449 and velocity 22. Attack 31 uses 168,999 microseconds for
852 permille and yields velocity 21. All 56 Note On messages reached the driver,
with no history loss, diagnostic loss, queue overflow, invalid observations or
MIDI send error. Maximum consumer history age is 21 microseconds.

The source is the K4 HJO1 backend. It receives accepted depth frames at roughly
8 ms intervals. The observation stream removed unchanged zeros, so the touch
estimator's previous timestamp represented the prior note release, even though
later real polls had confirmed zero.

The K4 backend now captures host receipt time after each accepted complete
Depth transaction. For each changed key, it also publishes the genuine value
and timestamp from the immediately preceding accepted frame. Unchanged frames
remain absent from the journal. An earlier timestamp is used only within the
same capture journal and keyboard connection. A weak reference tracks capture
identity without preventing MIDI shutdown.

The shared publication API accepts these explicit acquisition timestamps.
Existing delta sources continue without invented preceding samples. Velocity
uses the actual observed interval and keeps its sparse quality label for a
single-step stroke. No fixed velocity, timing clamp, sensitivity adjustment,
extra device requests or polling-frequency change is introduced.

A regression passes repeated stationary polls and a one-step jump through the
production physical publication, journal and MIDI engine. It verifies an 8 ms
interval instead of the inter-note dwell, actual velocity from that interval,
independent release, and absence of per-frame idle traffic. Missing or invalid
preceding timestamps retain the existing source limitations.

The physical trace identifies the bug. Confirmation of the corrected playing
response awaits the owner's next session.

The native suite passed with normal caching, including the acquired-frame
regression. The release wrapper passed all 12 exact-image tester checks,
production-linked profile/layout transactions, 92 Supported layout entries and
startup recovery. The installed portable EXE is 11,954,688 bytes with SHA256
`dcda9832640078d7b31804e47f82158af091db10d7b94d7c713f8a33a2ede20f`. Its bytes match the verified candidate.
Logs are `.local/midi-k4-velocity-native-20261115.log` and
`.local/midi-k4-velocity-build-20261115.log`.
The source backup is
`.local/backups/midi-k4-sparse-velocity-20261115/manifest.json`.
MIDI diagnostic autosave remains included in this build.
