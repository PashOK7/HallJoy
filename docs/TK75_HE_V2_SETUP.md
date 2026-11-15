# GamaKay TK75 HE V2: USB setup and functional check

This guide uses the existing HallJoy implementation. It does not change keyboard
support, prescribe new firmware or claim a physical test of your keyboard.

## Identify the regional variant

The retail model name and USB product string alone do not identify the layout.
HallJoy's [recorded investigation](current/GAMAKAY_2026-10-23.md) distinguishes:

| Board | Recorded variant | Existing HallJoy status and layout |
| --- | --- | --- |
| 3167 | US / ANSI | Supported over USB; built-in `GamaKay TK75 HE + TK75 HE V2 ANSI` geometry. |
| 3138 | UK / ISO | Experimental over USB; implemented analog route, manual layout until exact geometry is verified. |

Use **Open log** to check the board identity when it is present in the log; the
RongYuan connection record identifies the board as 3138 or 3167. Keep the physical
key arrangement in view. Do not select the ANSI preset solely to remove an ISO
layout limitation. A generic layout is not proof that every displayed key matches
the physical board. The yellow notice for 3138 is expected under current support.

## Establish a baseline

1. Connect the keyboard by USB and close connected keyboard web-configurator tabs.
   This guide does not establish Bluetooth or 2.4 GHz support.
2. Run one intended HallJoy copy. A source build and the personal portable copy
   can be different versions; [Start.cmd uses `.local/portable`](PORTABLE.md),
   while an ordinary source build produces `build/bin/Release/x64/HallJoy.exe`.
3. Confirm HallJoy detects the keyboard. If the virtual controller driver is
   missing, use HallJoy's included **Install ViGEmBus 1.22.0** action, as described
   in the [quick start](../README.md#quick-start).
4. Preserve your current configuration using the [backup guide](PORTABLE.md).
   Use a separate test profile before changing bindings, curves or blocking.
5. For 3167, leave **Automatic layout** enabled and verify the selected preset.
   For 3138, turn **Automatic layout** off, then select or edit a manual layout
   matching the actual ISO keyboard and verify every intended binding. Manual
   layouts use factory assignments and ignore device remaps, so verify keys
   previously changed in the keyboard's configurator.
6. Use the intended bindings on **Remap**, a simple response curve in
   **Configuration**, and **Gamepad Tester** to inspect the output. The
   [driving example](DRIVING_SETUP.md) maps A/D to left-stick X and W/S to RT/LT.

## Check behavior before tuning

| Action | What to observe |
| --- | --- |
| Slowly press one bound key from rest, then release | Intermediate values change with actual travel; the virtual control returns to neutral after release. |
| Hold a partly pressed key steady | The output remains usable at that depth; record any visible jitter rather than assuming a deadzone value. |
| Press steering and throttle together, then release both | Both controls respond independently and return to neutral without residual input. |
| Press opposite steering directions | Behavior matches the configured last-key priority / Snap Stick settings; do not assume every profile uses the same policy. |
| Pause and Resume | The controller does not retain an unintended held value; fresh input works after Resume. |
| Unplug and reconnect USB while testing | Output becomes neutral when the keyboard is unavailable and fresh input works after reconnect. |
| Test normal typing with bound-key blocking off, then with your intended blocking policy | Only intended bound keys are blocked; normal typing can be restored. |

If output is reversed, inspect global inversion and any per-key overrides before
changing sensitivity or deadzones. The control is labeled **Invert axis (press
to release)**. Record the original values and change one setting at a time. An
existing inversion setting may be intentional.

Tune deadzones from observed rest noise and attainable full-press output. The
protocol's 4 mm scale is nominal; it is not a measurement of the installed switch.
A Magnetic Phoenix V3 switch name alone does not establish suitable deadzones or
the measured travel of an individual keyboard. Keep firmware actuation settings
and HallJoy gamepad response separate when comparing changes.

## Report a result

Report the regional variant, connection, HallJoy version and which checks above
passed or failed. Use **Open log** for the normal `HallJoy.log` if investigation is
needed. Continuous logging remains optional; do not collect unrelated typing.
No extra scripts, diagnostic archive or research export is needed.

A successful analog connection alone does not verify the complete layout,
simultaneous controls, release behavior or game compatibility. Any support-status
change follows the project's [support synchronization process](development/SUPPORT_STATUS_SYNC.md).
