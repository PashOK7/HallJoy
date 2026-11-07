# HallJoy 1.6.15

## Now supported

- CORSAIR K70 PRO TKL over USB on stock firmware 1.12.54, with built-in ANSI,
  ISO and JIS layouts and automatic selection from the keyboard. The ISO tester
  confirmed proportional stick input, full range and release.
- AJAZZ AK680 MAX HE over USB, following user confirmation. HallJoy keeps the
  automatic ANSI layout and removes the hardware-testing notice.

## Experimental support

- Keydous NJ98-CP V4S TMR, with automatic ANSI layout selection. USB gamepad
  input is tester-confirmed. The 2.4 GHz receiver can stream depth directly.
  Startup is still intermittent. The latest build improves held-key handling,
  release recovery and startup diagnostics.
- SteelSeries Apex Pro TKL Gen 3 over USB on firmware 1.19.7, with automatic
  ANSI, ISO and JIS layouts. A tester log confirms live native depth. Game
  behavior and input interactions are still under review.
- CORSAIR VANGUARD PRO 96 over USB on stock firmware 2.8.171, with automatic
  ANSI, ISO and JIS layouts. The ISO tester confirmed live depth and release.
  Full controller testing is pending. Close CORSAIR WEB HUB before connecting.
- IPI Flash 75 PRO / Gemini Seven-Star works over USB through the existing
  SparkLink support, according to the tester. The exact layout is pending.

## Fixes

- All four virtual stick axes now reach the full signed range from -32768 to
  +32767, including mouse-to-stick output.
- Windows notifications for other devices no longer restart an active
  CORSAIR K70 PRO TKL session.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
