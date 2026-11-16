# MIDI setup skeleton — 2026-11-07

Follow-up: [working MIDI output and tester](MIDI_RUNTIME_2026-11-08.md) adds runtime
processing after the owner approved this skeleton. The record below describes the
previous delivery.

## Owner scope and decisions

Owner requests Add MIDI Device next to Add Gamepad, a draggable binding palette,
and the ability to disable the final virtual gamepad (0/4). Follow-up explicitly
limits this step to a skeleton. MIDI transport and velocity are deferred; fixed
velocity is not the intended final musical response. The objective is a more
usable experience than the Wooting reference, with actual response comparisons
reserved for future hardware evidence.

One logical MIDI device initially: channels 1..16 can address different instruments.
The skeleton configures note/channel assignments. The synth/DAW chooses sounds and
can remap MIDI; it does not read HallJoy's physical keyboard layout automatically.
No vendor source was copied into HallJoy.

## Implemented source changes

- Remap has an Add MIDI Device action next to Add Gamepad. One MIDI setup card
  has 25 chromatic notes in a selectable range (MIDI 0..127), channel 1..16,
  a QWERTY Piano preset action, and the existing vector power action.
- The card explicitly says output is not connected yet. No virtual MIDI endpoint,
  audio generator, MIDI send worker or fixed-velocity mode is shipped at this stage.
  Exploratory runtime work stays private under .local/research/midi-skeleton-20261107.
- Drag notes to keys; existing keyboard glyph rendering, move/copy/swap and removal
  paths recognize a separate bounded MIDI action domain. Each key holds one MIDI
  note/channel assignment independently of gamepad bindings.
- Channel/range selectors affect the palette for future assignments. They do not
  transpose or rechannel existing bindings. C4 is explicitly defined as note 60.
- The Piano preset replaces MIDI assignments only after its explicit dialog. It
  maps the physical Z/S/X/D/... and Q/2/W/3/... rows to two chromatic octaves.
- Disabling MIDI preserves assignments for re-adding. Disabling the last virtual
  gamepad preserves that pad's mappings and persists the existing enabled=false
  setting. Remap shows 0/4 and no pad cards; re-add enables the retained first pad.
  Removing an earlier pad in a multi-pad setup compacts following slots.
- MIDI setup participates in BindingsSnapshot and the existing staged profile
  persistence/import/export path. Absent MIDI data in old profiles means no MIDI
  device. Malformed fields reject the complete prepared profile. Sparse writing
  clears the prior Midi section before emitting the new snapshot.
- Shared immutable MIDI snapshots let profile commit publish without allocation.
  A UI callback posts the existing Remap refresh message after commits. Palette
  controls use canonical retained PremiumCombo rendering and existing scroll and
  drag animation machinery, without extra timers or visible scrolling child HWNDs.
- MIDI setup does not enter runtime key blocking or gamepad source demand. The
  backend implementation is unchanged byte-for-byte from the pre-task backup.

## Validation

Completed:

- `python tools/run_native_backend_checks.py --require-compiler`: PASS on final
  sources, cached compilation enabled. `MIDI_SETUP_TEST=PASS` and Windows MIDI
  profile roundtrip/malformed/legacy tests PASS.
- `tools/build_release.ps1`: PASS, including complete production native catalog,
  embedded resources, production profile transactions, 89 Supported layouts,
  282 yellow notices, and startup recovery (18 scenarios plus repeated startup).
- Verified normal EXE installed at `build/bin/Release/x64/HallJoy.exe`; the build
  wrapper restored the previously running HallJoy window.
- `python tools/project_context.py --check`: PASS.

Delivery: version 1.6.15.0, 11452416 bytes, SHA-256
`cc57f271c1a87b3c63d041453eb2c54acf0c8fba38b6ee0eb95915cddc887570`.
Evidence: `.local/research/midi-skeleton-20261107/result.json`,
`native-checks-r3.log` and `build-release-r2.log` in that directory.

An initial static gate required preserving the existing onboarding shutdown/show
ordering; corrected without weakening its assertions. Initial MSVC build rejected
deprecated shared_ptr atomic free functions; replaced with C++20 atomic<shared_ptr>.
Final checks above were run after those corrections. No warning suppression added.
New portable MIDI setup coverage checks the full 2048-address domain, all 176
preset range/channel combinations, immutable snapshots, invalid binding no-ops,
independence from gamepad demand, snapshot restoration and pad-zero compaction.
Windows profile-loader coverage checks exported-bundle roundtrip, lowest/highest
notes/channels, signed octave bounds, malformed metadata rejecting live changes,
stale-entry removal and legacy profile reset.

Owner evaluates visuals; no visual run is performed by the agent. Keyboard support
statuses and the live support Sheet are unchanged. No publication is requested.

## Backup and references

Backup: .local/backups/midi-first-stage-20261107/manifest.json, with original paths,
SHA-256 hashes and scoped restoration instructions.

- Feasibility: MIDI_FEASIBILITY_2026-11-07.md.
- MIDI message definitions: https://midi.org/summary-of-midi-1-0-messages
- Wooting reference feature list: https://github.com/WootingKb/wooting-analog-midi
- Wooting reference processing: https://github.com/WootingKb/wooting-analog-midi/blob/develop/wooting-analog-midi-core/src/lib.rs

The inspected Wooting implementation declares a 100 Hz refresh and estimates
velocity from depth/time observations. That is reference behavior in the inspected
branch, not a benchmark or a universal statement about Wooting hardware/firmware.
