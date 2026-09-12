
HallJoy turns an analogue Hall Effect keyboard into a fully configurable virtual
Xbox controller with low-latency analogue input. HallJoy includes:

- keyboard-to-gamepad remapping for buttons, sticks, and triggers;
- up to four virtual Xbox controllers for games that support multiple pads;
- per-binding response curves, deadzones, and sensitivity;
- last-key priority and optional Snap Stick handling for opposing directions;
- optional blocking of normal keyboard output for keys bound to gamepad input;
- global profiles and a visual layout editor with shareable `.ini` presets;
- a live analogue keyboard preview and Gamepad Tester;
- a browser-based Input Overlay for OBS;
- multiple native protocols plus an embedded Universal Analog Plugin runtime;
- safe reconnect handling, automatic crash/missing-keyboard reports, and optional
  continuous diagnostic logging (off by default).

## Video overview

[Watch HallJoy on YouTube](https://youtu.be/MI_ZTS6UFhM?si=Cpn9DY95S9no9ncJ).

## Why HallJoy exists

I bought a DrunkDeer A75 Pro HE and wanted a native gamepad mode, but could not
find one that matched what I needed. So I built HallJoy with heavy AI assistance
(ChatGPT), then kept improving it feature by feature.

I didn't write a single line of code, I'm not a programmer, even this readme
file was written by chatgpt completely except for this paragraph 🙂

## Requirements

- Windows 10 or Windows 11, x64;
- an analogue keyboard supported by HallJoy or one of its safe protocol routes;
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
4. Select or create the matching keyboard layout.
5. Assign controls on the **Remap** tab, tune curves and behavior in
   **Configuration**, and verify the result in **Gamepad Tester**.

Wooting Analog SDK and Universal Analog Plugin do not need to be installed separately.

## Compatible keyboards

If your model is not listed, that does not necessarily mean it is unsupported.
Some other models may work, but compatibility is not guaranteed.

### Supported models

| Brand | Models | Notes |
|---|---|---|
| Aula | **WIN 60 HE MAX**, **WIN 60 HE**, **WIN 68 HE**, **KP-TE153** | — |
| Irok | **MG75 Max**, **MG75 Pro** | **MG75 v2 is not supported.** |
| Redragon | **K673RGB-M (BR firmware)** — confirmed working | Other compatible magnetic-switch models are expected to work but have not been physically tested with HallJoy. |
| MADLIONS | **MAD 68 Pro R**, **MAD60HE**, **MAD68HE**, **MAD68R** | — |
| ATK | **Hex80** | — |
| SayoDevice | **O3C** | O3C is tested; other SayoDevice models may also work but are not confirmed. |
| IPI / QBZ | **QBZ75** | — |
| Razer | **Huntsman V2 Analog**, **Huntsman Mini Analog**, **Huntsman V3 Pro**, **Huntsman V3 Pro Mini**, **Huntsman V3 Pro Tenkeyless** | — |
| Keychron | **Q1 HE**, **Q3 HE**, **Q5 HE**, **K2 HE**, **K4 HE ANSI** | Requires [custom firmware](https://analogsense.org/firmware/). Ready-made images are available for some models. For other Keychron HE models, the linked source changes can be applied to Keychron's published firmware source and built for the exact model and ANSI/ISO/JIS variant. |
| Lemokey | **P1 HE ANSI**, **P1 HE ISO** | — |
| NuPhy | **Air60 HE**, **Air75 HE** | — |
| DrunkDeer | **A75**, **A75 Pro**, **G60**, **G65**, **G75** | — |
| Wooting | Analogue Wooting keyboards supported by the bundled runtime | — |

Not every listed model or hardware revision has been physically tested with HallJoy.

## Support

Join [HallJoy Discord](https://discord.gg/5FQ297yZh) for help, keyboard support
requests, feedback, and updates. Include your keyboard model, HallJoy version,
and a short description of the problem.

Choose **Open HallJoy folder** in Global settings to find `HallJoy.log`.
Attach it to your report; do not upload your entire data folder or personal text.

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
and Rim lighting. The overlay does not redraw an unchanged frame while idle.
Smooth response defaults to 15%.

## Keyboard layout editor

The visual layout editor can move, add, and delete keys; change labels, HID
usages, dimensions, positions, and spacing; and save the result as a preset.
Each preset is a standalone `.ini` file, so layouts can be backed up or shared
with other HallJoy users. Layout selection is independent from the hardware
protocol used to obtain analogue values.

## Saved data and portable mode

HallJoy stores user data under `%LOCALAPPDATA%\HallJoy` by default:

- `settings.ini` contains the main settings and Default profile, including bindings;
- `GlobalProfiles\` contains additional profile settings and bindings;
- `Layouts\` contains keyboard layout presets;
- `CurvePresets\` contains response-curve presets.

To use an intentionally portable installation, create an empty
`HallJoy.portable` file beside `HallJoy.exe` in a writable directory before the
first start. HallJoy will then keep its data beside the executable. Without that
marker, writable state remains in `%LOCALAPPDATA%` and is not mixed with program
files.

## Troubleshooting

If HallJoy starts but every analogue value remains at zero:

1. Close connected web-driver tabs. If input is still missing, temporarily close
   the desktop configurator to check for a conflict; closing it is not normally
   required when HallJoy already works.
2. Check the keyboard firmware and software mode. On some keyboards a vendor
   **Turbo**, performance, or compatibility mode can stop exposing the analogue
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
- a separate [commercial license](COMMERCIAL_LICENSE.md) is available for
  proprietary distribution, embedding, or licensing without AGPL obligations.

Commercial licensing inquiries: Discord **`pash.ok`**. Third-party components
and their licenses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Building from source

Install Visual Studio 2022 C++ Build Tools (including x64 Clang), Python 3.12,
Git and PowerShell. Clone the repository and run `BUILD.cmd`.
The executable is created at `build\release\HallJoy.exe`.
