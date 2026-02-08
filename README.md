# HallJoy

HallJoy is a Windows desktop app that turns an analog keyboard into one or more virtual Xbox 360 controllers.

It reads per-key analog values through the Wooting Analog SDK and publishes XInput-compatible gamepads through ViGEmBus.

## Video Overview

- YouTube: https://youtu.be/MI_ZTS6UFhM?si=Cpn9DY95S9no9ncJ

## Why This Exists

I bought a DrunkDeer A75 Pro HE and wanted a native gamepad mode, but could not find one that matched what I needed.
So I built HallJoy with heavy AI assistance (ChatGPT), then kept improving it feature by feature.

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
