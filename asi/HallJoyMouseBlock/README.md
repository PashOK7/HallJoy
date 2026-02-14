# HallJoyMouseBlock ASI

Simple in-process ASI helper that reads HallJoy mouse-block state via shared memory and suppresses mouse input in game window.

## Purpose

- HallJoy can already block normal Windows mouse events.
- Some games still read mouse in-process.
- This ASI runs inside the game and blocks `WM_MOUSE*` / `WM_INPUT` when HallJoy says `Block Mouse Input` is enabled.

## Build

1. Create a new `DLL` project in Visual Studio (x64).
2. Add `HallJoyMouseBlock.cpp` to the project.
3. Build Release x64.
4. Rename output to `.asi` if your loader expects `.asi`.

## Use

1. Install an ASI loader for the game (for Mafia 2 use common ASI loader setup).
2. Put `HallJoyMouseBlock.asi` into the game directory / ASI plugins directory.
3. Run HallJoy.
4. In HallJoy enable:
   - `Mouse to Stick: ON`
   - `Block Mouse Input: ON`

When `Right Shift` pause is active in HallJoy, ASI also stops blocking mouse.

## Notes

- This is a generic best-effort blocker.
- Some games may need deeper API hooks (DirectInput/RawInput internals) if they bypass window messages completely.

