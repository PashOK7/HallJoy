# HallJoy v1.4 IROK ND75 test build

This is an unsigned, experimental hardware-validation build for the wired IROK
ND75 (`0416:7372`, M484/X86HERGB). It does not flash the keyboard and does not
write key mappings, calibration, lighting, profiles, or firmware.

## Before the test

1. Close the official IROK software and any web keyboard configurator.
2. Connect the ND75 by USB cable in wired mode.
3. Keep all files from this archive in one writable folder.
4. Start `HallJoy-IROK-ND75-Test.exe` normally. Administrator rights should not
   be required.

If the keyboard is connected after HallJoy starts, wait two seconds. This build
has a hotplug worker and should discover it without restarting.

## Test sequence

1. Open HallJoy's Gamepad Tester and press one ordinary key slowly from rest to
   the bottom, then release it slowly. Repeat five times.
2. Repeat with keys from different rows: `Esc`, `F1`, `1`, `Q`, `A`, `Z`,
   `Space`, `Up`, and `Right`.
3. Hold at least four ordinary keys together, vary their depth, and release all.
4. Unplug the ND75 while HallJoy is running. Wait three seconds, reconnect it,
   wait three seconds, and repeat one slow press.
5. Close HallJoy with the window close button. Confirm that the process exits.

Do not run the official IROK application during the test because both programs
use the same configuration endpoint.

## Return these files

Send back `HallJoyStabilityTrace.log` from the same folder. Also include any of
these files if they exist:

- `HallJoyDiagnosticCrash.txt`;
- `HallJoyDiagnosticVectored.txt`;
- `HallJoyDiagnosticExit.txt`.

Please state whether the on-screen analogue value moved smoothly, reached zero
after every release, and whether HallJoy exited normally. The log intentionally
contains protocol reports and timing counters, but sanitizes the application
directory and does not collect a username, command line, keyboard text, general
hardware inventory, or memory dump.
