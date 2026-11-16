# HallJoy MIDI output feasibility — 2026-11-07

Subsequent owner request: implement the UI skeleton first. See
[MIDI setup skeleton](MIDI_SETUP_SKELETON_2026-11-07.md) for current scope and progress.
The assessment below predates that implementation request.

## Scope and conclusion

Owner requested an assessment of HE keyboard to MIDI output, implementation
complexity versus gamepad output, and an interface proposal. No implementation
or release was requested. MIDI output is feasible as a new consumer of existing
keyboard depth providers; vendor protocol work need not be duplicated.
This document records design findings, not an implemented feature or hardware test.

## Existing integration points inspected

- `src/HallJoyProject/HallJoy/native_analog_backend.h`: common per-HID normalized
  depth interface. MIDI should consume depth before gamepad curves and inversion.
- `src/HallJoyProject/HallJoy/native_analog_snapshot_adapter.h` and
  `physical_analog_state.h`: latest-state snapshots/publication timestamps are
  not necessarily a complete ordered history of sensor samples.
- `src/HallJoyProject/HallJoy/backend.cpp`: cached raw reads, tracked key demand,
  Backend_Init and Backend_Tick. MIDI mappings must contribute their key demand.
  ViGEm initialization currently participates in backend initialization success;
  MIDI-only mode must initialize independently of virtual gamepad availability.
- `src/HallJoyProject/HallJoy/backend.h`: raw UI reads and virtual-gamepad controls
  exist, but UI refresh is unsuitable as a musical attack timing source.

## Proposed architecture

One HID owner -> normalized device/key depth with timing -> independent output
consumers. Reuse protocol discovery, depth normalization, physical layouts and
profiles. Introduce an explicit gamepad/MIDI output choice and a dedicated MIDI
configuration view. Keep keyboard identity available instead of accidentally
merging separate physical keyboards into one note source.

MIDI needs an ordered per-key note state machine. On crossing the attack threshold,
record the emitted channel/note and send Note On with velocity 1..127. Release
below a lower reset threshold to avoid jitter. Preserve Note On/Off edges even if
several arrive between output ticks; axis-style latest-state coalescing alone is
insufficient. Continuous pressure changes may be coalesced at encoded precision.

Track actual sent notes across octave, transpose, profile, port and device changes.
Release those notes and reset sustain where applicable on controlled teardown.
Offer a visible panic action. Unexpected process termination cannot guarantee
that external instruments receive a final release.

Use a bounded MIDI send queue/worker because legacy midiOutShortMsg can block.
Preserve release ordering and define overflow recovery; do not delay HID acquisition
or gamepad work while sending to an external MIDI device.

## Velocity, expression and timing

HE supplies position, not measured force. Standard note velocity is an initial
attack value, usually estimated from descent speed (depth difference/time) before
the configured trigger. Continuous depth can map to Polyphonic Key Pressure or
CC after onset. It does not become measured physical pressure by that mapping.

Good velocity requires useful sample timestamps and a short per-key attack
history from the acquisition path. Latest values at a UI timer are insufficient.
A board that feels smooth as a stick may expose insufficient attack detail for
consistent musical velocity. Verify actual sample cadence and repeated fast chords.
Estimating velocity does not inherently require a fixed extra waiting interval;
waiting for eventual maximum depth would add latency and should not be the default.

A first version can use MIDI 1.0 Note On/Off, velocity and optional polyphonic
pressure. MPE is optional for later per-note pitch/expression requirements, not a
prerequisite for a playable piano mapping. MIDI does not generate sound: synth/DAW
and audio buffer/driver settings also determine audible latency.

## Windows port strategy

WinMM can send to an existing MIDI output port. That alone does not publish an
application-owned virtual MIDI input for other programs. A compatible first route
can select an existing loopback port supplied by software such as loopMIDI.

Windows MIDI Services documents native application virtual endpoints. Assess SDK,
installed service/OS version and target DAW interoperability before promising an
automatic HallJoy MIDI port to all users. Documentation includes future rollout
material; do not assume universal current availability on Windows 10/11.
Do not embed the proprietary virtualMIDI SDK as a freely redistributable dependency
without the provider's required clearance. A custom signed driver is separate work.

## Interface proposal and verification scope

MIDI output settings: port, channel, physical-key to note mapping, octave/transpose,
attack/reset sensitivity, velocity curve, optional depth expression and panic.
Reuse the physical keyboard layout with note labels and active-note/velocity display.
Profiles persist MIDI settings separately from gamepad axes and curves.

Before delivery: verify fast repeated chords and preserved note edges, noisy
threshold hysteresis, zero-velocity handling, exact releases during profile/port/
disconnect transitions, output backpressure, MIDI-only startup without ViGEm,
DAW port discovery, timing under load, and human musical response on real hardware.
No such verification was performed for this assessment. Existing keyboard Supported
statuses and the Google Sheet are unchanged by this design discussion.

## Primary references

- MIDI Association message summary: https://midi.org/summary-of-midi-1-0-messages
- Wooting Analog MIDI: https://github.com/WootingKb/wooting-analog-midi
- Wooting implementation: https://github.com/WootingKb/wooting-analog-midi/blob/develop/wooting-analog-midi-core/src/lib.rs
- WinMM send behavior: https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-midioutshortmsg
- Microsoft application virtual device: https://microsoft.github.io/MIDI/kb/virtual-device-app/
- Microsoft requirements: https://microsoft.github.io/MIDI/kb/minimum-requirements/
- Microsoft porting/rollout context: https://microsoft.github.io/MIDI/kb/porting-midi-libraries/
- loopMIDI: https://www.tobias-erichsen.de/software/loopmidi.html
- virtualMIDI SDK terms: https://www.tobias-erichsen.de/software/virtualmidi/virtualmidi-sdk.html
