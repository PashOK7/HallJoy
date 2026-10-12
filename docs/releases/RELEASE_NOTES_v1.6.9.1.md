# HallJoy 1.6.9.1

Hotfix for 1.6.9.

- Fixed: the AULA MINI 60 HE PRO was mistaken for another keyboard that uses
  the same USB ID (Red Square Alumix 68). HallJoy showed a "Red Square Alumix 68:
  unrecognized firmware" notice and the AULA keyboard could not connect. The
  AULA MINI 60 HE PRO is now recognized by its own name and identity, including
  units whose firmware reports a different internal product code.
- Red Square Alumix 68 support is switched off for now, so it can no longer
  interfere with AULA keyboards.
- The support report now records the identity values of MINI 60 keyboards, which
  makes connection problems easier to diagnose.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
