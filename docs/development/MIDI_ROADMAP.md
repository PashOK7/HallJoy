# MIDI: final product contract and implementation roadmap

Canonical plan, 2026-11-08. Checked against the current source, not only prior
implementation reports. The runtime report is [MIDI_RUNTIME](../current/MIDI_RUNTIME_2026-11-08.md).
This document is a plan; unchecked milestones are not implemented or hardware verified.

## Ordinary release build

The owner requested a gamepad-only build for review before the next release on
2026-11-15. Normal MSBuild, BUILD.cmd and build_release.ps1 compile without the
MIDI owner, WinMM MIDI calls, settings tab, remap palette or tester card. Existing
MIDI profile fields remain inert compatibility data, including on profile save.
An explicit MSBuild property (/p:HallJoyMidi=true) enables the feature for MIDI
development. It stays out of the ordinary release build and its exact-image
check verifies that MIDI imports and user-facing controls are absent.

The ordinary portable review EXE is 11,845,632 bytes (SHA-256
d9ed546b4c42fb138276d0d0780b6cf545dcc474371ad896ca75810c67edae96). Its bytes match
the checked candidate. The native suite, exact image (12 keyboard families),
profile and layout simulator, and startup recovery passed. The simulator
loaded a MIDI-enabled profile, excluded its MIDI-only binding from gameplay,
verified Remap hit testing, and retained MIDI assignments and output through a
save/reload. Evidence: .local/gamepad-only-native-20261115.log and
.local/gamepad-only-build-20261115.log. Source backup:
.local/backups/gamepad-only-build-20261115/manifest.json.

## Owner decisions and product priority

- HallJoy is primarily a gamepad application. A fresh installation enables ONE
  gamepad and disables MIDI. MIDI never becomes the default output.
- The user explicitly adds MIDI and may then disable the last gamepad to reach
  0/4. Persist and restore that choice, including an intentionally MIDI-only profile.
- MIDI is niche: no additional MIDI worker, port enumeration/retry, input history
  or full note-table scans when disabled. A small availability/configuration check
  is acceptable. Optional dependencies load only when their feature is requested.
- Keep Add MIDI Device as a secondary action beside Add Gamepad. Reveal MIDI
  settings and its tester after activation; advanced features use progressive
  disclosure. No MIDI onboarding, service installation or warnings for gamepad users.
- The owner requested a plan through the FINAL complete controller, not only v1.
  V1 is an intermediate milestone. Advanced features below belong to the final
  proposed scope, while remaining optional in everyday use.
- Preserve gamepad behavior/performance and all approved keyboard integrations.
  Prefer small demand-driven extensions to existing provider/profile/lifecycle
  facilities; do not rewrite every provider or build a general plugin platform
  solely for this niche output. Measure each necessary shared-path change.

## Source audit: current state and remaining work

M0 changes are recorded in [optional lifecycle and independent outputs](../current/MIDI_M0_ISOLATION_2026-11-08.md).
The earlier baseline findings below are updated as milestones are delivered.

