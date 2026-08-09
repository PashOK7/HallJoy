# HallJoy

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
- safe reconnect handling and crash-only production diagnostics.

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
   or build it from source.
2. Run HallJoy. If ViGEmBus is missing, HallJoy will show the pinned official
   [ViGEmBus 1.22.0 release](https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0)
   and installation instructions. Install it once, then restart HallJoy.
3. Close the keyboard's official or web configurator so it does not retain the
   vendor HID interface.
4. Select or create the matching keyboard layout.
5. Assign controls on the **Remap** tab, tune curves and behavior in
   **Configuration**, and verify the result in **Gamepad Tester**.

You do not need to install the Wooting Analog SDK or Universal Analog Plugin.
HallJoy carries its own verified private Universal Analog Plugin runtime and
prepares it automatically without UAC. ViGEmBus is the only system-wide runtime
dependency and is intentionally installed manually from its official release.

## Compatible keyboards

If your model is not listed, that does not necessarily mean it is unsupported.
HallJoy can identify some compatible devices from live protocol responses rather
than a fixed model or PID table.

### Native HallJoy support

These devices use HallJoy's native protocol backends where noted. Some models
in a grouped brand row may use HallJoy's embedded Universal Analog Plugin
runtime instead.

| Brand | Models | Route and status |
|---|---|---|
| Aula | **Aula WIN 60 HE MAX**, **Aula WIN 60 HE**, **Aula WIN 68 HE**, **KP-TE153**, and compatible siblings | MAX uses the dynamic 6×21 backend. Standard/W669 reads the firmware product identity and selects an official 61-, 68-, or 69-key factory profile before applying the keyboard's live remap records. WIN 68 HE and KP-TE153 profiles are official-driver-derived but not yet physically validated by HallJoy. |
| Irok/SparkLink | **Irok MG75 Max**, **Irok MG75 Pro**, and protocol-compatible models | MG75 Max is physically validated with sustained polling and reconnect tests. Not every Irok keyboard is compatible: **Irok MG75 v2 was physically tested and is not supported**. |
| MADLIONS | **MAD 68 Pro R**, **MAD60HE**, **MAD68HE**, **MAD68R** | MAD 68 Pro R uses native A0; the other listed models use the embedded Universal Analog Plugin runtime |
| ATK | **ATK Hex80** | Native Hex80 `0x96`; a compatible PID is accepted only after valid GET responses |
| SayoDevice | **O3C** | O3C has been tested; other SayoDevice products may work when they expose the same validated `0x22` depth protocol |
| IPI / QBZ | **QBZ75** and compatible devices | Native Addressed Analog `09/94/02` after a dynamic capability proof |
| Other brands using compatible protocols | Unlisted compatible models | HallJoy may recognize them only after a valid device-info and protocol proof; a brand name alone is never enough |

The MADLIONS models are intentionally grouped into one public row. Their
internal routes differ, but they belong to one supported brand from a user's
perspective.

### Support through embedded Universal Analog Plugin

The pinned Universal Analog Plugin runtime bundled with HallJoy declares support
for these models. No system-wide plugin installation is required. Not every
hardware revision in this section has been tested by the HallJoy team on
physical hardware.

