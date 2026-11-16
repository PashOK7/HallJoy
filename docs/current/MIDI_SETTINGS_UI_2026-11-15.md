# MIDI settings tab

[Current absolute velocity curve](MIDI_ABSOLUTE_VELOCITY_CURVE_2026-11-15.md).

This page records the initial MIDI settings design.

The initial M2 form crowded the Remap palette with numerical choices. The
settings now live in a separate tab, available after Add MIDI Device and from
Touch settings in the MIDI card. Profiles that already enable MIDI show the tab
when loaded. Disabling MIDI removes it. Remap owns the port, channel, note range,
Piano preset and drag-to-bind palette.

Three touch presets and a playing-effort slider adjust how quickly the output
reaches maximum velocity. The velocity graph has three editable points and a
marker for the latest strike. That marker previews the current curve applied to
the last observed speed. MIDI Tester still shows the velocity generated for the
actual note. Sparse observations are labelled on the settings page. Match my
strongest press uses the latest stroke with a usable speed measurement.

The depth bar shows the deepest bound key in the keyboard preview. The purple
marker is the note start and the blue marker is the release point. Aftertouch
can be toggled on this page. Advanced settings contains noise filtering and the
numeric speed reference. Reset restores touch defaults while preserving output
and bindings. The settings use the existing profile fields and validation.

The tab follows the shared retained viewport and tab transition. The current
curve and live depth overlay update at the existing UI cadence while visible.
Dragging only edits a local preview until mouse release. A profile switch or
capture loss cancels the preview. No background work is added for disabled MIDI.

## Verification

The native backend suite passed with normal caching. The production-linked
simulator passed gesture preview and commit, threshold validation, scrolled hit
testing, retained-cache reuse, and preservation of the selected route and note
bindings. The full catalog, candidate delivery, embedded resources, profile and
layout tests and startup recovery checks passed. The existing Shark test returned
20 once, then 0 on the identical candidate. The remaining release checks resumed
for that image and the ordinary publisher installed it. Visual acceptance remains
with the owner.

`build/bin/Release/x64/HallJoy.exe`, version 1.6.15.0, 11917312 bytes.
SHA-256 `8846280c76e4f95a51e31f23c48409a7dbdfb6a06f85da9bbab9dbfd7667fa4e`.
Logs are `.local/midi-ui-native.log`, `.local/midi-ui-build-retry.log` and
`.local/midi-ui-build-resume.log`.

Pre-change backup: `.local/backups/midi-ui-20261115/manifest.json`.