| Area | Confirmed current implementation | Remaining work |
| --- | --- | --- |
| MIDI disabled | M0: arming creates no worker/session; disabled Tick/Panic fast paths; conditional port enumeration and tester construction | Keep this invariant through later features; lightweight profile/status checks remain |
| Input | M1 adds source/connection-qualified normalized publication history, position/code namespaces, captured timestamps and opt-in ordered UAP child-to-parent history | SDK-internal coalescing and already-collapsed provider aliases remain explicit fidelity limits; history transport failures retain a visible snapshot fallback |
| Velocity | M2: versioned motion estimator, captured host timestamps, documented units/anchors, editable five-knot curve, preset/custom full-velocity speed and optional last-stroke setup; explicit timing quality | Physical musical response measurement; upstream cadence and unobserved motion remain limitations |
| Note reset | M2: separate trigger, release and noise; partial-release retrigger; no timer before a note edge | Physical playability validation; compatible live edits remain M5 |
| Output | WinMM sends to an existing named output; no HallJoy-owned DAW endpoint | Native endpoint capability assessment and supported connection paths; stable identity where the API permits |
| Port identity | Exact name; duplicate names refused | Preserve safe ambiguity handling; distinguish endpoints by stable identity where supported, guide legacy selection |
| Backend startup | M0: gamepad-only keeps dependency guidance; explicitly enabled MIDI survives gamepad failure, and MIDI failure does not veto gamepad | Maintain output-local health/cleanup as more routes are added |
| 0/4 | Pads disabled separately; configured pad count retained >=1; disabled output runtime does NOT launch a ViGEm child | Preserve this valid behavior and add integrated no-bus/0-pad verification; do not claim ViGEm is always required |
| Queue | Ordered FIFO includes every changed aftertouch value; overflow releases/rearms | Coalesce continuous expression safely, preserve ordered note edges; measure queue age and congestion |
| Editing | Any SamePerformance change triggers global Panic/reset, including touch/pressure edits | Defined live-edit rules and immutable sounding-voice identity; avoid unnecessary chord cuts |
| Bindings | One logical MIDI setup, one note/channel per key; palette channel/octave affect future drops | Full control palette, layers, transpose, multiple actions/routes and explicit semantics |
| Tester | Computed/sent note state, last attacks, M2 speed/interval/timing quality, sparse/error/overflow counters | Further queue timing and external receive verification; API success is not DAW receipt |
| Validation | Engine tests and production output owner with fake MIDI driver | End-to-end provider/output tests, real receiver/DAW matrix and measured musical response |

Source anchors: midi_engine.h, midi_profile.h, midi_output.cpp, midi_event_queue.h,
backend.cpp, keyboard_subpages.cpp, vigem_output_runtime.cpp, remap_panel.cpp,
midi_engine_test.cpp and midi_output_windows_test.cpp under src/HallJoyProject/.

### Existing sound decisions to retain

Real analog input, no fixed velocity, no digital-depth substitution, separate output
owner, ordered sampled Note On/Off, exact sent-note bookkeeping, explicit Panic,
profile transactions, reconnect-held suppression and bounded error recovery are
useful foundations. WinMM and an explicitly selected existing MIDI port are valid
compatibility paths. A bounded queue and hysteresis are normal engineering choices;
unexplained constants and untested assumptions are the problem.

Position sensors measure travel. Velocity is derived from motion over time;
depth-derived aftertouch is expression, not measured finger force. The final system
must state these distinctions. Software cannot recover motion the firmware never
reported or transform coarse source data into additional measured precision.

## Target architecture

Existing keyboard owner -> source-qualified analog observations -> optional MIDI
attack/voice engine -> per-route ordered note events + coalescible expression ->
output owner/transport -> receiving application. Gamepad remains an independent
consumer of the same acquisition, with its current behavior preserved.

Reuse existing provider identity/timing fields where adequate. Add a bounded,
opt-in observation path at the actual publication boundary only where needed.
Use acquisition QPC if no sensor clock exists, and label its timing origin.
Provider silence on an unchanged held key is not itself a disconnect or release.
Source replacement/disconnect, malformed values and lost sequence information have
separate meanings. Preserve the best valid live input with visible quality limits;
do not invent musical precision or prohibit analog over unrelated metadata.

## Ordered milestones through final completion

Each milestone needs implementation, focused automated evidence, documentation
and its stated acceptance criteria. These are dependencies, not promised dates.
Existing gamepad fixes/releases retain priority. Do not enable unfinished stages
implicitly or expand MIDI startup costs for users who never turn it on.

### M0 — optional feature isolation and output independence

- Lazy start/stop of MIDI ownership and heavy state; no enumeration, retry wakes,
  MIDI history capture or note-table iteration when disabled.
- Preserve one enabled default gamepad. Respect saved MIDI-only/0-pad profiles.
- Start/diagnose outputs independently; preserve working MIDI when a gamepad fails
  and preserve gamepads when a MIDI destination fails.
- Gate: default/legacy/0-pad profiles, missing ViGEm, MIDI-port failure, repeated
  enable/disable and shutdown; prove no MIDI owner/driver calls in gamepad-only mode.

### M1 — trustworthy analog observations

