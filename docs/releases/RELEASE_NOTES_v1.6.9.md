# HallJoy 1.6.9

- Added support for MonsGeek FUN60 Ultra TMR, with an automatic layout.
- Several keys can now drive one gamepad stick direction (up to 8 per
  direction). The stick follows the most pressed key, and each key's own curve
  sets its part of the range, for example D for 0-25% and F for 25-50%.
  Drop a key on a direction to replace its keys; Shift-drop to add one.
  Profiles saved by 1.6.9 with several keys per direction open in older
  versions with only the first key.
- Fixed: a game or app that keeps the keyboard open (for example Unity games
  such as Valheim, or the manufacturer's app) no longer stops HallJoy from using
  MonsGeek, Akko, EPOMAKER and other RongYuan-based keyboards.
- RongYuan-based keyboards connect more reliably: HallJoy now uses the
  manufacturer's command timing, and retries a keyboard that another program
  released.
- HallJoy no longer asks an unrecognised keyboard for its identity again and
  again; it waits until a device is plugged in or removed.
- The support report records more detail about why a keyboard could not be
  connected.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
