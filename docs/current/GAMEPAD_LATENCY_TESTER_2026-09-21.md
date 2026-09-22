## Owner camera observation after analog-scale correction

Owner reports Xiaomi12,720p960fps slow-motion mode; MSI21:9 monitor rated200Hz,
exact model unknown. About0.5seconds of slowed video from physical press to
first pixel change, about0.3seconds from gray to bright green. Saved playback
FPS/slowdown factor and physical reference point not yet established; footage
not inspected. Treat as promising qualitative end-to-end observation, not a
verified firmware latency or isolated monitor pixel response measurement.
If960-to30fps constant32x slowdown applies:0.5s=15.625ms and0.3s=9.375ms.
At960-to60fps16x these become31.25ms and18.75ms. Do not subtract the intervals
unless their endpoints establish an additive sequence. At actual200Hz display
frame period is5ms; enabled refresh has not been verified. Camera mode spec
alone does not establish native temporal resolution or interpolation behavior.

## Analog scale correction after owner feedback

Owner rejected binary black/white output: needs an analog scale showing amount
of input. Reports white never returned to black. Previous max(abs(X),abs(Y))
plus exact nonzero test could saturate the indicator for any residual value;
actual device residual/stuck-release cause is NOT established by this report.
Do not hide it with an arbitrary deadzone.

Replaced binary patch with a buffered analog bar, signed center-zero axes,
0..100% triggers, ticks, marker and percentage/raw numeric value. Select each
axis separately; default Left stick Y. Source IDs now0=LY,1=LX,2=RY,3=RX,4=LT,
5=RT,6=any button. No temporal smoothing or easing. Dedicated direct OS reader
and timer unchanged. Back buffer retained across paints/resized safely;
GDI timestamp now follows the composed scale submission. No fresh numeric
rendering latency claim; earlier headless poll measurements exclude drawing.

Release build, source-encoding audit and four linked EXE gates PASS; installed
by standard publisher. No agent visual run, no firmware/calibration changes.
EXE SHA256 0671d3a6cd63c61364448b2cd3b6dfcd4c5858f33031a7d93699baee88fd4fa3.
Backup: .local/backups/keychron-protocol-20260921/before-analog-latency-scale.zip.
Build log .local/latency-analog-delivery.log. Owner checks the visual scale.
Historical black/white description below is superseded.

# Camera latency tester — 2026-09-21

Delivered build/bin/Release/x64/HallJoy.exe, SHA256
55c38b0e326a612066aa40738056e8273ccb5776124a0607e5f902def0b512cd.
Entry: Gamepad Tester > Camera latency test. Firmware remains r4.
No forced logging, automatic CSV writing, publication or agent GUI run.

## Implementation

gamepad_latency.cpp owns an explicit diagnostic window on a separate thread.
It caches the selected actual Windows.Gaming.Input controller and bypasses
Backend_GetLastReportForPad, the normal10ms monitor, main UI timer and key HID
telemetry. Independent OS controller selection also works outside the K4 route.
A unique controller is initially selected; a disconnected selection is never
silently replaced. Controller/source selection starts a new capture.

Select left/right stick, left/right trigger or any button. Stick signal is
max(abs(X),abs(Y)). The patch is black at exactly zero, white at nonzero, gray
without a controller. No additional deadzone, smoothing or animation; upstream
profile/device thresholds still apply. Standard WGI buttons exclude Guide.
Initial/resumed/reconnected readings are baselines, not invented edges.

High-resolution timer requests0.5ms waits without spinning or real-time priority.
Actual intervals are measured rather than inferred from configuration. Changed
signals invoke UpdateWindow on the same thread and flush/timestamp the GDI
patch submission before drawing diagnostic text. Main-window timer is bypassed.
This still uses desktop composition: scheduling, GDI/DWM, monitor scanout and
camera sampling remain. Sub-refresh transitions may be invisible on camera.
No zero-latency, lossless-packet-capture or physical-key-latency claim.

Fast polling only while the diagnostic window is open and non-minimized.
Device inventory refresh is at most once/second; unplug recognition may lag.
Dragging/resizing, selectors and save dialogs may interrupt sampling: record
with the window stationary and unobstructed. Closing stops the worker; app
shutdown signals it with bounded3s join and retains live-thread resources if
a system API stalls. No firmware/calibration/profile changes by the tester.

## Timing records

8192 changed-value records kept in memory, overwrite count visible. Save CSV
is the only output write and uses an explicit save dialog. Columns:
read_begin_qpc, read_end_qpc, gdi_submit_qpc, os_timestamp_raw, poll_gap_ms,
connected, active, value. Header includes QPC frequency/max poll gap.
Raw OS timestamp is not assumed to share QPC units or firmware clock.
Zero GDI timestamp means submission was not recorded for that sample.
Change-to-GDI means HOST observation-to-submission, excluding preceding polling
uncertainty and subsequent display delay. It is NOT physical key latency.
Save resumes with a fresh timing/edge baseline and preserves existing records;
Reset clears records. Camera measures chosen physical reference to screen.

## Verification

- Edge model test PASS: exact zero, tiny signed values, no hidden threshold,
  held/released state, NaN, disconnect/reconnect without false transitions.
  Registered in run_native_backend_checks.py.
- tools/gamepad_latency_probe.cpp uses actual State::Sample and timer on real
  K4 in a neutral production-worker session. No window or synthetic key input.
- Final2000samples: median gap0.9974ms, p95 1.0648ms, max1.5432ms;
  maximum read call0.1453ms. One neutral-value record: this does NOT verify
  motion latency, rendering or camera appearance.
- Earlier1ms requested wait produced median1.97ms, hence0.5ms request and repeat.
  Initial no-controller attempt correctly refused a hardware timing claim.
- Neutral worker stopped Joined, reservation cleared. Final phase0/native0;
  XInput and DirectInput have no controller. Firmware unchanged r4.
- All static native-backend audits PASS. Final Release build and four linked
  image gates PASS, standard atomic publisher installed EXE. Known missing
  ViGEmClient.pdb warning. Intermediate int/LONG max overload error fixed;
  probe compile needed UNICODE defines; final builds succeeded.

Logs: .local/latency-probe-final.log, latency-neutral-session-final.log,
latency-tester-static.log, latency-tester-delivery.log.
Backup: .local/backups/keychron-protocol-20260921/before-latency-tester.zip.
Owner camera/visual check is next. Film one bound key and a fixed patch location
with unchanged camera/monitor settings; compare repeated press/release events.

Primary API references:
- https://learn.microsoft.com/en-us/windows/uwp/gaming/gamepad-and-vibration
- https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw
- https://learn.microsoft.com/en-us/uwp/api/windows.gaming.input.gamepadreading.timestamp
