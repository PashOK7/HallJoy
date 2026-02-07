# HallJoy

This project is entirely made using AI, including this README.md, I am not a programmer and did not write a single line of code🙂

HallJoy is a Windows desktop app that turns an analog keyboard into a virtual Xbox 360-style gamepad. It reads per-key analog values through the Wooting Analog SDK and publishes an XInput-compatible controller via ViGEm, making analog movement possible in games that expect a gamepad.

## Features

- **Analog keyboard → virtual gamepad** bridge with real-time input updates.
- **Per-key tuning** with configurable deadzones and response curves.
- **Curve presets** you can save and reuse across sessions.
- **Remapping UI** for common gamepad controls (ABXY, bumpers, triggers, sticks, D-pad, guide).
- **Persistent settings** stored alongside the executable (INI files).

## Requirements

- **Windows 10/11** (x64).
- **Visual Studio 2022** (or compatible) with C++ build tools.
- **ViGEmBus driver** installed and running.
- **Wooting Analog SDK** (the project links against the Wooting Analog Wrapper).
- A **supported analog keyboard** (e.g., Wooting).

## Build

1. Open `HallJoy.sln` in Visual Studio.
2. Select **Release | x64**.
3. Build the solution.

The project copies `wooting_analog_wrapper.dll` into the output folder automatically during the build.

## Run

1. Ensure the **ViGEmBus** driver is installed and active.
2. Plug in your analog keyboard.
3. Launch the built `HallJoy.exe` from the output folder.

## Configuration & Files

HallJoy stores configuration near the executable:

- `settings.ini` — global input settings (deadzones, curves, etc.).
- `bindings.ini` — key-to-gamepad bindings.

Curve presets are stored in one of two locations (first writable wins):

1. `CurvePresets/` next to the executable.
2. `%LOCALAPPDATA%\HallJoy\CurvePresets`.

## Project Structure (high level)

- `HallJoy/` — main application source (UI, backend, bindings, curve math).
- `runtime/` — runtime dependencies copied to the build output.
- `third_party/` — Wooting Analog Wrapper and ViGEm client libraries.

## Troubleshooting

- **“Failed to init backend (Wooting/ViGEm)”**
  - Verify the ViGEmBus driver is installed.
  - Ensure `wooting_analog_wrapper.dll` is present next to the executable.
  - Confirm your keyboard supports Wooting’s analog API.

- **No analog input detected**
  - Check that the Wooting Analog SDK is installed and up to date.
  - Try reconnecting the keyboard and restarting HallJoy.

## Third-Party Dependencies

- [ViGEm](https://github.com/ViGEm/ViGEmBus) — virtual gamepad driver.
- [Wooting Analog SDK](https://github.com/WootingKb/wooting-analog-sdk) — analog keyboard input.