| Brand | Models |
|---|---|
| Razer | **Huntsman V2 Analog**; **Huntsman Mini Analog**; **Huntsman V3 Pro**; **Huntsman V3 Pro Mini**; **Huntsman V3 Pro Tenkeyless** |
| Keychron | **Q1 HE**; **Q3 HE**; **Q5 HE**; **K2 HE**. **K4 HE ANSI** (`3434:0E40`) is physically validated and used daily by the HallJoy author with the custom full-report firmware. Stock K4 firmware is not supported for gaming because its per-key protocol introduces unacceptable sub-actuation latency. See [AnalogSense's full-report firmware page](https://analogsense.org/firmware/) for background and general flashing guidance; it does not currently provide the HallJoy K4 image. |
| Lemokey | **P1 HE ANSI**; **P1 HE ISO** |
| NuPhy | Analogue NuPhy keyboards; the decoder explicitly handles **Air60 HE** and **Air75 HE** |
| DrunkDeer | The DrunkDeer family; the decoder includes **A75**, **G60**, **G65**, and **G75** |
| Wooting | Analogue Wooting keyboards through the embedded build with Wooting device support |

### If your keyboard is not listed

HallJoy will first try to match the device against one of its known safe protocol
families. It accepts the interface only after a valid protocol proof; it never
claims a random HID endpoint just because it looks similar.

If automatic detection does not work, contact the author on Discord:
**`pash.ok`**.

Any of the following is usually enough to start adding support:

- an open-source application that already reads analogue values from the keyboard;
- an open SDK or protocol specification;
- a firmware file;
- an offline `.exe` updater or configurator that can be analysed.

Some keyboards do not expose a separate analogue protocol at all: their firmware
may never provide key travel to external applications. In that case, HallJoy
cannot create full analogue support on its own.

> **Experimental firmware route:** If you are brave enough, your keyboard can
> load a custom firmware update, and you accept all the risks, the author is
> willing to try modifying its firmware for HallJoy support. There is no promise
> of success. A failed or incompatible image can permanently brick the keyboard,
> so proceed entirely at your own risk and be prepared for that outcome.

## Input Overlay for OBS

Input Overlay renders the current keyboard layout and real HE key travel on a
transparent browser canvas. It is served locally on `127.0.0.1` and does not
publish your input to the internet.

To add it to OBS:

1. Open the **Input Overlay** tab in HallJoy.
2. Click **Start server**.
3. Click **Copy URL**.
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

- `settings.ini` and `bindings.ini` contain the Default global profile;
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

1. Close the keyboard's official desktop configurator and every web-driver tab.
2. Check the keyboard firmware and software mode. On some keyboards a vendor
   **Turbo**, performance, or compatibility mode can stop exposing the analogue
   interface used by external applications.
3. Reconnect the keyboard, restart HallJoy, and inspect **Configuration** and
   **Gamepad Tester** for the detected route and live values.
4. If the model still does not work, contact **`pash.ok`** on Discord with the
   exact model, firmware version, and any available firmware or offline updater.

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

> **For most users, this is the end of the guide.** Everything below is intended
> for developers, contributors, and advanced troubleshooting.

## Building from source

Requirements:

- Visual Studio 2022 Build Tools with **Desktop development with C++** and the
  x64 Clang tools component;
- Python 3.12;
- Git;
- PowerShell 5.1 or newer.

1. Clone or extract the repository into a fresh directory.
2. Run `BUILD.cmd`.
3. The release executable will be created at:

```text
build\release\HallJoy.exe
```

The official build uses the pinned versions in
[tools/dependency-lock.json](tools/dependency-lock.json) and runs the same static
and portable C++20 gate used by CI before producing the x64 executable.

## Runtime dependencies

HallJoy contains its own pinned ABI1 build of Universal Analog Plugin. It does
not require a system Wooting Analog SDK or a global Universal Analog Plugin
installation. In writable or portable locations, the private runtime is verified
beside the EXE. In a protected location it is automatically placed, without UAC,
in a versioned `%LOCALAPPDATA%\HallJoy\Runtime` directory. Before the isolated
child host starts, the runtime is checked byte-for-byte against the embedded
resource.

ViGEmBus remains the only external system dependency required for the virtual
Xbox controller.

## Supported analogue routes

| Family | Protocol | Discovery |
|---|---|---|
| MAD68 Pro R and compatible MAD68 devices | Native asynchronous A0 | Exact HID fingerprint, 68-key layout boundary, and either audited PID/firmware or a valid reversible A9 acknowledgement |
| ATK Hex80 and compatible devices | Native `0x96` matrix polling | `VID 373B`, `FF60:0061`, and valid GET `02 96 24` / `02 96 1C` responses; the PID does not need to be pre-registered |
| IPI/QBZ75 and compatible devices | Addressed Analog `09/94/02` | `FF60:0061`, reports of at least 64 bytes, valid checksum, and a response for the exact requested key IDs; up to nine keys per request |
| Sayo and compatible brand models | Native depth `0x22` | Audited `8089:0009` or a valid depth response from another `VID 8089` PID |
| Irok/SparkLink-compatible devices | Native matrix/route polling | Capability fingerprint and valid protocol response |
| Aula WIN 60 HE MAX and compatible 6×21 family | Native read-only `5C/12/23/2B` polling | Brand-scoped prefilter, `FFA0:0001`, 65-byte HID envelope, and complete dynamic proof; `1CA2:1902 / App V1.1.6` is physically validated |
| Aula WIN 60/68 HE Standard and KP-TE153 | Native W669 `0D/18/21` event stream | `FF1B:0091`, 64-byte report-ID-1 envelope, read-only firmware identity, travel descriptor, complete 132-position override generation, and an exact official SI2825/SI2828/SI2851 factory profile; an unknown product must prove enough explicit mappings and never inherits a guessed layout |
| Wooting, selected MADLIONS devices, and others | Universal Analog Plugin | Support provided by HallJoy's pinned embedded runtime |

Brand or VID alone is not enough. A native backend receives a device only after
proving its protocol. An unproven device remains available to Universal Analog
Plugin or continues to operate as a normal digital Windows keyboard.

## Native/Universal Analog Plugin arbitration

At startup, HallJoy classifies devices in a fixed safe order:

1. MAD68 native A0;
2. Hex80-compatible `0x96`;
3. Addressed Analog `09/94/02`;
4. Aula MAX-family and Standard/W669 complete read-only proofs on their separate
   vendor interfaces;
5. SparkLink and Sayo capability checks inside the shared backend initialization;
6. Universal Analog Plugin starts only after the exact proven native HID
   interface paths have been published.

Every native backend:

- verifies its HID fingerprint;
- performs only its known capability probe;
- claims an exact SetupAPI interface path only after a valid response;
- publishes the confirmed native path list to the isolated plugin host before
  Universal Analog Plugin HID enumeration begins.

The embedded plugin patch applies the exclusion before `CreateFileW`, so
Universal Analog Plugin never opens an endpoint already proven by a native
protocol. The first protocol to prove a path owns only that exact path; sibling
interfaces with the same `VID:PID` remain independent.

A newly connected device may require a HallJoy restart so arbitration can occur
before Universal Analog Plugin starts. Reconnecting an already classified exact
interface within the current session is supported by its native worker.

## Addressed Analog

The Addressed backend supports the QBZ75-compatible family and other devices
that prove the same protocol:

```text
Usage Page: FF60
Usage:      0061
Reports:    at least 64 bytes
Map:        09 83 00
Analogue:   09 94 02
Batch:      up to 9 key IDs per request
```

The backend first requests a dynamic `0x83` map. A complete map is used directly;
the validated QBZ-compatible family retains a canonical fallback. A `09/94/02`
response is accepted only with a correct checksum, the exact record count,
matching requested key IDs without duplicates, and plausible Hall values.

The scheduler prioritizes bound keys, moving or held keys, recently released
keys, and then background coverage of the remaining positions. Every actual
value change immediately wakes the shared realtime pipeline. Production does
not write Addressed per-key or trace files.

## MADLIONS protocol arbitration

Devices using `VID 373B` may expose different protocols:

- Universal Analog Plugin;
- MAD68 native A0;
- Hex80-compatible `0x96`;
- potentially Addressed `09/94/02`, if the device proves that fingerprint.

HallJoy does not route them by brand alone. MAD68 requires the compatible 68-key
family boundary and a valid A9 acknowledgement. Hex80 requires two valid GET
responses. Addressed requires a valid `09/94/02` response for the exact requested
key IDs. All other `373B` devices remain available to Universal Analog Plugin.

The MAD68 runtime allow-list remains limited to A8/A9. HallJoy does not modify
the keyboard firmware.

## Native ATK Hex80 `0x96`

Known Hex80 devices have used PID `1176`, `1177`, and `1250`, but v1.4 is not
limited to that list. Another `VID 373B` PID is accepted only when it has the
exact `FF60:0061` fingerprint and returns valid GET responses:

```text
02 96 24 — travel_max
02 96 1C — matrix block
```

After revalidating the open device, the backend sends the documented `03 96 19`
once to leave calibration mode. HallJoy never sends the `03 96 18` command that
enters calibration mode. It reads 104 slots in blocks of four and publishes 82
standard HID keyboard keys.

## Shared low-latency ViGEm output

All input sources use one output scheduler:

1. The first changed state after idle is sent to ViGEm immediately.
2. Changes inside the next 1 ms window are coalesced into the freshest complete
   XInput state.
3. A deferred state is guaranteed to be sent at its fixed deadline; a newer
   packet does not move that deadline.
4. Curves, bindings, and last-key priority are applied before the actual output
   submission.
5. Unchanged keepalive reports are not sent.

HallJoy does not use interpolation, prediction, digital fallback, or republish an
old value as a new measurement.

## Interface and diagnostics

Live in-memory diagnostic information is available on **Configuration** and
**Gamepad Tester**. It covers MAD68 A0, Hex80 `0x96`, Addressed `09/94/02`, Aula
WIN60HE, SparkLink, Sayo, and Universal Analog Plugin/Wooting. Blue analogue
bars and green digital
preview indicators are retained. Digital events are UI-only and do not drive
analogue ViGEm controls.

The production target creates no continuous per-key, latency, analog-host,
Addressed trace, or general diagnostic files, and starts no permanent log writer.
A lightweight unhandled-exception filter performs no normal-operation I/O and
creates `HallJoyCrash.txt` only after a process crash. The silent MAD68 emergency
A9 recovery watchdog remains enabled.

## Limitations

- HallJoy does not infer a completely unknown protocol from arbitrary packets;
  it safely proves only known protocol families.
- MAD68 A0 uses a validated 68-position table. Incompatible MADLIONS devices
  remain available to Universal Analog Plugin.
- An unknown Addressed device without a sufficiently complete `0x83` map must
  match the canonical QBZ key-ID mapping or its capability probe is rejected.
- Aula WIN60HE with `App V1.1.6 / Feb 4 2026` has physical validation for real
  analogue matrices, 10+ rollover, measured polling rate, release-to-zero, and
  repeated disconnect/reconnect. Other Aula-compatible PIDs, firmware builds,
  and 6×21 maps may connect as protocol-compatible devices after a complete
  structural proof, but are not automatically considered physically validated.
- Aula Standard/W669 is a different protocol. The official product catalog and
  layout files provide exact built-in maps for four SI2825 variants (61 keys),
  two SI2828 variants (68 keys), and SI2851/KP-TE153 UK (69 keys). HallJoy asks
  the keyboard for that firmware product on every proof and reconnect. Future
  W669 products can use a sufficiently explicit device map, but a stock unknown
  product whose firmware reports only “inherit factory layout” is rejected until
  its factory map is known; key count alone is not enough to map HID usages safely.
- Keep the keyboard's official configurator closed while HallJoy owns its vendor
  interface.

## Adding a new protocol

The v1.4 architecture uses independent protocol modules. A new backend should
not add special cases to application lifecycle, the shared raw-input path, UI,
or ViGEm output.

Start with:

```text
python tools/new_native_backend.py --help
python tools/new_native_backend.py ^
  --slug foo_matrix ^
  --prefix FooMatrix ^
  --enum FooMatrix ^
  --protocol-value 7 ^
  --display-name "Foo Matrix" ^
  --start-phase AfterRealtime
```

The generator creates separate parser/backend files, a portable unit test, a
protocol document, a unique enum, and one entry in the central catalog. After
implementing the protocol, run:

```text
python tools/run_native_backend_checks.py --require-compiler
BUILD.cmd
```

The shared catalog provides safe discovery ordering relative to Universal Analog
Plugin, exact path
ownership after capability proof, lifecycle and device-change dispatch,
max-aggregation across real analogue devices, digital-fallback blocking only for
authoritatively owned HID keys, the shared curve/last-key-priority/low-latency
ViGEm path,
automatic Configuration and Gamepad Tester integration, and reverse-order
shutdown with state cleanup.

Begin with [Architecture Overview](docs/development/ARCHITECTURE_OVERVIEW.md) and
[New Protocol Worksheet](docs/development/NEW_PROTOCOL_WORKSHEET.md). A new
protocol pull request should follow
[the protocol PR template](.github/PULL_REQUEST_TEMPLATE/new-protocol.md).

## Development and validation records

The current v1.4 roadmap, decisions, risks, validation matrix, and worklog are
under [`docs/v1.4`](docs/v1.4/README.md). Documents under `docs/stability` are
preserved as historical evidence and do not define the current version or
release readiness.
