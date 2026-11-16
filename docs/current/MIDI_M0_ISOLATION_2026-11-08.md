# MIDI M0: optional lifecycle and independent outputs

Owner requested implementing the next [roadmap](../development/MIDI_ROADMAP.md)
milestone. Fresh defaults stay one enabled gamepad and MIDI disabled. Saved
MIDI-only profiles remain valid. No musical algorithm/feature expansion in M0.

## Implementation

MidiOutput_Start now arms the optional subsystem. Disabled MIDI creates neither
its driver worker nor its heap-owned queue/engine session. Disabled Tick exits
before reading input, comparing profiles or scanning note tables; Panic also has
an owner-free fast path. Port enumeration is rejected when MIDI is disabled.
Remap defers enumeration until activation/explicit refresh, and the tester constructs
its note telemetry only when showing MIDI. The small profile/status atomics remain.

The existing runtime supervisor handles optional start/reap; no new permanent
manager thread or timer was added. A separate enabled-change callback coexists
with the UI callback and wakes the supervisor through a static generation word,
without borrowed event-handle races. Reconcile runs before waiting, so a profile
change before supervisor startup is not lost. The existing 500 ms recovery cadence
remains; enable/disable and worker completion also wake it explicitly.

Disable closes note production and invalidates pending work immediately. The driver
owner releases actual sent notes and closes its port. Supervisor reaping checks
completion without waiting on the external driver. A retained owner that fails to
stop within five seconds is diagnosed and cannot be replaced while alive. Full
Pause/shutdown still uses a bounded join and retains dependent state on failure.
Re-enabling while cleanup is underway waits for that same owner to finish.
Queue/engine state is freed only after join and exclusion of the producer. The
realtime owner tries the producer mutex without waiting: driver calls and joins
cannot hold up gamepad acquisition. No forced thread termination is used.

Output startup uses a common tested policy: input proof remains required. With
MIDI disabled, gamepad startup failure retains existing dependency guidance. With
MIDI explicitly enabled, gamepad startup failure is local and cannot tear down
input/MIDI; MIDI startup/port failure cannot cancel the working gamepad either.
If both requested outputs fail, input/UI remain available for diagnosis/repair.
Already-disabled gamepads retain their established no-ViGEm-child behavior.
Safe failed initial gamepad starts can retry locally from the existing supervisor,
with one-second spacing and the original unsafe-restart protection preserved.

## Validation scope

The actual Windows MIDI owner with an isolated fake driver verifies:

- Disabled startup plus 1000 Tick/Panic/reconcile calls: no MIDI owner/session,
  note-table scan, input read, port enumeration/open or optional wake.
- Enable without a realtime tick, disable with a held note, exact release/close,
  freed session, no subsequent disabled-state work, reconnect-held suppression.
- Stalled driver send: control reap and 1000 disabled ticks return without waiting;
  releasing the fake stall permits orderly cleanup. No actual driver is wedged.
- Real production startup policy plus real MIDI owner: failed gamepad still sends
  MIDI; rejected MIDI owner creation leaves gamepad admission intact; ordinary
  gamepad-only failure does not invoke MIDI; explicit toggle recovers start failure.
- Concurrent input against 20 enable/disable/reap cycles, plus existing queue,
  Panic, port-switch, send-error, missing-port and recovery tests.

The integration audit verifies actual backend policy use, conditional UI work,
settings defaults and supervisor wiring. Existing profile-loader/transaction tests
cover absent MIDI metadata, saved 0-pad profiles and snapshot persistence. Software
failure injection is not a real uninstalled-ViGEm or external DAW session. No physical
keyboard, UI visual test, audio timing measurement or external receiver was used.

## Build and delivery

Native suite passed, including M0 integration, MIDI engine/setup/Windows owner and
profile tests. Targeted Windows owner checks also passed after the concurrent-stop
deadline ordering correction. Final ordinary Release pipeline passed, including
exact-image full catalog and HZ-94 admission gate, embedded resources, profile
transactions, 89 Supported layouts and startup recovery (18 scenarios plus repeat).
Initial MSVC compilation found a min/max macro conflict and a const WaitOnAddress
argument in the newly connected supervisor. Both were corrected without suppressing
checks; that failed attempt did not install a candidate. The successful build has
only the existing missing ViGEm PDB linker warning.

Installed normal delivery: build/bin/Release/x64/HallJoy.exe, version 1.6.15.0,
11,513,344 bytes, SHA-256 `aea8d10b4fa3a35d0ddd4c756ef45d1350255d80b9c2216e9f6a3c98b25187aa`.
Installed image equals the validated candidate byte-for-byte. The wrapper restored
the previously running app. M0 implementation and software acceptance are complete;
this does not establish external DAW/audio/hardware results.

Evidence: .local/research/midi-m0-20261108/ (native-checks.log, build-release.log,
build-release-retry.log and result.json with source hashes and image receipt).
Backup: .local/backups/midi-m0-20261108/manifest.json.
No keyboard support status change, Sheet mutation or GitHub publication is requested.
M1/M2 source history and measured velocity remain outstanding.
