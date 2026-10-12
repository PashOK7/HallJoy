# SparkLink V2 platform survey (2026-10-12)

Owner question: three of the four recently checked keyboards (IPI Flash75 Pro, AULA AG75,
Everglide SU75 V2) turned out to be SparkLink V2. How many keyboards on this platform exist,
and does HallJoy already support some of them without knowing?

Data: official keyboard list (`getKeyboardListEncrypt`, decrypted 2026-10-12) and device catalog
(`getAllDevices`, 511 entries). Raw lists stay local: `.local/research/sparklink-survey-20261012/`.
The lists carry no model names (the driver shows the USB product string at run time), so models
are attributed through brand drivers and PID blocks only.

## Numbers

- Official SparkLink V2 keyboard identities: **552** (all usage page `0xFFB0`, usage 1). VID
  `1CA6`: 487; `38A6` (AULA): 35; other VIDs: 30 (`371C` 7, `3233` 6, `3973` 4, `36B5` 2, `38EE` 2,
  `39A5` 2, and single entries `391D`, `7968`, `17EF`, `2DC8`, `32CA`, `3151`, `3CA2`).
  Classes: public 454, other (brand hub) 42, shuangzixin+public 36, shuangzixin 23.
- HallJoy admits **30** SparkLink identities (exact list in `sparklink_model_profiles.h`);
  26 are in the official list, the 4 IROK/CAROTMAS ones (`0528`, `052A`, `052B`, `052D`) are from
  those brands' own clients. So **about 526 listed identities are not admitted.**
- No hidden support: SparkLink admission is an exact identity gate (2026-09-24 safety change), no
  other backend knows `1CA6`/`38A6`. The backend itself has no per-model code (rows, keys and
  layout come from the keyboard, up to 8 rows); the gate is the only model dependence.

## PID blocks look like customer blocks

The high byte of a `1CA6` PID tracks the brand for every brand with known PIDs:

| Block | Known brand (evidence) | Identities in list | Admitted |
| --- | --- | --- | --- |
| `05xx` | IROK, CAROTMAS | 28 | 4 (+4 not listed) |
| `08xx` | AULA (driver PIDs `0814 0834 0836 0839 083C 083E`) | 12 (4 driver PIDs are not in the list) | 0 |
| `1Axx` | IPI (`1A08 1A0F 1A10 1A11` in IPI's routing table) | 11 | 0 |
| `1Cxx`, `27xx`, `5E01` | EWEADN | 51 + 5 + 8 | 17 + 3 + 1 |
| `30xx` | Everglide (SU75 Pro `3002`) | 12 | 1 |
| `38A6:*` (own VID) | AULA | 35 | 0 |

About 90 further blocks (largest: `2C` 30, `54` 26, `39` 20, `45` 19, `09` 19, `1B` 15, `3D` 12,
`35` 11, `59` 10) belong to other brands that were not looked at.

## The three brands asked about

- Everglide: block `30`, 12 identities: `3001 3002 3003 3006 3007 3008 3009 300A 300B 300D 300E 300F`.
  Only `3002` (SU75 Pro) is admitted. SU75 Pro V2 uses the same web app; its PID is not known.
- IPI: block `1A`, 11 identities: `1A08 1A0A 1A0B 1A0C 1A0D 1A0E 1A0F 1A10 1A11 1A13 1A14`. Four are
  IPI Flash75 Pro candidates (`1A08 1A0F 1A10 1A11`); the other seven are not named by IPI's driver
  (other IPI models or other brands in the same block).
- AULA: `1CA6:08xx` 12 identities (plus `0836 0839 083C 083E` only in AULA's hub driver) and
  `38A6:*` 35 identities (`271C 272C..2731 2743 2745 2754 2763 28D0..28D2 28DB 28DD..28E1 28EF..28F2
  28F5 28F6 2902..2907 2909 290A 2912`). AG75 HE is `28DE 28DF 28E0 28E1 28F0 28F1 28F2`. Roughly 50
  AULA identities, 0 admitted; AULA's HE families already supported through other protocols (HERO,
  WIN, MINI 60) use different VIDs and are unaffected.

Potential for these three brands alone: about 74 identities (12 + 11 + 51), 1 admitted.

## What is not known

- Which identities are Hall-effect (analog) boards: the list covers every keyboard of the platform.
  The backend proves device info, layout and travel replies before it claims a device, so a
  non-analog board would be rejected, but this has not been tested on non-HE boards.
- Which model each PID is, and layout/ISO/JIS per PID.

## Options (owner decision: option 2 applied 2026-10-12, see [SPARKLINK_GENERIC_ADMISSION_2026-10-12.md](SPARKLINK_GENERIC_ADMISSION_2026-10-12.md))

1. Keep exact admission and add blocks with evidence (a tester log per model).
2. Generic SparkLink V2, as for BY/IPI on 2026-10-07 ([GENERIC_PROTOCOL_SUPPORT_2026-10-07.md](GENERIC_PROTOCOL_SUPPORT_2026-10-07.md)):
   admit exactly the official-list identities (the same devices the official drivers already send
   `01 02` to), fail closed on the existing proof, yellow "keyboard not in the list" notice,
   default layout, log line with the PID and USB product string. Needs the owner to relax the
   2026-09-24 rule from "exact model list" to "exact official-list identity".
