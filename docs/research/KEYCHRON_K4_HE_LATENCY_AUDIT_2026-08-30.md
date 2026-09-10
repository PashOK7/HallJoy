# Keychron K4 HE latency audit - 2026-08-30

## Scope

Read-only audit of the locally connected Keychron K4 HE ANSI, followed by
static inspection of Keychron's public QMK source at commit
`bc56b3c611dcc1a8ed9a2acb8bdc4da5e1a80c27`. No configuration, calibration,
EEPROM, or firmware image was written to the keyboard.

## Confirmed device and transport

- USB identity: `VID 3434`, `PID 0E40`, which the public `info.json` assigns
  to K4 HE ANSI.
- The device exposes the Keychron analogue vendor HID collection on interface
  1, usage page/usage `FF60:0061`.
- `A9 01` (`AMC_GET_VERSION`) returned analogue-matrix protocol version `4`.
- A documented read-only `A9 30` (`AMC_GET_REALTIME_TRAVEL`) request for the
  `W` matrix location (row 2, column 2) was measured for 100 transactions.
  The end-to-end host request/reply time was: minimum 1.978 ms, mean 2.548 ms,
  p50 2.992 ms, p95 3.007 ms, p99 3.015 ms, maximum 3.016 ms.

This measures only the external vendor-HID query, not finger-to-game latency.
It is nevertheless decisive for route selection: an external UAP/HallJoy path
must wait for at least one such request after it elects to read a key, whereas
the keyboard's native game-controller/XInput path uses the analogue value on
the keyboard itself.

## Firmware findings relevant to latency

The board uses an STM32F401 clocked at 72 MHz and builds with `-O2`. The
custom analogue scan loops across all 19 columns. Per column it waits 40 us for
analogue settling, then samples all six ADC rows. `ANALOG_DEBOUCE_TIME` defaults
to 3, so a column with a changing key can be re-read up to two additional
times. Digital QMK debounce is configured as `sym_eager_pk` with a zero-ms
window, and the custom scan bypasses the normal debounce call.

The USB hardware is USB Full Speed. Consequently a firmware-only change cannot
make the standard wired HID endpoint an 8 kHz endpoint; one USB frame is 1 ms.
Lowering settling, ADC sampling, or analogue retry counts is possible in a
custom source build, but it trades latency for false presses/noise and must be
validated on this individual board before it can be considered an improvement.

## Recommended lowest-latency route

1. Use Cable mode on a direct USB port; do not use Bluetooth, a hub, or the
   external analogue polling path when the game can consume a controller.
2. Configure the desired controls in Keychron Launcher as native gamepad/XInput
   controls. This avoids the measured 2-3 ms vendor request/reply hop and the
   additional PC-to-ViGEm output hop.
3. Use Rapid Trigger for the relevant keys, with the lowest stable actuation and
   press/release sensitivity exposed by Launcher. Settings change trigger
   distance, not the USB 1 ms ceiling.
4. Keep the stock firmware unless a measurement demonstrates a real regression.
   The current official K4 HE release notes describe RGB/other fixes, not a
   latency improvement.

## Custom firmware experiment, if needed

The only defensible next experiment is a separately built, recoverable
"wired-tournament" image, starting with one isolated change:
`ANALOG_DEBOUCE_TIME=1`. Do not combine it with a reduced 40-us settling delay
or ADC sample time on the first build. Compare repeated key presses and hold
stability against stock, then change one parameter at a time. The stock DFU
bootloader and official image must be retained for recovery.