[Implementation and remaining source qualification](../current/MIDI_M1_OBSERVATIONS_2026-11-08.md).
Normalized native history and ordered child-to-parent UAP publication history are
implemented. The software gate covers publications already received by acquisition.
Snapshot SDKs and upstream-collapsed aliases retain their declared provenance;
this is not a claim of recovering unseen sensor samples. Exact final build
validation is recorded in the report.

- Source/physical-key identity, connection epoch, sequence and honest timing origin.
- Deliver ordered movement/release observations already received by acquisition;
  do not rely exclusively on latest-value snapshots or UI refresh.
- Bounded optional history with explicit overflow/loss counters and recovery;
  reuse present provider infrastructure before adding transport or shared memory.
- Gate: complete press/release between consumer wakes, repeated fast chords,
  source replacement, duplicate HIDs across devices, held-key silence and overflow.
  Consumer scheduling changes must not alter the recovered received event order.

### M2 — defined musical touch

[Version 1 implementation, software evidence and remaining physical gate](../current/MIDI_M2_TOUCH_2026-11-15.md).
Implemented on 2026-11-15. The [MIDI settings tab](../current/MIDI_SETTINGS_UI_2026-11-15.md) adds a live editable velocity curve and key-travel view. Real musical acceptance remains open.

- Versioned estimator based on actual observed motion; document units, sample
  selection, rest/reversal/gap rules, source resolution and timestamp uncertainty.
- Separate velocity curve, trigger, release/retrigger and optional noise controls.
  Light/balanced/firm become documented presets, with editable curves and an
  optional user touch setup. No mandatory per-key ritual for every keyboard.
- Emit at the usable threshold crossing; no fixed waiting window for eventual
  bottom-out, no cadence reduction, no fixed-velocity substitute.
- Sparse input remains explicitly lower-confidence. Define its deterministic
  estimation policy before implementation; never silently label it high-quality.
- Gate: replay identical acquisition traces under different consumer delays with
  identical MIDI results (apart from declared rounding), monotonic response,
  noise/slow/fast/partial-release attacks, chords and nonidentical source cadences.
  Record physical response measurements; synthetic traces alone cannot certify touch.

### M3 — dependable musical delivery and usable DAW connection

- Preserve note edges and order while coalescing continuous pressure/controllers
  at their encoded precision. Bound queue age; do not play a stale backlog after
  congestion. Recovery cancels affected voices explicitly, with no held-note replay.
- Assess Windows MIDI Services app-hosted endpoints against actual OS/service/API
  capability and tested DAWs. Where supported, offer a HallJoy endpoint on explicit
  activation; remove it cleanly on exit. Keep existing-port output as compatibility.
- Show exact missing capability/setup action. No port with a misleading connected
  label, automatic third-party driver installation or guessed duplicate-name match.
- Gate: real software receiver verifies full message sequence, timestamps and
  cleanup; test late receiver, port disappearance/recreation, slow driver, errors
  and Panic while input is idle. Cover representative legacy and modern DAWs.
- M0..M3 form the first dependable note-controller milestone, not final completion.

#### Connecting a music app

Once the user adds a MIDI device, offer a way to hear a test note using the
Windows synth when available, and a separate action to connect a music app.
On a capable system, that action publishes a single HallJoy MIDI input through
Windows MIDI Services. Show the exact port name to select in the music app.
Wait for the MIDI 1.0 input to be visible before reporting it ready for older
apps such as LMMS. A ready port and an attached receiver are different states.
Keep the port active while its HallJoy output is selected and remove it when
that output is disabled or HallJoy exits.

Detect the OS build, the inbox API, the service and the app-endpoint transport
at runtime. A supported Windows version alone does not establish availability.
The current development PC reports Windows 10 22H2 (build 19045), so it can
test the existing WinMM route and setup guidance with LMMS. The native endpoint
needs a separate Windows 11 test environment with the API enabled. Target
Microsoft's inbox API for distribution and verify MIDI 1.0 input discovery,
note/velocity delivery and cleanup in LMMS. On systems without the native
capability, keep the existing MIDI outputs and explain the one-time virtual
port setup only when the user asks to connect another app. Preserve saved
output choices and keep HallJoy portable.

