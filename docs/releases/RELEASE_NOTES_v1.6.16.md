# HallJoy 1.6.16

## Now supported

- EPOMAKER HE108 over USB and 2.4 GHz. Fixed wireless stream startup being blocked
  by an optional depth-table reply. The owner confirms wireless operation after
  the tester rebooted their PC; the wireless testing notice is removed.
- MonsGeek FUN60 Pro over USB, following tester confirmation. Added its automatic
  ANSI layout, combined with FUN60 Ultra TMR in the picker while preserving saved
  layouts. Existing 2304, 2600 and 2785 revisions use their reviewed input paths.
- MCHOSE Ace 68 Air II over USB, following positive feedback. Air III remains
  supported. The shared automatic ANSI layout is retained; Air 2 is unchanged.
- FREEWOLF F68 over USB, with an automatic ANSI layout and both known USB revisions.
- LANGTU BOOM68-USB on stock firmware 1.10, with an automatic ANSI layout.

## New and improved experimental support

- Added wired Akko MOD007 V5 HE board 3113 through read-only depth snapshots and
  an automatic 80-key ANSI layout. The older 2453 path is retained.
- Added wired MCHOSE Ace 60 with an automatic ANSI layout. Stock 1.20 has reviewed
  activation and normal-exit restoration; other firmware reports activation
  status in Open log when its depth stream is available.
- Added HATOR Skyfall 80 MAG Ultima 2.4 GHz support and ANSI/ISO layouts for six PCBs.
- Added wired MCHOSE Ace 68 GT input and automatic ANSI layout, tested on stock 1.16.
- Added wired ARDOR GAMING Viper input from full ADC rows, with an automatic
  82-key ANSI layout. The tester confirms operation; depth scaling is provisional.
- Added wired Akko 5087 V3 HE board 3131 with an automatic ANSI layout and read-only
  depth snapshots. Hardware validation remains pending.
- Added wired MCHOSE GT75 input and an automatic ANSI layout. The tester reports
  operation. Its shallow-depth cutoff and mode flag remain documented; normal
  exit restores the flag change made by HallJoy.

## Fixes and availability

- Fixed late Keydous NJ98-CP V4S wireless depth replies being discarded and
  restored the manufacturer command handoff interval to prevent a reproduced
  mailbox overwrite. USB stays confirmed; wireless multikey retest is pending.
- Improved wireless cleanup when a table reply is still pending and accepted
  split HID capability descriptions for the HE108 interrupt input.
- Fixed truncation of Sonix keyboard diagnostics in Open log.
- AULA F75 HE now shows a red unavailable-support notice on stock firmware;
  repeated stock probes are disabled and research is retained.
- Removed experimental Logitech G PRO X TKL RAPID support: the stock stream can
  omit releases during simultaneous input.
- MIDI remains disabled in this release while development continues.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).

## Build tools

- Windows compiler processes now share a 90% CPU limit and Below Normal priority
  to leave CPU time for other applications. Incremental caches remain enabled.
