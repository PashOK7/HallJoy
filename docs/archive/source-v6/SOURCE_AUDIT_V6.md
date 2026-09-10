# HallJoy Madlions V6 source audit

## Evidence reviewed

- eight V5 analogue-host crash events from one field session;
- two complete text crash reports and minidumps;
- `HallJoyAnalogHost.log` and the main HallJoy diagnostic log;
- HallJoy, Universal Analog Plugin, and the affected Soup sources.

All eight failures share the same signature: execution at `0x21` from
`madlions_before_receive`. Individual host lifetimes vary, so the event is not
explained by a fixed counter or a Steam-specific startup path.

## Corrected violations

1. Removed simultaneous unfinished Madlions reads and writes on one handle.
2. Removed stack-lifetime `OVERLAPPED` objects from Madlions transactions.
3. Added a dedicated manual-reset event for each operation.
4. Paired every `CancelIoEx` with a wait for terminal completion.
5. Kept context and buffer addresses stable until kernel I/O completed.
6. Drained stale reports before starting a request.
7. Added bounded transport timeouts and exact checkpoints.
8. Exported Win32 transport errors into shared diagnostic state.
9. Converted persistent I/O failure into a controlled new-process/new-handle
   reconnect.
10. Neutralized the parent snapshot before crash evidence was written.
11. Preserved the V5 bounds, length, clamp, per-device-state, and `HandleRaii`
    corrections.
12. Pinned Soup to an exact commit for reproducibility.

## Static and build checks

- all 50 HallJoy Windows x64 translation units passed syntax compilation under
  LLVM-MinGW/Clang with MS extensions and diagnostic definitions;
- focused compile fixtures exercised the exact SafeHID implementation and the
  replaced Madlions parser function;
- project/filter XML and mandatory source/library paths were verified;
- the archive passed `unzip -t`;
- the build script verifies every required input before MSBuild.

## Required physical validation

The original Linux environment could not run the MSVC link, Windows HID I/O, or
a real keyboard. Runtime acceptance therefore required a long PnP stress test
on the same Madlions/Windows system. The criterion was zero genuine
analogue-host crash reports, no stuck keys, and no prolonged input interruption.
