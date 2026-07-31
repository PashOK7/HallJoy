# HallJoy

> **Madlions V6 SafeHID branch**
>
> This archive is a complete MSVC build tree for the Madlions root-cause fix.
> The old overlapping Windows HID request path is replaced with a serial,
> event-per-operation SafeHID state machine. The private plugin remains inside a
> supervised child process as an independent containment boundary.
>
> Build:
>
> ```powershell
> powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_madlions_diagnostic.ps1
> ```
>
> Tester package:
> `x64\MadlionsDiagnostic\SEND_TO_MADLIONS_TESTER\`

HallJoy is a Windows desktop app that turns an analog keyboard into one or more
virtual Xbox 360 controllers.

This V6 branch calls a pinned, patched Universal Analog Plugin ABI1 directly in
an isolated child process. The Wooting SDK dynamic layer and the system plugin
folder are not used. ViGEmBus remains the virtual-gamepad backend.

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

### Addressed Hall-effect protocol

HallJoy includes a native read-only backend for keyboards that expose the verified `FF60:0061` 64-byte addressed analogue protocol. Compatibility is determined by a capability probe, not by brand or VID/PID. The backend requests up to nine key positions per packet, prioritises bound and currently active keys, and keeps a guaranteed background sweep over the remaining matrix.

The obsolete single-last-key diagnostic backend has been removed. Devices that do not pass the addressed capability probe are left untouched.

### ATK × QK Hex80

HallJoy includes a separate native backend for the documented Hex80 vendor HID protocol (`VID 0x373B`, PIDs `0x1176/0x1177/0x1250`, usage `0xFF60:0x0061`). It validates read-only travel-info and matrix responses before claiming the device from UAP, polls 104 matrix slots in four-slot chunks, normalizes the device travel scale, and publishes 82 standard HID keys through the common curve/SOCD/ViGEm pipeline. The calibration-start command is never used.

### SparkLink PCB Keyboards

HallJoy includes a native HID path for keyboards that expose the SparkLink/XD protocol on vendor usage pages `0xFFB0` or `0xFFA0`.

This path is separate from the Wooting Analog SDK. When a supported SparkLink device is detected, HallJoy reads the keyboard layout and per-row analog route data directly from HID, then feeds those values into the normal remap and curve pipeline.

This support is intentionally limited to the SparkLink/XD protocol. The old MG75 v2 probing path is not part of the runtime because that keyboard revision uses a different MCU/protocol family.

SparkLink support has been tested on Irok MG75 Max.

### SayoDevice OSU O3C

HallJoy includes native HID support for SayoDevice OSU O3C (`VID 0x8089`, `PID 0x0009`).

The SayoDevice path reads the device's realtime depth polling stream directly and maps the three physical buttons to their current keyboard HID outputs, so it can follow user-configured key bindings instead of assuming fixed letters.

## Requirements for this V6 SafeHID branch

- Windows 10/11 (x64)
- ViGEmBus

The plugin is embedded. Wooting Analog SDK and a system-wide Universal Analog Plugin installation are not required for this build. Nothing is written to Program Files.

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
