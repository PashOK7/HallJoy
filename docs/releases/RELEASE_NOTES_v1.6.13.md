# HallJoy 1.6.13

## Now supported

- Weikav EG75 HE, with a new built-in layout. A user's keyboard was confirmed working. Before, it worked only as an
  unlisted keyboard with the "Keyboard not in the list" notice.
- EPOMAKER HE65 V2 (including HE65 V2 TMR) over USB, with a built-in layout (shared with HE65 Mag). A user's
  keyboard was confirmed working. Over the 2.4 GHz receiver the keyboard sends no analog values.
- ATTACK SHARK X82 HE over USB, with a built-in layout. A user's keyboard was confirmed working. Keyboards that
  report USB PID 5029 are now recognised; before, HallJoy rejected them and the keys worked only as ordinary keys.
- MCHOSE Ace 60 Pro (wired USB), with a built-in layout. A user's keyboard was confirmed working.

## Fixes

- MCHOSE Ace 60 Pro and other MCHOSE M HUB keyboards (Jet 75 II family): HallJoy now picks the active keyboard
  profile the same way the keyboard's firmware does. Before, some keyboards were rejected without any message and
  analog never started. A key the keyboard reports outside its known layout no longer stops analog input.
- ATTACK SHARK keyboards on a RongYuan USB ID: HallJoy no longer sends identity queries to the keyboard about once
  a second while it is already in use, which could raise a false "communication unstable" warning.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
