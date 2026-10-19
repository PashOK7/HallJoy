# HallJoy 1.6.10

- Added experimental support for the MelGeek MADE68 Air, with an automatic
  layout. Hardware testing is incomplete; please report how it works.
- SparkLink V2 keyboards that are not in HallJoy's list but are in the
  manufacturer's official list (many IPI, AULA, Everglide and other models)
  now connect when they answer the protocol and report live key travel. They
  show the yellow "Keyboard not in the list" notice.
- Fixed: when HallJoy found a supported keyboard but another program had it
  open, for example the manufacturer's web driver in a browser tab, HallJoy
  said that no supported keyboard was detected. It now says that the keyboard
  is in use by another app, and connects by itself as soon as that app or tab
  is closed. This affects AULA WIN 60 HE PRO / MAX, WIN 68 HE PRO / MAX,
  HERO 68 HE PRO, GravaStar Mercury V75, IROK MG75 Pro and related keyboards,
  Chilkey Slice75, Neo65 and SteelSeries Apex Pro.
- Fixed: the "Keyboard not in the list" notice and its Open log button did not
  appear for keyboards connected through a protocol match.
- MonsGeek, Akko, EPOMAKER and other RongYuan keyboards with a live stream:
  analog input is read at a higher priority, so a busy game delays it less.
- The support report says why an AULA keyboard could not connect, and records
  key timing only for keys bound to the gamepad, never ordinary typing.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
