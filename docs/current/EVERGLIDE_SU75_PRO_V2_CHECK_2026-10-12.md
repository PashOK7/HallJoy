# Everglide SU75 Pro V2: support check (2026-10-12)

Owner request: check whether an "SU75 v2" keyboard can be added. Research only; no code
change. The SU75 Pro (`1CA6:3002`) is already Supported, see
[EVERGLIDE_SU75PRO_2026-10-02.md](EVERGLIDE_SU75PRO_2026-10-02.md).

## What the model is

- Retailer listings (KeebForce, "SU75 Pro V2 Barebones Kit") sell a second revision of the
  SU75 Pro: 75%, 81-key ANSI, wired USB-C only, 8000 Hz polling, "3x240 MHz MCU" and
  "full-key ADC", magnetic switches below 680 Gs. The listing does not state how it differs
  from the original beyond the controller architecture.
- Its driver is the same web app as the SU75 Pro: the V1/V2 page links to
  `https://www.xsyd.top/connect` ("Su75 Pro Web APP"). That is the SparkLink PlayJoy platform
  HallJoy already follows. SparkLink is not named in the listing.
- No public source gives its USB VID/PID or product string.

## Official driver, re-checked 2026-10-12

- `xsyd.top/connect` still serves `index-DwbZ9fGF.js`; `/v2-app/` still serves
  `index-D-Vi6Arc.js`. Both are byte-identical names to the 2026-10-02 snapshot, so the
  command set (`01 02`, `03 01`, `04 03 01`) and the catalog class are unchanged.
- The V2 app has no per-PID branches (no productId comparisons, no model names); it works from
  device replies only.
- Fresh catalog (`getAllDevices`, public client header, AES-CBC key from the page): 511
  entries, 507 on 2026-10-02. Added: `1CA6:1A14`, `1C51`, `4506`, `6C05`, `7903`. Removed:
  `1CA6:6C01`. Nothing changed in existing entries. Every `1CA6` entry is class
  `public`/`shuangzixin*`, protocol `v2`, usage page `0xFFB0`, usage 1. The catalog carries no
  model names, so none of the five new PIDs can be tied to the SU75 Pro V2 from data alone.
  `3002` is unchanged.

## Verdict

- If the V2 reports `1CA6:3002`: it is already admitted; the backend proves device-info,
  layout and travel replies before claiming it. Nothing to add.
- If it reports another PID: the change is an exact-identity addition in
  `sparklink_model_profiles.h` (same pattern as the SU75 Pro), the existing 81-key layout
  "Everglide SU75 Pro ANSI" applies (manual selection), then `supported_layouts.json` and the
  Sheet row follow the "Supported only with a layout" rule. Protocol work is not expected.
- Not established: the PID, and whether the three-MCU design changes the HID interfaces or the
  row count that `04 03 01` returns. The backend fails closed if replies do not match.
- Adding all five new catalog PIDs is not proposed: it would send `01 02` to keyboards of
  unrelated brands (2026-09-24 exact-identity safety rule).

## Needed to proceed

One support log from a HallJoy build that logs product names (`hid.candidate name=`), or the
Device Manager product string and hardware ID (`VID_1CA6&PID_xxxx`) of the V2.

Local files: `.local/research/everglide-20261002/` (old catalog) and the scratchpad copy of the
2026-10-12 catalog diff (not kept in the repository).
