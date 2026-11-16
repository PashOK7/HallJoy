# MIDI runtime and tester — 2026-11-08

Current canonical plan and source audit: [MIDI_ROADMAP](../development/MIDI_ROADMAP.md).
It records unfinished work, final completion criteria and the later owner decisions:
one default gamepad, MIDI opt-in and niche, with a plan through full MIDI capability.
The report below describes its particular delivered implementation, not final readiness.

## Owner authorization and scope

Owner requested proceeding from the approved UI skeleton to working MIDI output,
velocity and a MIDI block inside Gamepad Tester. Earlier explicit objectives remain:
independent analog across supported keyboards, expressive velocity without an
artificial waiting interval, gamepad count 0/4, familiar note drag-and-drop, and a
better user experience than the reference implementation. A superiority claim
requires hardware comparison and has not been made.

## Implemented architecture

- Existing normalized analog acquisition feeds a separate MIDI consumer before
  gamepad inversion/curves. MIDI uses real analog availability, not digital states,
  held-key duration synthesis or mouse pseudo-key depth.
- Per-key attack observations use QPC time at the realtime consumer. Descent speed
  at the selected trigger maps to nonzero MIDI 1 velocity 1..127 with light,
  balanced or firm touch sensitivity. There is no fixed-velocity mode and no
  wait for peak depth or extra output cadence limiter.
- Notes use 5% release hysteresis, support polyphony and same-note aliases, retain
  attack velocity during hold, and optionally send polyphonic key pressure from
  depth. Note 0 and 127 and channels 1..16 are supported.
- The source data is the existing latest-value analog interface. Observations are
  consumer timestamps, not device sensor timestamps. A short complete movement
  coalesced before this consumer cannot be recovered. Sparse attacks are counted
  and shown; firmware-specific musical response remains unverified. This release
  does not claim sample-history preservation before the realtime consumer.
- Sampled note edges enter a bounded FIFO and are never overwritten by axis-style
  latest-value updates. A separate output owner calls WinMM. Slow driver calls
  cannot block keyboard acquisition. Overflow invalidates queued attacks, releases
  known sent notes, and requires physical release before rearming held keys.
- Panic/profile transitions/port changes invalidate pending events and release the
  exact sent note/channel pairs. Pause and shutdown stop/join the MIDI owner before
  dependent teardown. A failed 5-second join retains ownership and reports failure
  to the existing backend restart/poison policy. No forced thread termination.
- WakeByAddress signals give immediate work/Panic notification without a borrowed
  event handle race; an idle recovery wait does not delay queued notes. Failed or
  absent ports retry at most once per second, including when input is idle.
- Successful reconnection does not replay held notes. Per-key source loss releases
  its contribution and requires release on reacquisition. Cleanup is best effort
  when the external endpoint/driver itself fails; external audio is not guaranteed
  silenced by a disconnected device or process crash.
- K4 onboard analog demand remains active while MIDI is enabled/admitted, including
  hidden window and 0/4 gamepads. Its former 50 ms monitor heartbeat is bypassed for
  MIDI; no new firmware commands or flash writes were added.

## Ports, settings and UI

The implementation sends standard MIDI 1.0 through an existing Windows MIDI
output. Remap provides exact-name port selection, refresh, touch sensitivity,
trigger depth, aftertouch and Panic. No output port selected means monitor-only.
Duplicate names are explicitly ambiguous and are not silently opened. Port names
are persisted as validated UTF-16 hex to preserve Unicode even in the legacy
ASCII binding-file writer. Settings share existing transactional profiles and
import/export. Palette-only range/channel changes do not alter a sounding note.

This does not publish a new DAW-visible virtual endpoint automatically. To reach a
DAW, select an installed loopback port (for example a user-created loopMIDI port)
and choose that port as the DAW's MIDI input. Alternatively select an available
Windows synth/device output. Windows MIDI Services app-hosted virtual endpoints
remain a separate integration requiring OS/service/SDK compatibility work. No
third-party driver, vendor source or proprietary virtualMIDI SDK is bundled.

Gamepad Tester contains a MIDI Tester card even at 0/4 gamepads. It shows computed
versus successfully sent active notes, latched last attacks, note/channel labels,
velocity/depth bars, output status, sent-message/error/overflow counters and sparse
attack counts. Brief attacks remain inspectable in the last-attack lines. Eight
held-note rows are shown with a count for additional held notes; processing is not
limited to those rows. Successful API submission is not an acknowledgement from
the DAW or an audible-latency measurement. UI updates use existing visible-tester
invalidation and shared scroll surface, without another timer/scroll implementation.

## Diagnostics and validation

Structural support events: midi.output_state, midi.queue_overflow,
midi.worker_failed, midi.stop_failed and midi.start_failed. No key codes, note
values, user port names or per-key depth are written by these events. Continuous
logging remains optional. Detailed note state is confined to the tester display.

Targeted local checks passed before the full gate:

- Portable engine: different attack speeds, hold velocity, chords, hysteresis,
  pressure, disconnect/reconnect-held suppression, Panic, aliases, 100 rapid
  sampled repetitions, note/channel boundaries and 100,000 FIFO wraparound events.
- Production Windows owner with an isolated fake driver: monitor versus sent,
  port changes, Panic without further input, send error, idle recovery, slow driver
  queue overflow, stop/restart and missing-port recovery. It never opens a real
  musical device in that test build.
- Profile tests extended for Unicode port identity and invalid touch/trigger/
  pressure/port data. Existing snapshot/legacy behavior remains covered.

Final verification:

- Cached `run_native_backend_checks.py --require-compiler`: PASS, including all
  three MIDI tests and extended Windows profile-loader tests.
- `build_release.ps1`: PASS. Exact candidate retains the complete native catalog,
  embedded resources, production profile transactions, 89 Supported layouts,
  282 yellow notices, and startup recovery (18 scenarios plus repeated startup).
- Installed EXE and candidate are byte-identical. Build wrapper restored the
  previously running HallJoy window. Link map confirms midi_output.obj in the EXE.
- Delivery version 1.6.15.0, 11535872 bytes. SHA-256:
  `36981fd1193d62c5fa98bfcc7616be5cb66a02da02671368910eeb9e7645eac3`.
- Logs: `.local/research/midi-runtime-20261108/native-checks-r2.log`,
  `build-release-r2.log`, and `result.json`.

Initial full checks found a missing support-log declaration and an old static
assertion that assumed a nonzero gamepad divisor. Added the header and strengthened
that assertion to require the guarded zero-pad expression and MIDI tester surface.
The final gate above ran after these corrections; no compiler warnings suppressed.

Owner evaluates visuals. Hardware velocity consistency, DAW interoperability and
audible latency require user/tester validation. Keyboard support statuses and Sheet
are unchanged. No GitHub publication was requested.

## Backup and references

Backup: `.local/backups/midi-runtime-20261108/manifest.json`; additional originals
for keychron_onboard_backend.cpp, realtime_loop.cpp and the updated factory-reset
static audit are recorded in additional-manifest.json at their matching paths
under that backup. Restore only scoped paths after checking current changes.
Local check logs and private binaries: `.local/research/midi-runtime-20261108/`.

- Previous skeleton: [MIDI_SETUP_SKELETON_2026-11-07.md](MIDI_SETUP_SKELETON_2026-11-07.md).
- MIDI messages: https://midi.org/summary-of-midi-1-0-messages
- WinMM send semantics: https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-midioutshortmsg
- Windows app-hosted endpoint model: https://microsoft.github.io/MIDI/kb/virtual-device-app/
