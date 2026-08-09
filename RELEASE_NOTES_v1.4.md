# HallJoy v1.4 release notes

HallJoy v1.4 is a major architecture, hardware-support, interface, and
reliability release. It replaces the older shared-runtime design with isolated
protocol ownership, a bounded realtime/output pipeline, transactional storage,
and one retained scrolling system across the application.

## Highlights

- expanded native support for Aula, MADLIONS, ATK Hex80, IPI/QBZ Addressed
  Analog, Irok/SparkLink, and SayoDevice protocol families;
- a pinned private Universal Analog Plugin runtime embedded in HallJoy;
- no system-wide Wooting Analog SDK or UAP installation;
- up to four virtual Xbox controllers;
- unified low-latency ViGEm output with immediate wake after idle;
- global profiles, transactional presets, and a detached keyboard-layout editor;
- a browser-based OBS Input Overlay with configurable visual effects;
- one high-performance retained scrolling architecture for all six tabs;
- bounded worker lifecycle, exact HID-path ownership, and crash-only production
  diagnostics.

## Dependencies and first start

HallJoy carries and verifies its own private Universal Analog Plugin runtime.
It is extracted beside the executable in a writable portable installation or
to a versioned `%LOCALAPPDATA%\HallJoy\Runtime` directory when the executable is
in a protected location. The isolated analogue host loads only the verified
embedded bytes.

ViGEmBus 1.22.0 remains the only system-wide runtime dependency. HallJoy does
not silently download or elevate an installer; if ViGEmBus is missing, it opens
the pinned official release and provides manual installation guidance.

## Hardware support

### Aula WIN 60 HE MAX and compatible 6x21 devices

The new read-only Aula backend performs a complete structural proof before it
claims an interface. The physically tested WIN 60 HE MAX route uses
`1CA2:1902`, `FFA0:0001`, and firmware `App V1.1.6 / Feb 4 2026`.

Physical validation covered real analogue matrices, release-to-zero, 10+
rollover, up to 22 simultaneous keys, approximately 344 Hz sustained delivery,
and three disconnect/reconnect recoveries. Compatible sibling firmware, PIDs,
and key counts can be admitted only after the same bounded dynamic proof and
are not labelled physically tested without hardware evidence.

### Aula Standard/W669

WIN 60/68 HE Standard and KP-TE153 use a separate `0D/18/21` protocol. HallJoy
reads the exact firmware product, selects one of the official SI2825/SI2828/
SI2851 factory profiles, overlays the complete live remap generation, and then
subscribes to the event stream.

Unknown products never inherit a guessed layout from their PID, name, or key
count. A stock unknown product that reports only “inherit factory layout” is
rejected until its exact factory map is known.

### MADLIONS protocol arbitration

Devices under `VID 373B` are no longer routed by brand alone. HallJoy separates:

- MAD68 asynchronous A0;
- ATK Hex80-compatible `0x96`;
- Addressed Analog `09/94/02`;
- devices that remain owned by the embedded Universal Analog Plugin.

Every native route must prove its own fingerprint and claims only the exact HID
path that answered correctly.

### ATK Hex80-compatible `0x96`

The native Hex80 route accepts the audited interface only after valid
`02 96 24` and `02 96 1C` GET responses. Known PIDs include `1176`, `1177`, and
`1250`, but a compatible PID can be admitted after the complete proof. HallJoy
never sends the command that enters calibration mode.

### Addressed Analog / IPI / QBZ75

The Addressed backend requests a dynamic `09/83/00` map and polls analogue
values through checksummed `09/94/02` batches of up to nine key IDs. It rejects
duplicate, substituted, malformed, or implausible records. The scheduler
prioritizes bound, moving, held, and recently released keys while maintaining
background coverage.

### Irok/SparkLink and SayoDevice

SparkLink support is capability-driven rather than tied to one VID/PID. Irok
MG75 Max is physically validated; compatible models may work after the same
device-info and route proof. Irok MG75 v2 was physically tested and is not
supported because it uses another MCU/protocol family.

SayoDevice O3C is physically tested through native depth protocol `0x22`.
HallJoy follows the device's current keyboard HID assignments instead of
assuming fixed letters. Other `VID 8089` devices require a valid depth response.

The detailed compatibility boundaries are maintained in
[`SUPPORTED_HARDWARE.md`](SUPPORTED_HARDWARE.md).

## Realtime input and ViGEm output

All input sources now use one output owner:

1. the first changed state after idle is sent immediately;
2. additional changes inside the next 1 ms window are coalesced into the newest
   complete XInput state;
3. a deferred deadline never moves forward because a newer packet arrived;
4. curves, bindings, last-key priority, and Snap Stick are applied before the
   actual ViGEm submission;
5. unchanged keepalive reports are not sent.

HallJoy does not interpolate, predict, publish digital fallback as analogue, or
republish an old value as a new measurement.

## Remapping, profiles, and layouts

- up to four virtual gamepads can be configured independently;
- buttons, sticks, triggers, D-pad, Start, Back, Guide, and stick clicks can be
  assigned through drag-and-drop remapping;
