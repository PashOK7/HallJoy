HALLJOY v1.4 - OFFICIAL BUILD
=============================

Supported target: Windows x64. Win32/x86 is not supported because the bundled
ViGEm and Universal Analog Plugin components are built for x64.

Both Release|x64 and Debug|x64 are supported. Debug retains symbols, runtime
checks, and disabled optimization, but uses the compatible static release CRT
required by the bundled `/MT` ViGEmClient library.

Requirements
------------

- Visual Studio 2022 Build Tools;
- Desktop development with C++ workload;
- C++ Clang tools for Windows x64;
- Python 3.12;
- Git;
- PowerShell 5.1 or later.

Build from the repository root with one command:

  BUILD.cmd

Equivalent direct invocation:

  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1

Before MSVC runs, the script verifies the dependency lock, static audits, and
portable C++20 tests. Production builds use Warning Level 4; any warning not on
the explicit allowlist fails the official build.

Release output
--------------

  build\release\HallJoy.exe

`build\release` is the sole ordinary package. It contains the executable,
dependency lock, third-party notices and SHA-256 checksum. Build only replaces
these owned files; local portable settings are retained. Compiler outputs and
matching PDB/MAP live in build\bin\<variant>\<configuration>\<platform>;
intermediates live in build\obj with the same suffix. Diagnostic packages are
under build\packages. See docs/current/PROJECT_LAYOUT.md.

Do not use the obsolete `build_all.ps1`, V6/Madlions diagnostic paths, or
`HallJoyMadlionsSafeHID.exe`; none is the current release target.

HallJoy bundles the pinned ViGEmBus installer and verifies its hash/signature.
Installation requires a user action; the build self-test does not install it.
`BUILD.cmd` builds and verifies the private UAP runtime itself.

Release gates are documented in:

  docs\development\TESTING.md
  docs\v1.4\VALIDATION_MATRIX.md
  docs\v1.4\RISK_REGISTER.md

Aula diagnostic boundary
------------------------

Aula WIN60 HE MAX passed the physical exclusive 17-command protocol proof,
including sync, precision, map, travel-envelope, and live analogue behavior.
Other Aula/SparkPlayJoy 6x21 profiles use brand-bounded structural proof plus a
dynamic map; this does not promote them to physically tested status.

The hardware gate uses one isolated file:

  build\packages\aula-diagnostic\HallJoy.exe

It measures actual matrix frequency, latency, ten-key hold, complete release,
and reconnect in one run. High-detail telemetry exists only in that diagnostic
build; the official production build fails if those markers enter its image.
Normal trace/debug calls compile out at their call sites, so their arguments are
not evaluated. Production creates no continuous logs and starts no log writer;
`HallJoyCrash.txt` is created only after an unhandled fatal exception.
