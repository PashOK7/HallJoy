# HallJoy — Gamepad Mode for Analog Keyboards

HallJoy is a free, open-source Windows application that turns supported analog
and Hall Effect keyboards into a fully configurable virtual Xbox controller
with low-latency analog input. HallJoy uses real keyboard depth measurements; ordinary digital key presses 
are not converted into simulated analog travel. HallJoy includes:

- analog gamepad emulation with configurable buttons, sticks, and triggers;
- up to four virtual Xbox controllers for games that support multiple pads;
- per-binding response curves, deadzones, and sensitivity;
- last-key priority and optional Snap Stick handling for opposing directions;
- optional blocking of normal keyboard output for keys bound to gamepad input;
- global profiles and a visual layout editor with shareable `.ini` presets;
- a live analog keyboard preview and Gamepad Tester;
- a browser-based Input Overlay for OBS;
- multiple native protocols plus an embedded Universal Analog Plugin runtime;
- safe reconnect handling, automatic crash/missing-keyboard reports, and optional
  continuous diagnostic logging (off by default).

## Video overview

[Watch HallJoy on YouTube](https://youtu.be/MI_ZTS6UFhM?si=Cpn9DY95S9no9ncJ).

## Why HallJoy exists

I bought a DrunkDeer A75 Pro HE and wanted a native gamepad mode, but could not
find one that matched what I needed. So I built HallJoy with heavy ChatGPT assistance, 
then kept improving it feature by feature.

I didn't write a single line of code, I'm not a programmer, even this readme
file was written by chatgpt completely except for this paragraph 🙂

## Requirements

- Windows 10 or Windows 11, x64;
- an analog keyboard supported by HallJoy or one of its safe protocol routes;
- ViGEmBus 1.22.0 for the virtual Xbox controllers.

## Quick start

1. Download `HallJoy.exe` from [GitHub Releases](https://github.com/PashOK7/HallJoy/releases)
   and run it. No archive extraction is needed.
2. Run HallJoy. If ViGEmBus is missing, choose **Install ViGEmBus 1.22.0**.
   The installer is included in HallJoy. Approve the Windows permission prompt
   and follow the setup instructions.
3. Close any connected keyboard web-driver tabs; they commonly conflict with
   HallJoy. Desktop software such as Razer Synapse does not necessarily conflict
   and can stay open if everything works.
4. Leave **Automatic layout** enabled to select a recognized keyboard and apply
   supported device remaps. If detection is unavailable or multiple supported
   keyboards are connected, select a layout manually. Manual layouts use factory
   key assignments.
5. Assign controls on the **Remap** tab, tune curves and behavior in
   **Configuration**, and verify the result in **Gamepad Tester**.

Wooting Analog SDK and Universal Analog Plugin do not need to be installed separately.

## Compatible keyboards for gamepad mode

|  |
|---|
| ATK Hex80 |
| ATTACK SHARK X65 Pro |
| AULA WIN 60 HE MAX, AULA WIN 60 HE, AULA WIN 68 HE, AULA KP-TE153, AULA MINI 60 HE (USB), AULA MINI 60 HE Pro, AULA MINI 60 HE MAX (USB) |
| DrunkDeer A75, DrunkDeer A75 Pro, DrunkDeer G60, DrunkDeer G65, DrunkDeer G75 |
| GravaStar Mercury V75 |
| IPI QBZ75, IPI Aurora 75 |
| IROK MG75 Max, IROK NA87 |
| Keychron Q1 HE, Keychron Q2 HE (ANSI), Keychron Q3 HE, Keychron Q4 HE (ANSI), Keychron Q5 HE, Keychron Q6 HE, Keychron Q12 HE, Keychron Q1 HE 8K, Keychron Q3 HE 8K, Keychron Q5 HE 8K, Keychron Q6 HE 8K, Keychron K2 HE, Keychron K3 HE, Keychron K4 HE, Keychron K6 HE (ANSI), Keychron K8 HE, Keychron K10 HE |
| Lemokey P1 HE |
| MADLIONS MAD 68 Pro R, MADLIONS MAD60HE, MADLIONS MAD68HE, MADLIONS MAD68R |
| NuPhy Air60 HE, NuPhy Air75 HE, NuPhy Field75 HE |
| Razer Huntsman V2 Analog, Razer Huntsman Mini Analog, Razer Huntsman V3 Pro, Razer Huntsman V3 Pro Mini, Razer Huntsman V3 Pro Tenkeyless |
| Redragon K673RGB-M |
| SayoDevice O3C |
| Wooting 60HE, Wooting 60HE+, Wooting 60HE v2 (including Split), Wooting 80HE, Wooting 80HE+ (including Split), Wooting One, Wooting Two, Wooting Two HE, Wooting UwU, Wooting UwU RGB |

Keychron HE requires compatible [custom firmware](https://analogsense.org/firmware/).
Not every hardware revision has been tested.

### Experimental support 🟨

These keyboards should work, but have not yet been tested on physical devices.
Support is enabled: keyboard detection, analog input, bindings and virtual
gamepad output are implemented. HallJoy shows a yellow notice for these models.
Use a wired USB connection.

|  |
|---|
| ATTACK SHARK Beat75, ATTACK SHARK K85, ATTACK SHARK K85 Pro HE, ATTACK SHARK R68 HE, ATTACK SHARK R82 HE, ATTACK SHARK R82 Pro HE, ATTACK SHARK R85 HE, ATTACK SHARK R85 Ultra, ATTACK SHARK R86 Pro HE, ATTACK SHARK R98 GT, ATTACK SHARK R98 HE, ATTACK SHARK R98 Pro, ATTACK SHARK R98 Ultra, ATTACK SHARK X60 HE, ATTACK SHARK X65, ATTACK SHARK X65 HE, ATTACK SHARK X68 HE, ATTACK SHARK X68 MAX, ATTACK SHARK X68 Pro HE, ATTACK SHARK X68 Ultra, ATTACK SHARK X82 HE, ATTACK SHARK X82 Pro HE, ATTACK SHARK X820 Pro, ATTACK SHARK X85 Ultra, ATTACK SHARK X87 Ultra, ATTACK SHARK X96 HE, ATTACK SHARK X98 HE |
| AULA HERO 68 Air, AULA HERO 68 HE, AULA HERO 68 HE PRO, AULA HERO 68 MINI, AULA HERO 99 HE, AULA HERO84 HE, AULA WIN 60 HE PRO, AULA WIN 68 HE MAX, AULA WIN 68 HE PRO, AULA WIN 68 HE Ultra |
| GravaStar Mercury V75 Lite, GravaStar Mercury V75 Pro |
| IPI AURORA65, IPI AURORA65W, IPI Aurora75 PRO, IPI flash68, IPI QBZ65, IPI RAIN65 |
| IROK MG75 Pro |
| Redragon K673WB-RGB-M |

## Support

Have a problem or want support for another keyboard? Write to me on
[HallJoy Discord](https://discord.gg/5FQ297yZh).

## Input Overlay for OBS

Input Overlay renders the current keyboard layout and real HE key travel on a
transparent browser canvas. It is served locally on `127.0.0.1` and does not
publish your input to the internet.

To add it to OBS:

1. Open the **Input Overlay** tab in HallJoy.
2. Click **Start server**.
3. Copy the address using the URL field.
4. Add a **Browser** source in OBS and paste the copied URL.
5. Set the Browser Source size to match the proportions of your keyboard layout.

You can configure the fill direction, choose raw key travel or the value after
curves, change indicator and label colors, select the label font, size and
shadow, and adjust the refresh interval. Available visual effects include
Smooth response, Glass keys, Bloom, Edge sweep, Micro-scale, Label contrast,
and Rim lighting.

## Keyboard layout editor

The visual layout editor can move, add, and delete keys; change labels, HID
usages, dimensions, positions, and spacing; and save the result as a preset.
Each preset is a standalone `.ini` file, so layouts can be backed up or shared
with other HallJoy users. Layout selection is independent from the hardware
protocol used to obtain analog values.

## Saved data and portable mode

HallJoy stores user data under `%LOCALAPPDATA%\HallJoy` by default:

- `settings.ini` contains the main settings and Default profile, including bindings;
- `GlobalProfiles\` contains additional profile settings and bindings;
- `Layouts\` contains saved custom or edited keyboard layouts; built-in layouts
  are embedded in HallJoy and do not need to be written here on startup;
- `CurvePresets\` contains response-curve presets.

To use an intentionally portable installation, create an empty
`HallJoy.portable` file beside `HallJoy.exe` in a writable directory before the
first start. HallJoy will then keep its data beside the executable. Without that
marker, writable state remains in `%LOCALAPPDATA%` and is not mixed with program
files.

## Troubleshooting

If HallJoy starts but every analog value remains at zero:

1. Close connected web-driver tabs. If input is still missing, temporarily close
   the desktop configurator to check for a conflict; closing it is not normally
   required when HallJoy already works.
2. Check the keyboard firmware and software mode. On some keyboards a vendor
   **Turbo**, performance, or compatibility mode can stop exposing the analog
   interface used by external applications.
3. Reconnect the keyboard, restart HallJoy, and inspect **Configuration** and
   **Gamepad Tester** for the detected route and live values.
4. If the problem persists, see [Support](#support).

Do not manually install Wooting Analog SDK or Universal Analog Plugin as a
troubleshooting step. HallJoy verifies and prepares its own private plugin
runtime.

## License

HallJoy uses dual licensing:

- open-source use is available under [AGPL-3.0](LICENSE);
- a separate [commercial license](docs/legal/COMMERCIAL_LICENSE.md) is available for
  proprietary distribution, embedding, or licensing without AGPL obligations.

Commercial licensing inquiries: Discord **`pash.ok`**. Third-party components
and their licenses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Building from source

Install Visual Studio 2022 C++ Build Tools (including x64 Clang), Python 3.12,
Git and PowerShell. Clone the repository and run `BUILD.cmd`.
The executable is created at `build\bin\Release\x64\HallJoy.exe`.
See the [build guide](docs/development/BUILD_README.txt) for incremental builds
and the staged replacement of a running HallJoy.