- per-binding response curves, deadzones, and sensitivity are retained;
- last-key priority and Snap Stick handle opposing directions;
- Block Bound Keys can suppress normal keyboard output outside HallJoy while a
  key is assigned to a gamepad control;
- global profiles now track remap changes as unsaved and support explicit save;
- layout presets are transactional and can be shared as standalone `.ini`
  files;
- the detached layout editor can move, add, delete, label, bind, resize, and
  space keys;
- Generic 100% ANSI and Keychron K4 HE layouts are shipped as optional presets;
  the default preset remains unchanged.

## Interface and scrolling

All six tabs now use the same retained page/scroll-controller architecture.
Scrollbar dragging no longer causes child controls and icons to disappear, and
wheel/track/thumb behavior is consistent across Remap, Configuration, Gamepad
Tester, Global settings, Input Overlay, and Mouse settings.

Additional interface corrections include:

- retained controls render from cached static content plus explicit live
  overlays;
- keyboard-preview release updates remain active regardless of the selected tab;
- Configuration graphs repaint continuously from live telemetry without
  requiring window clicks;
- Gamepad Tester refreshes changed values without the old low-FPS appearance;
- mode, font, profile, and hardware selectors use real themed combo boxes;
- overflowing combo lists scroll their list content, while fitting lists do not
  reinterpret the mouse wheel as option selection;
- popup focus, outline, close, and retained-face rendering share one canonical
  implementation;
- numpad `+` and numpad `Enter` use the visually validated 87 px height in both
  Generic 100% ANSI and Keychron K4 HE presets.

## Input Overlay for OBS

The local browser overlay renders the selected keyboard layout and live HE key
travel on a transparent canvas. It binds only to `127.0.0.1` and uses a
per-generation session token.

The overlay supports:

- top-to-bottom or bottom-to-top fill direction;
- raw travel or the value after response curves;
- configurable indicator and label colours;
- font, label size, shadow, and refresh interval controls;
- Smooth response, Glass keys, Bloom, Edge sweep, Micro-scale, Label contrast,
  and Rim lighting effects;
- retained dirty-frame rendering that does not redraw an unchanged idle frame.

Smooth response defaults to 15%.

## Storage and portable mode

Writable state defaults to `%LOCALAPPDATA%\HallJoy`. Settings, bindings,
profiles, layouts, and curve presets use checked same-directory transactions:
temporary write, flush, readback validation, and atomic replacement. Failed
saves preserve the previous known-good file and surface the failure.

Portable mode is explicit: create `HallJoy.portable` beside the executable in a
writable directory before first start. Legacy writable state is migrated with
source-preserving backups and a transaction marker.

## Lifecycle and fault containment

The background architecture was redesigned around generation ownership and
bounded stop results:

- worker entry points have C++ and OS exception boundaries;
- failed or incomplete generations retain ownership and block unsafe overlap;
- shutdown joins are bounded and report truthful completion state;
- the private UAP host runs in a contained child process with unnamed inherited
  IPC handles and a random generation token;
- native paths are published before UAP enumeration so the plugin never opens a
  device already proved by a native backend;
- device reconnect cannot revive a stopped generation;
- output and overlay owners can reap and restart contained failed workers.

Ordinary shutdown paths do not use `TerminateThread`.

## Production diagnostics

The release target has no permanent per-key, latency, analogue-host, Addressed,
or general diagnostic log writer. Live in-memory route information is available
on Configuration and Gamepad Tester. `HallJoyCrash.txt` is created only after an
unhandled process crash; the silent MAD68 emergency recovery watchdog remains
enabled.

## Build and validation

The official `BUILD.cmd` pipeline:

- verifies pinned dependencies from `tools/dependency-lock.json`;
- runs the complete static and portable C++20 native-backend gate;
- requires Visual Studio 2022 x64 C++ tooling;
- rejects every unexpected production compiler/linker warning;
- builds the private UAP runtime and the optimized x64 executable;
- packages the result under `build\release`.

The only allowed linker warning is the external ViGEmClient missing-PDB
`LNK4099`; production code compiles with zero errors and no other warnings.

## Known limitations

- A newly connected device may require a HallJoy restart so native/UAP
  arbitration occurs before plugin enumeration.
- Keep official desktop configurators and web drivers closed while HallJoy owns
  the vendor interface.
- Protocol-compatible does not mean physically tested.
- Keychron K4 HE ANSI is physically validated and used daily with HallJoy on
  the custom read-only full-report firmware. Its stock firmware remains
  unsupported for gaming because the per-key protocol has unacceptable
  sub-actuation latency. [AnalogSense's full-report firmware page](https://analogsense.org/firmware/)
  provides background and general flashing guidance, but does not currently
  list a pre-built K4 HE image.
- Some keyboards expose no external analogue protocol at all.

## Upgrade notes

Existing settings are migrated into the current writable-data root with backup
and replay protection. Newly shipped built-in layouts become visible without
replacing same-name user presets, and the first preset remains the default.

HallJoy v1.4 is not compatible with a release folder assembled from mixed old
DLLs or manually installed UAP variants. Use the complete package produced by
`BUILD.cmd` or downloaded from the matching GitHub release.
