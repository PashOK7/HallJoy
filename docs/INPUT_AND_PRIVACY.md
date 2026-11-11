# Input and local data

HallJoy reads supported keyboard HID reports, including physical key travel, to
produce virtual Xbox controller input. It can also block normal keyboard output
for configured bindings. This is the purpose of the application; it should not
be described as reading no keyboard input.

HallJoy's [README](../README.md) describes automatic crash/missing-keyboard reports
and optional continuous diagnostic logging, disabled by default. Reports and
crash dumps can contain technical device/process information. Review them before
sharing with support. This page is not a complete audit of the application's
input processing, memory contents or network behavior.

The optional installer adds no telemetry, account, service, autostart or network
request. It does not launch HallJoy or install ViGEmBus. Installing that driver
remains an explicit action in HallJoy with separate administrator approval.

The optional portable updater contacts GitHub only when invoked. It downloads
the official release and verifies its published SHA-256. A checksum does not
establish publisher identity or replace code signing. The launcher itself does
not check for updates.

Normal user data is stored under `%LOCALAPPDATA%\HallJoy`. A `HallJoy.portable`
marker beside the EXE selects data beside that EXE. The repository's portable
helper uses `.local/portable`, which is ignored by Git. Uninstalling the installed
copy preserves user data and the shared ViGEmBus driver. See
[portable data and backups](PORTABLE.md) for explicit transfers between copies.

Implementation references: `src/HallJoyProject/HallJoy/app_paths.cpp`,
`debug_log.cpp`, `analog_host_client.cpp`, and the optional scripts in `tools/`.