For Windows 10, loopMIDI uses the virtualMIDI driver. HallJoy could create its
own named port through the vendor SDK after driver installation. The vendor
requires prior clearance to distribute software linked to the SDK and offers
a driver MSI module to licensees. The public loopMIDI 1.0.16.27 ZIP is
7,884,702 bytes and contains an 8,139,264-byte full installer. Those sizes
do not establish the size of the licensed driver-only module. Keep any
licensed installer separate from the ordinary portable EXE.
[Vendor SDK and distribution terms](https://www.tobias-erichsen.de/software/virtualmidi/virtualmidi-sdk.html).

Other Windows 10 options reviewed from publisher documentation:
[LoopBe1](https://www.nerds.de/en/loopbe1.html) exposes an installed fixed
WinMM port and has separately licensed bundling and branding. Its public
installer is listed at about 1 MB. [SBVMIDI](https://springbeats.com/sbvmidi/)
provides eight installed ports and requires a bundling license.
[Bome BMIDI](https://www.bome.com/products/bmidi) provides an SDK for HallJoy-owned
named ports, a signed driver and commercial licensing. Its public standalone
driver installer 2.1.0.44 is 669,056 bytes per the vendor's HTTP response;
that is not a measurement of a licensed HallJoy package.
[Virtual MIDI Cables](https://github.com/tarekwasfy01/Virtual-MIDI-Cables)
is a BSD-2-Clause USB/IP proof of concept requiring usbip-win2 and target DAW
validation. These are options, with no HallJoy integration or LMMS test result
established by the documentation review.

References: [minimum requirements](https://microsoft.github.io/MIDI/kb/minimum-requirements/),
[app-hosted devices](https://microsoft.github.io/MIDI/kb/virtual-device-app/),
[SDK deployment and runtime detection](https://microsoft.github.io/MIDI/kb/porting-midi-libraries/).

### M4 — complete musical control palette

- Draggable notes, chords, velocity curves, octave/transpose actions, sustain,
  sostenuto/soft-pedal controls, assignable CC, channel pressure, poly pressure,
  centered pitch bend, program/bank selection and explicit RPN/NRPN controls.
- Define momentary/toggle/absolute/relative modes only where meaningful; set
  neutral/reset values and hysteresis per control. Label depth-derived expression.
- Multiple actions per key, zones/layers, layer switches and per-route channels;
  bulk-edit existing mappings separately from changing the future-drop palette.
- Explicit sustain/retrigger/alias policies, including an optional repeated-note
  articulation policy. Never let one alias accidentally release another.
- Gate: protocol encoding boundaries, controller neutral/reset, pedal semantics,
  simultaneous notes/controllers, transpose/layer changes while voices are held,
  profile import/export and old profiles. Verify in a real receiver and instrument.

### M5 — multiple routes and predictable live edits

- Allow additional logical MIDI devices/destinations when explicitly added;
  one remains sufficient by default. Channels, zones and routes target different
  instruments; no extra always-running devices or four-device gamepad-style limit.
- Capture destination/channel/note at voice onset. Defined profile/port teardown
  releases old voices; compatible touch/palette edits do not panic unrelated notes.
- Track source and route contributions and port reconnection generations. Panic
  targets owned voices/controllers; document shared-port/channel interaction.
- Optional MIDI feedback/control learning uses explicit input selection and loop
  prevention. DAW MIDI Learn can use ordinary emitted controls without this feature.
- Gate: two sources, multiple destinations, shared channels, partial route failure,
  active-note edits, independent reconnect/Panic, feedback loops and full persistence.

### M6 — expressive per-note operation (MPE)

- Explicit MPE zones and member-channel allocation, per-note pitch/expression and
  pressure; negotiate/configure pitch range and relevant controller state.
- Define channel exhaustion and optional voice stealing, with release/reset before
  reuse. Do not silently fall back to shared-channel pitch for independent voices.
- Gate: compatible MPE receiver/instrument, chords with independent expression,
  channel exhaustion/reuse, sustain, port changes, Panic and old-profile compatibility.

### M7 — native MIDI 2.0 where supported

- Native UMP transport, endpoint/protocol discovery and capability-based feature
  selection. Higher-resolution velocity/controllers and per-note controls preserve
  available source precision; widening integers does not add measured resolution.
- Persist musical intent independently of transport. MIDI 1 conversion has explicit
  precision/feature limits; unsupported capabilities are visible and safely scoped.
- Gate: compatible native MIDI 2.0 receiver plus legacy interoperation, protocol
  selection, reconnect and equivalent note/cleanup behavior. Verify current SDK,
  deployment and redistribution requirements before adding an optional dependency.

### M8 — final experience, diagnostics and acceptance

- Simple note setup remains a few explicit steps; advanced panels are optional.
  Presets explain instrument/channel use, and profiles/import/export cover all
  completed features with versioning, validation and recoverable migration.
- Tester shows source quality, estimated velocity, expression, generated/queued/
  API-submitted events, queue age and errors. Remote receipt is claimed only when
  an actual test receiver supplies evidence; sound latency needs audio measurement.
- Bounded ordinary logs identify failing stage without exposing played notes or
  user device/port identity. No tester scripts are required for ordinary diagnosis.
- Record acquisition->engine->submission timing and receiver timing separately,
  with p50/p95/p99/max under idle and load. Establish numerical budgets from the
  tested source cadence before claiming a latency PASS; never promise zero latency.
- Validate sustained chords, fast repetitions, held releases, multiple supported
  source families/transports, MPE/MIDI 2 paths and adversarial lifecycle sequences.
  Publish an explicit tested OS/API/DAW matrix and known source limitations.
- Compare musical response/usability with the intended reference implementation
  on equivalent hardware/settings before claiming HallJoy is better.

## Final definition of done

A fresh user gets one working gamepad and no MIDI setup burden. A MIDI user adds
an output, sees a usable connection route, binds notes or controls, and can use
0/4 gamepads. Slow and fast attacks are expressive from actual observed motion;
chords, partial retriggers, pedals, layers and controllers have documented semantics.
Multiple routes, MPE and MIDI 2.0 are available on the declared compatible paths.
Switching profiles/ports, Pause and normal exit release the exact owned voices;
reconnection never replays held keys. Source/transport limitations are visible.

All M0..M8 acceptance evidence is recorded. A planned feature or a fake-driver PASS
cannot close its real interoperability/musical gate. No hidden fixed velocity,
invented force, recovered firmware samples, automatic guessed ports or silent
advanced-feature downgrade is used to make the completion claim.

This is the proposed complete MIDI-controller scope. A built-in synthesizer,
audio workstation or vendor-firmware rewrite is a separate product decision.
Unavailable sensor detail and a crashed/disconnected external instrument cannot
be repaired by promising software. New scope must amend this contract explicitly,
rather than moving the finish line without recording it.

## Evidence and next action

M0 implementation and software acceptance are complete; the
[delivery report](../current/MIDI_M0_ISOLATION_2026-11-08.md) records final gate
results, limitations and image provenance. M1 normalized-host and ordered UAP
publication history and software acceptance are complete; final ordinary-build
provenance and source capabilities are recorded in its
[report](../current/MIDI_M1_OBSERVATIONS_2026-11-08.md). M2 implementation and software
acceptance are recorded in its [report](../current/MIDI_M2_TOUCH_2026-11-15.md).
M2 physical musical response still needs measurement; software traces cannot close
that gate. Next engineering stage is M3 delivery/DAW connection. M3..M8 remain open.
Hardware response and DAW evidence are not implied by software history tests.
The initial roadmap-only task is backed up under
.local/backups/midi-roadmap-20261108/; M0 source originals are under
.local/backups/midi-m0-20261108/.

Primary references checked 2026-11-08:

- [MIDI message definitions](https://midi.org/summary-of-midi-1-0-messages)
- [WinMM send semantics](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-midioutshortmsg): driver submission may block, supporting an independent owner.
- [Application virtual endpoints](https://microsoft.github.io/MIDI/kb/virtual-device-app/): host API and client interoperability differ; verify both.
- [Windows MIDI Services requirements](https://microsoft.github.io/MIDI/kb/minimum-requirements/): check the actual OS/service/API deployment rather than assuming availability on all Windows versions.
