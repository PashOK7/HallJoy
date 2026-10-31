# HallJoy 1.6.14

## Now supported

- AULA AG75 (including the Ultra edition), with a new built-in layout. A user's keyboard was confirmed working.
  The "Keyboard not in the list" notice is removed. Select the layout manually.
- Redragon Kumara RT Magnetic K552-M, with new ANSI, ISO and ABNT2 layouts. A user's ISO keyboard was confirmed
  working, including analog controls in Enter the Gungeon and the Gamepad Tester.
- Redragon K686 HE (K686BG-RGB-M / K686RGB-M), with built-in ANSI and ISO layouts. A user confirmed that the
  keyboard and controller controls now work correctly; the hardware-testing notice is removed.
- YUNZII RT75 Pro over USB, with a new approximate built-in layout. A user's keyboard was confirmed working.
  The layout covers the implemented 81-key revisions; other RT75 / RT80 models are separate keyboards.

## Fixes

- ATTACK SHARK keyboards on a RongYuan USB ID could stay undetected until reconnected if their HID interface
  was briefly busy at startup. HallJoy now retries discovery every second, keeps a successful result and no
  longer restarts the waiting worker just because the interface remains busy.
- MCHOSE Mix 87 III is no longer rejected just because its firmware version differs from 1.22. The detected
  version is now recorded in the log; the existing keyboard-settings and analog-reply checks remain in place.
- The ordinary HallJoy.log now identifies the ATTACK SHARK discovery, interface-open, identity and first valid
  analog-read stages, making unresolved cases such as R82 HE easier to diagnose.
- Startup logs no longer label backend initialization flags as Windows errors. In particular, a missing
  ViGEmBus driver is no longer reported as Windows error 8 (out of memory).

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
