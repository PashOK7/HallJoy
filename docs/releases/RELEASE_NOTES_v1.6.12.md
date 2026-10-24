# HallJoy 1.6.12

## Now supported

- EPOMAKER HE75 V2 TMR (USB), with a built-in layout.
- GamaKay TK75 HE V2, US version (USB), with a built-in layout.
- MonsGeek M1 V5 TMR (USB).
- MonsGeek M2 V5 HE (USB), with a new built-in layout taken from the official MonsGeek driver.
- Womier SK75 TMR (USB), with a built-in layout.

## New experimental support

- GamaKay TK75 HE V2, UK/ISO version (USB). Choose a layout manually.
- IROK Mars68 Pro (USB), with a built-in layout.

## Fixes

- RongYuan-based keyboards (MonsGeek, EPOMAKER, GamaKay, Womier and others): HallJoy no longer shows a false
  "Keyboard communication is unstable" warning shortly after start. Another part of HallJoy kept re-checking the
  keyboard every few seconds while it was already in use; it now checks once.
- MonsGeek FUN60 Ultra TMR: keys no longer stay slightly pressed after releasing several keys at once.
- DrunkDeer A75 Ultra (USB 19F5:FB3E): key travel is now read correctly. Before, keys could reach full press too
  early and the connection was reported as unstable.

## Interface

- Keyboards that HallJoy cannot support (red in the keyboard list) now get a clear message that the keyboard will
  not be supported, instead of the general "No supported analogue keyboard detected" banner.
- Gamepad Tester now shows the real update rate of the keyboard ("updates now / max"). "Now" reads 0 Hz while keys
  are held still on keyboards that only send changes; this is normal.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
