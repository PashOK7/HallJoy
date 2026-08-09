# HallJoy v1.4 project tree

This directory contains the Visual Studio C++ application, portable tests and
project-local support tools for HallJoy v1.4. The current product turns real
analogue keyboard input into up to four virtual Xbox 360 controllers through a
single curve/SOCD/ViGEm output path.

The supported build target is **Windows x64 only**. The bundled ViGEm client
library and the private Universal Analog Plugin runtime are x64 artifacts; no
Win32/x86 configuration is advertised.

Both `Release|x64` and `Debug|x64` are buildable. Because the bundled ViGEm
client is an `/MT` release library, Debug keeps symbols, checks and disabled
optimization but uses the compatible static release CRT plus the project-local
`HALLJOY_DEBUG_BUILD` feature macro.

## Build

Run the official entry point from the repository root:

```powershell
BUILD.cmd
```

Requirements:

- Visual Studio 2022 Build Tools with Desktop development with C++ and x64
  Clang tools;
- Python 3.12;
- Git;
- PowerShell 5.1 or newer.

The final executable is:

```text
build\output\HallJoy.exe
```

Do not use old `build_all.ps1`, V6 Madlions package paths or renamed diagnostic
executables as a release build. See [BUILD_README.txt](BUILD_README.txt)
and the repository [README](../../README.md).

## Runtime and storage

HallJoy uses its pinned private UAP/Soup runtime in an isolated child process.
It does not require a global Wooting SDK or UAP installation. ViGEmBus remains
the external driver dependency; HallJoy never downloads or elevates an installer
and instead gives the user an exact official manual-install link when the driver
is missing.

Writable application state is stored under `%LOCALAPPDATA%\HallJoy`. Portable
storage is used only when explicitly requested and writable. State is not
normally saved beside `HallJoy.exe`, which allows the executable to run from a
protected directory without unsafe migration or silent deletion.

## Analogue routes

Production uses one catalog and exact HID-interface ownership for native
backends, then lets the private UAP handle every unclaimed interface. Implemented
routes include MAD68 A0, Hex80 `0x96`, Addressed `09/94/02`, Aula WIN60HE,
SparkLink, Sayo and UAP-supported devices.

Matching only a brand or VID is insufficient. Each native backend must validate
its protocol/capability before claiming an exact interface. Digital keyboard
events are not synthesized into analogue depth.

Aula WIN 60 HE MAX support is deliberately labelled firmware-proven, not
hardware-validated. It accepts only the documented `1CA2:1902`, `FFA0:0001`,
65-byte interface/proof and remains a release gate until the external physical
test succeeds.

## Verification

Run:

```powershell
python tools/run_native_backend_checks.py --require-compiler
BUILD.cmd
```

The current testing guide is [TESTING.md](TESTING.md). Architecture and protocol
development documentation lives under `docs/development/`; the authoritative
v1.4 status, risks and release gates live under `docs/v1.4/`.

This branch is still under qualification and must not be labelled released until
the open hardware and long-run gates are completed.
