# Example driving bindings

This is a setup guide, not a ready-made game profile or a claim of testing every
game, keyboard layout or firmware. Start with a keyboard supported by HallJoy and
verify the actual input in Gamepad Tester before tuning a game's settings.

| Key | Virtual control | Example use |
| --- | --- | --- |
| A | Left stick X, negative direction | Steer left |
| D | Left stick X, positive direction | Steer right |
| W | Right trigger (RT) | Throttle |
| S | Left trigger (LT) | Brake |

RT and LT are the right and left analog triggers. Left-stick directions are
different from clicking the stick (L3); use the directional bindings for steering.
The game's controller settings determine the final action; brake/reverse behavior
and available assists depend on the game and its configuration.

1. Connect the keyboard through its supported connection. Close conflicting
   keyboard web-configurators, then check the detected device and layout.
2. Create a separate driving profile. On Remap, assign the controls above to the
   matching physical keys. Keep your existing profile as a fallback.
3. Begin with a simple response curve. In Gamepad Tester, press each key slowly:
   check intermediate values, full travel and return to neutral after release.
4. Adjust deadzones based on observed travel/noise; there are no universal values
   for all switches, keyboards and games.
5. If normal keyboard input duplicates gamepad actions, configure blocking only
   for the intended bound keys. Verify you can still return to normal typing.
6. Configure the game's Xbox-controller bindings, steering response and assists.
   Test steering, throttle and braking separately, then together.
7. Close HallJoy normally so it saves settings, then back up the profile using
   the [data-directory guide](PORTABLE.md).

Signal path: physical key travel → HallJoy binding/curve → virtual Xbox controller
through ViGEmBus → game. Example games include Forza Horizon, but this guide does
not prescribe game-specific menu positions or promise compatibility.
