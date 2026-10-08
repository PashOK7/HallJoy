# HallJoy 1.6.8

- Added support for WLMOUSE Ying75 and MCHOSE Ace 68 Air III.
- Added experimental support for more keyboards:
  - Logitech G PRO X2 RAPID and PRO X TKL RAPID, using the keyboard's live
    key-depth stream. Keys are learned while you press them.
  - Redragon K686 HE: K686BG-RGB-M (US and BR) and K686RGB-M (UK).
  - Red Square Alumix 68 Yotei (Magnetite Ice). Only firmware v1.30 is read;
    other firmware versions are not connected and are logged.
  - FUN60 Pro (board 2304) and FuryCube M35HE.
  - Other keyboards on the BY/IPI and Logitech HID++ protocols that are not in
    the list. Their key positions are read from the keyboard; the app shows
    that the model is not verified.
- Added built-in layouts for the Logitech PRO X2 RAPID and PRO X TKL RAPID and
  a generic 100% ANSI default for the new keyboards.
- Game profiles v2:
  - a Profiles tab and a profile selector in the tab strip;
  - profile rules by program file name and window title;
  - a setting for what happens to the profile when you switch windows;
  - profile import and export (.hjprofile);
  - shortcuts for the next profile, automatic profile switching and
    per-profile shortcuts.
- Tab transitions are shorter (0.2 s) and use one drawing method for all tabs.
- Fixed "HallJoy is paused" at startup on a busy computer: the start-up resume
  now retries automatically, and the Resume button works.
- Fixed the keyboard layout resetting after a restart when a named global
  profile was active.
- The support report records more device details (USB names, identifiers and
  the key data needed to diagnose a problem) for keyboards that are not yet
  supported.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
