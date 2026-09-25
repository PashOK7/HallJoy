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

Keyboards are listed in two groups: [Supported](#supported) and
[Experimental support](#experimental-support). Check both lists for your model.

See the [full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0) in Google Sheets.

### Supported

**AJAZZ:** AK820 MAX RGB (USB)  
**ATK:** Hex80  
**ATTACK SHARK:** R85 HE (USB), X65 Pro  
**AULA:** WIN 60 HE MAX, WIN 60 HE, WIN 68 HE, KP-TE153, MINI 60 HE (USB), MINI 60 HE Pro, MINI 60 HE MAX (USB)  
**DrunkDeer:** A75, A75 Pro, G60, G65, G75  
**GravaStar:** Mercury V75  
**IPI:** QBZ75, Aurora 75  
**IROK:** MG75 Max, NA87  
**Keychron:** Q1 HE, Q2 HE (ANSI), Q3 HE, Q4 HE (ANSI), Q5 HE, Q6 HE, Q12 HE, Q1 HE 8K, Q3 HE 8K, Q5 HE 8K, Q6 HE 8K, K2 HE, K3 HE, K4 HE, K6 HE (ANSI), K8 HE, K10 HE — requires compatible [custom firmware](https://analogsense.org/firmware/).  
**Lemokey:** P1 HE  
**MADLIONS:** MAD 68 Pro R, MAD60HE, MAD68HE, MAD68R  
**NuPhy:** Air60 HE, Air75 HE, Field75 HE  
**Razer:** Huntsman V2 Analog, Huntsman Mini Analog, Huntsman V3 Pro, Huntsman V3 Pro Mini, Huntsman V3 Pro Tenkeyless  
**Redragon:** K673RGB-M  
**SayoDevice:** O3C  
**Wooting:** 60HE, 60HE+, 60HE v2 (including Split), 80HE, 80HE+ (including Split), One, Two, Two HE, UwU, UwU RGB

### Experimental support

These keyboards should work, but have not yet been tested on physical devices.
Support is enabled: keyboard detection, analog input, bindings and virtual
gamepad output are implemented. HallJoy shows a yellow notice for these models.

**AIM1:** MATATAKI (US)  
**AJAZZ:** AK680 MAX HE, AK680MC, ALUX60, ALUX68 AIR, ALUX68 PRO, NS67, NS67 PRO, NS87  
**Akko:** MOD 007 V5 HE, MOD007B V3 HE, MOD007S V3 HE, Ray68 HE, TAC75 HE  
**ANGRYSHARK:** Final 75  
**ANTGAMER:** AGK75 PRO, AGK75 U2, AGK87  
**ARDOR GAMING:** Radiant  
**ASTROMEDA:** AMGK80-001  
**ATTACK SHARK:** Beat75, K85, K85 Pro HE, R68 HE, R82 HE, R82 Pro HE, R85 Ultra, R86 Pro HE, R98 GT, R98 HE, R98 Pro, R98 Ultra, X60 HE, X65, X65 HE, X68 HE, X68 MAX, X68 Pro HE, X68 Ultra, X82 HE, X82 Pro HE, X820 Pro, X85 Ultra, X87 Ultra, X96 HE, X98 HE  
**ATWO:** GK7 MX  
**AULA:** HERO 68 Air, HERO 68 HE, HERO 68 HE PRO, HERO 68 MINI, HERO 99 HE, HERO84 HE, WIN 60 HE PRO, WIN 68 HE MAX, WIN 68 HE PRO, WIN 68 HE Ultra  
**Blackstorm:** Renegade HE  
**BOYI:** H60 Pro  
**CHERRY XTRFY:** K5 Pro TMR Compact  
**Chilkey:** Slice75 HE  
**COLORFUL:** QY98 Ultra  
**DARKFORCE:** Fib(68)  
**DSPIXEL:** DS KEY, Magic 80  
**E7:** 68 PRO V2  
**EDRA:** EK368RT  
**EPOMAKER:** G84 HE, G84 HE JIS, HE108, HE60 Lite, HE60 Wired, HE60 Wireless, HE65 Mag, HE68 Lite, HE68 Mag, HE75 Mag, HE75 V2  
**EvoFox:** Ronin HS65  
**EWEADN:** DEEP68 HE, DEEP68 Pro HE, DEEP80 HE (magnetic version), DEEP80 Max HE (magnetic version), DEEP80 Pro HE (magnetic version), DK63 HE, DK63 Star HE, DK68 HE, DK68 Pro HE, DK68 Star HE, DK68 V2 HE, DK75 E HE, DK75 HE, DK75 Pro HE, DK80 HE, ES68, ES68 EVO, ES68 Lite, Gamma75 HE (EXX collaboration), SEEK75, SMART 875 HE, V99 (magnetic version), X87HE, ZAP68 HE, ZAP68 SE, ZAP68 Ultra HE, ZAP87 HE  
**FL ESPORTS:** Blend HE, D75 HE, D98 HE, FL750 (magnetic version), Flame65S, GP75 HE, GP87 HE, MK870 HE, NX108, NX68 Pro, X80 HE  
**FREEWOLF:** F68, F68 PRO  
**Fuego:** GKB904  
**Funbey:** AST V68, Coke V68  
**Fury:** Kanabo K6  
**G TUNE:** GMK82  
**GamaKay:** NS68, NS75, TK75 HE, TK75 TMR  
**Game Arena:** GKX68 MAGNUM  
**GAMEBOOSTER:** RAPID HE  
**GAMEPOWER:** Nexa HE60 1K, Tirus HE80  
**GamePro:** MK160B MAX  
**GravaStar:** Mercury V75 Lite, Mercury V75 Pro  
**HATOR:** Skyfall 65 MAG Ultima 8K Wireless, Skyfall 65 MAG Ultra 8K, Skyfall 80 MAG Ultima 8K Wireless, Skyfall 80 MAG Ultra 8K  
**HAVIT:** KB900L, KB904L  
**HAWK Gaming:** HK550, HK610S  
**IDEEZ:** SWIFT X85  
**IDJ:** H60HE  
**IPI:** AURORA65, AURORA65W, Aurora75 PRO, flash68, QBZ65, RAIN65  
**IROK:** Mars75, Mars75 Pro, Mercury68, Mercury68 Max, Mercury68 Pro, Mercury68 SE (JingTai V2), MG68 Plus, MG75 Pro, NA87 Pro, ND63, ND63 Ultra, ND68 Pro, RA68  
**IYX:** MU68 Pro  
**JEDEL:** KL166  
**JINGSU:** KA67, KB98, KCC04A, KE87  
**Keydous:** NJ68 Pro-CP, NJ80-CP V3 HE, NJ81-CP, NJ81-CP V3 HE, NJ98-CP, NJ98-CP V4 HE  
**KiiBOOM:** Cybrix29  
**Koda:** A68  
**KYSONA:** KM82 HE  
**LinkerFoo:** LF67R1  
**LOMZ:** 75S  
**M4G:** MAG 68 HE  
**MageGee:** AIR68, Captain87 JIS, MK-BOX (magnetic version)  
**MAMBASNAKE:** M82 HE, X60 HE  
**MechLands:** M75  
**MEETION:** Magic A68, Magic A75  
**MICROPACK:** K-68M  
**MonsGeek:** FUN60 Pro, FUN68 HE, FUN75, M1 V5 HE, M1 V5 TMR, M2 V5 HE, M3 V5 HE  
**MSI:** STRIKE 700 HE  
**Neo:** Neo65 Sonic HE+ (ANSI / ISO)  
**Ninjadog:** Varna Atlas  
**NOS:** C800 ALU (UK)  
**Nova Gaming:** GK505 Eon  
**Nyfter:** Nyfboard HE 61K, Nyfboard HE 82K  
**Oniverse:** Maegnus  
**OUSAID:** HG68 HE  
**PIIFOX:** DEFENDER 68, ER75 PRO  
**PSYCommu:** PSY P1  
**Rampage:** KAISEL, ZENITH PRO  
**Razer:** Tartarus Pro (requires Razer Synapse)  
**Redragon:** K617 HE, K673WB-RGB-M  
**Royal Kludge:** A72HE  
**ROYALAXE:** X68  
**SALPIDO:** SHOT209  
**SARU:** KX69HE, KX78HE  
**SAVIO:** ASTRAL  
**Skyloong:** GK61 HE, GK68 HE, GK75 HE  
**Sunsonny:** N-J100  
**Syntech:** Chronos 68  
**Titan Nation:** Storm68, TITAN60 PCB, TITAN68HE  
**UluGames:** Howl 75  
**URX:** Core68 HE  
**Valkyrie:** VK 99 Gaming (Naruto), VK Mag68, VK Mag68 Max, VK Mag75, VK Mag75 Lite, VK Mag75 Max, VK Mag75 Pro, VK NB68, VK NB68 Max  
**Veekos:** Shine60 HE  
**Womier:** M68 HE Pro, SK61 HE, SK75 TMR  
**XINMENG:** Beat65, Beat68, Beat75, X87 TMR, X98 V3 (magnetic version), Zero 68  
**YUNZII:** RT75 Pro

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
