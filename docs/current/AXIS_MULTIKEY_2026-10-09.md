# Several keys per gamepad stick direction — 2026-10-09

Owner request (from a Discord question): bind several keyboard keys to one stick
direction, so that the keys split the range, for example D = 0–25%, F = 25–50%,
G = 50–75%, and a key pressed later overrides the lower range. The owner's
design: allow several keys per axis direction; the virtual stick takes the most
pressed key; each key's own curve maps its part of the range (for example 0–50
and 50–100). Ranges come from the existing per-key curve, not from a new field.

## Behaviour

- Each stick direction (left, right, up, down) holds up to 8 keys, unique,
  kept in order. Triggers and buttons are unchanged (buttons already allowed
  many keys).
- For each direction the virtual stick uses the most pressed key, judged on the
  value after that key's curve (`configured_xusb_builder.cpp`, `ReadSide`).
  Two keys never add up.
- Per-key curves use the existing fields: `low`/`antiDeadzone` is the start
  point, `high`/`outputCap` the end point. Example for four keys:

  | Key | low | antiDeadzone | high | outputCap | Output range |
  |---|---|---|---|---|---|
  | D | 0.01 | 0.00 | 1.00 | 0.25 | 0–25% |
  | F | 0.01 | 0.25 | 1.00 | 0.50 | 25–50% |
  | G | 0.01 | 0.50 | 1.00 | 0.75 | 50–75% |
  | H | 0.01 | 0.75 | 1.00 | 1.00 | 75–100% |

  `low` must be above zero: below `low` the curve outputs 0, but at exactly 0
  it would output `antiDeadzone`, so a resting key would report it.
- Drop on a direction: replaces every key of that direction (as before).
  Shift-drop: adds the key to the direction (the same gesture that already adds
  a button action).
- Removing a key removes it from its own side only; Clear removes it everywhere.

## Code

- `bindings.h/.cpp`: `AxisBinding` holds `minusHids`/`plusHids` arrays of
  `BINDINGS_MAX_AXIS_KEYS` (8). Atomic slots per pad, axis and side.
  New API: `Bindings_AddAxis{Minus,Plus}ForPad`, `Bindings_RemoveAxis{Minus,Plus,Key}ForPad`.
  `AxisBinding::Single(minus, plus)` for one-key bindings. `minusHid()`/`plusHid()`
  return the first key.
- `configured_xusb_builder.cpp`: `ReadSide` (most pressed key).
- `backend.cpp`: every key of every direction is read and registered with the
  provider mask.
- `binding_actions.cpp`: replace vs. append; per-side removal.
- `input_trace.cpp`, `input_config_summary.cpp`, `alumix104_backend.cpp`:
  every key listed.
- `remap_panel.cpp`: repaints every key of the changed direction.
- Profiles (`profile_ini.cpp`): the first key stays in `<Axis>_Minus` and
  `<Axis>_Plus`; further keys in `<Axis>_Minus2..8` and `<Axis>_Plus2..8`.
  Earlier HallJoy versions read exactly the first key and ignore the rest.
  Duplicates and gaps are dropped on load; unused slots are written empty.
- K4 onboard (`keychron_onboard_host_profile.cpp`, `game_profile_service.h`): the
  onboard mapping holds one key per direction, so a direction with two or more
  keys is not sent to the controller; the existing notice explains it.

## Known limits

- The UI writes the slots of one direction one by one; the backend could read a
  half-updated list for a single tick. The old single-key storage had the same
  kind of window between its two halves; the effect is one frame.
- Selection is by the highest output, not by the last key pressed. Two keys
  with the same curve behave like one key at its deepest press.
- The remap panel's per-action label still shows the first key of a direction.

## Validation

- Portable tests (`run_native_backend_checks.py`, all PASS, `HALLJOY_NO_TEST_CACHE=1`):
  - `axis_multikey_bindings_test.cpp`: set/add/remove, capacity 8, compaction,
    duplicates, removal per side, clear everywhere, snapshot round trip, pad-0
    shortcuts.
  - `axis_most_pressed_builder_test.cpp`: single-key result unchanged; two keys
    take the most pressed (not the sum); equal values; fall-back after release;
    plus side; empty side.
  - Existing tests updated to the key-list types (K4 onboard host profile,
    keychron mapper, alumix session, profile loaders, extended bindings).
- Windows profile tests (inside `build_release.ps1`): extra axis keys load in
  order, duplicates dropped, legacy single-key profile restored.
- `build_release.ps1` EXIT=0 with all candidate checks and embedded licences.
- Not tested on hardware and not visually checked by the agent.

## Release status

The version string is still 1.6.8 (the published release). A tester build
with this change should get a new version number when it is released.
