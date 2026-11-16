# Absolute MIDI velocity curve

All five points have independent horizontal speed and vertical velocity
coordinates. Drag either way or click the numeric fields below the graph.
The Start/End and numbered point buttons select a handle even when points
appear close together. Speeds use full key travels per second with three
decimal places. Velocity uses the MIDI range from 0 to 127.

Below Start the note is silent. At Start the curve begins at the selected
velocity. Above End output stays at the End velocity. Velocity 0 suppresses
the note and its aftertouch contribution. Selecting and moving one point
changes only that point. Speed remains strictly ordered and velocity remains
nondecreasing.

The X-axis limit is a view setting. It can be smaller than the final point.
The graph clips at the entered speed. Show full curve expands the view to
End and keeps the saved MIDI response. New profiles and Reset curve use a
0–70 travels/s view and a linear curve with the five points at 0, 17.5, 35,
52.5 and 70. All five handles can be clicked on the plot, including End on
the right edge. The factory response reaches velocity 127 at 70 travels/s.
Existing profiles keep their saved curve and axis limit. Previously valid
partial v2 curve records keep their previous missing-coordinate defaults.
Profiles created before MIDI with no MIDI section receive the factory curve
when loaded. Profile storage covers the estimator's full speed range.
The last generated velocity and measured speed are always shown after a
strike. The blue marker is drawn only when that speed is on the visible axis.
Playing effort, the touch preset buttons and matching to the last strike
are removed from the editor. Reset curve restores the new linear default.

Version 1 profiles keep their original response until the first curve edit.
Editing converts the five legacy points to absolute coordinates. Discrete
rounding can change a velocity by one step on a converted portion. Saving
version 2 records each point and the view limit explicitly. Note bindings,
output port and aftertouch remain part of the same profile.

The depth controls below the graph set note start and release independently.
An edit clamps against the other depth marker and noise threshold.

The native suite passed absolute interpolation, dead zones, legacy conversion
and profile data roundtrip. The production simulator passed preview and commit
on both axes, endpoint edits, numeric axis entry, retained rendering while
scrolling and preservation of routing and bindings. The release wrapper passed
all 12 exact-image tester checks and installed the portable EXE.
Visual and physical acceptance are pending with the owner.

The ordinary delivered build is `build/bin/Release/x64/HallJoy.exe`
(11,963,904 bytes, SHA256 `8909f3f9861c65cba39c1c19576ce3e0ac85d71b27a778cddc08bbabe4411dc4`). Candidate and delivery match.
Verification logs are `.local/midi-static-curve-native-20261115.log` and
`.local/midi-static-curve-build-retry-20261115.log`.

## Numeric field feedback

Clicking any numeric field opens a temporary Windows text editor and places
the caret at the click. It supports partial selection, arrows and clipboard
editing. Enter or leaving the field commits a valid edit. Esc cancels it.
The interaction test exercises partial replacement, cancel, focus loss and
Enter. It also verifies that a lower axis limit preserves the full MIDI curve.

Native, production UI and release checks passed. The installed portable build
is 11,963,904 bytes, SHA256 `3150e500c19f44b56d33561a42e475ba6f5b8bab671ac8d7f4eba4b7c10e35c3`. Its bytes match the tested candidate.

## Default 70 travels/s view

The native profile loader verifies the 70 travels/s fallback and round-trips a
custom limit. The production MIDI page interaction test verifies Reset curve
returns the view to 70. Both suites passed. The installed portable EXE is
11,963,904 bytes, SHA256 `ae7252713879d8387c2bd22fe4a526b1ed0f7414e503db91aaad052b855ae3f7`, matching the candidate.
Verification logs are `.local/midi-default70-native-20261115.log` and
`.local/midi-default70-build-20261115.log`. K4 timing evidence is recorded in
[MIDI interval selection](MIDI_TERMINAL_VELOCITY_2026-11-15.md).

## Factory curve fits the default view

The previous build paired the 70 travels/s axis with fifth and fourth points
at 100 and 75. The production page test now clicks all five factory handles
directly, tests the old 70-axis/100-End combination with Show full curve, and
clicks all five handles again after Reset curve. It also drags End left and
down from the right edge. Numeric edit focus, scroll and route checks still
pass. The native suite checks 127 at 70, half-scale velocity at 35, stored zoom,
previously valid partial v2 coordinates and profile loading without MIDI data.

The native suite and ordinary release build passed. The installed
`build/bin/Release/x64/HallJoy.exe` is 11,964,928 bytes, SHA256
`932332e3594726d4a221d5be472f84e836fa0d2ac37b1dd1e49c6be01b92494a`, byte-identical to the checked candidate. Logs are
`.local/midi-curve-fit-native-20261115.log` and `.local/midi-curve-fit-build-20261115.log`.
Source backup: `.local/backups/midi-curve-fit-20261115/manifest.json`.
