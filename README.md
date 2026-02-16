# HallJoy

HallJoy is a Windows desktop app that turns an analog keyboard into one or more virtual Xbox 360 controllers.

It reads per-key analog values through the Wooting Analog SDK and publishes XInput-compatible gamepads through ViGEmBus.

## Video Overview

- YouTube: https://youtu.be/MI_ZTS6UFhM?si=Cpn9DY95S9no9ncJ

## Why This Exists

I bought a DrunkDeer A75 Pro HE and wanted a native gamepad mode, but could not find one that matched what I needed.
So I built HallJoy with heavy AI assistance (ChatGPT), then kept improving it feature by feature.

I didn't write a single line of code, I'm not a programmer, even this readme file was written by chatgpt completely except for this paragraph 🙂

## Key Features

- Analog keyboard -> virtual gamepad bridge with real-time updates.
- Up to 4 virtual gamepads at once (if your game supports multi-controller binds).
- Full remap UI for sticks, triggers, ABXY, bumpers, D-pad, Start/Back/Home.
- Advanced per-key curve/deadzone tuning.
- Last Key Priority and Snap Stick options.
- Optional block of physical key output when that key is bound to gamepad input.
- Keyboard layout editor (move/add/remove keys, set labels/HID/size/position/spacing, and save/share presets as `.ini` files).
- Custom layout presets and fast switching from the app.
- Settings saved next to the executable.

## Keyboard Support

HallJoy uses:

- Wooting Analog SDK: https://github.com/WootingKb/wooting-analog-sdk
- Universal Analog Plugin: https://github.com/AnalogSense/universal-analog-plugin

That means it can work with many HE keyboards supported by that stack (not only Wooting).

If your keyboard works and you created a good layout preset, send it to me on Discord: `pash.ok`

### Aula Keyboards (Experimental)

Experimental support for Aula keyboards is currently available, but it is limited.

Right now, HallJoy can only do a best-effort emulation of a calibration-like mode (similar to how it appears in Aula web drivers). In this mode, regular keyboard input is effectively blocked while HallJoy is using the device.

Proper native Aula support is not realistically possible from the app side alone. It would require either firmware-level changes or direct help from the Aula firmware developers.

## Requirements

- Windows 10/11 (x64)
- ViGEmBus
- Wooting Analog SDK
- (Optional but recommended) Universal Analog Plugin for wider keyboard support

On missing dependencies, HallJoy can prompt to download/install them automatically.

## Build

1. Open `HallJoy.sln` in Visual Studio 2022.
2. Select `Release | x64`.
3. Build.

## Run

1. Start `HallJoy.exe`.
2. Select or create a keyboard layout.
3. Map keys to gamepad controls in the `Remap` tab.
4. Tune curves and behavior in `Configuration`.

## Troubleshooting

- If HallJoy starts normally but all analog values stay at `0`, check your keyboard firmware/software mode first.
- Some keyboards disable analog output for the Wooting SDK when `Turbo mode` (or similar performance mode) is enabled.
- Disable `Turbo mode`, then restart HallJoy and test again.
- Recent Universal Analog Plugin releases may include multiple plugin folders (`universal-analog-plugin` and `universal-analog-plugin-with-wooting-device-support`).
- For non-Wooting keyboards, use `universal-analog-plugin` (do not install both variants at the same time).
- If analog stops working after a plugin update, reinstall UAP and keep only one plugin variant in `C:\Program Files\WootingAnalogPlugins`.

### Roll Back Wooting SDK Runtime (quick)

If a newer Wooting SDK release causes unstable input/flicker, you can download and install an older runtime into this repo:

1. List available tags:
   - `powershell -ExecutionPolicy Bypass -File .\tools\rollback-wooting-sdk.ps1 -ListOnly`
2. Install a specific tag (example `v0.8.0`):
   - `powershell -ExecutionPolicy Bypass -File .\tools\rollback-wooting-sdk.ps1 -Tag v0.8.0`
3. Rebuild in VS (`Release | x64`) and run again.

The script updates DLLs in:
- `runtime\`
- `x64\Release\` (if present)
- `x64\Debug\` (if present)

It also creates automatic backups under `runtime\backup\...`.

## Config Files

Stored near the executable:

- `settings.ini` - global settings
- `bindings.ini` - key-to-gamepad bindings
- `Layouts/` - keyboard layout presets (`1 file = 1 preset`)
- `CurvePresets/` - curve preset files

## Third-Party Dependencies

- ViGEmBus: https://github.com/ViGEm/ViGEmBus
- Wooting Analog SDK: https://github.com/WootingKb/wooting-analog-sdk
- Universal Analog Plugin: https://github.com/AnalogSense/universal-analog-plugin

## License

HallJoy uses dual licensing:

- Open source: `AGPL-3.0` (see `LICENSE`)
- Commercial licensing: see `COMMERCIAL_LICENSE.md`

For commercial licensing inquiries:

- Discord: `pash.ok`
