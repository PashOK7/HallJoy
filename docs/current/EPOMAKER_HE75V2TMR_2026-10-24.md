# EPOMAKER HE75 V2 TMR (board 3613): Supported — 2026-10-24

## Report

A user sent `HallJoy-HE75-TMR-for-developer.zip` (kept locally in `.local/research/he75v2tmr-user-20261024/`,
read as data, nothing applied or executed): a README, two git patches against upstream a405b1f, a layout preset
and a HallJoy.log from the user's own patched 1.6.11 build.

- Keyboard: USB `3151:5030`, product string "HE75 V2 TMR", firmware ID3613_V409; answers the 0x8F board query with
  3613; units 100/mm (`rongyuan.connected value=3613 detail=100`). ~6 min session: 11461 updates, 1 failure
  (`rongyuan.admission_command value=138 error=13` once at start, retried), clean session end, no communication
  warning. User: in The Witcher camera and walking speed follow key depth smoothly.
- Patch 01: own profile 3613 on pid 5030 with the board's matrix, precision enum required, 4 mm provisional, layout
  from gearhub, a new evidence type and checker. Patch 02: the same false "unstable connection" fix that HallJoy
  already has locally since 2026-10-22 (Shark wait heartbeat, no anomaly for HallJoy's own stream handle).

## Verified independently

- Official EPOMAKER gearhub registry (`https://gearhub.top/v4/js/index.91333ef6.js`, SHA256 cf7abeea...) and the
  pinned WOMIER 3.2.15 catalog: `{id:3613,vid:12625,pid:20564,keyLayout:Common82_GK8642_v2,
  name:"ry5088_jc_8642_tmr_3m_8k_8k",displayName:"HE75 V2 TMR"}`. Same keyLayout as HE75 V2 (3518).
- No vendor registry lists 3613 on 3151:5030 (searched all local vendor copies). The tuple comes from the device log.
- The user's matrix SHA256 (b001e55a...) equals the matrix of the pinned catalog class `1666eace.js` (parent
  7a5b12c9.js, clean inventory). The user's layout preset equals our extraction exactly (81 keys, same positions).

## Integration (our pipeline, not the user's patch)

- Board 3613 added with `tools/rongyuan_batch.py` at the catalog identity 3151:5054 (EPOMAKER / HE75 V2 TMR,
  3500 um provisional like HE75 V2, precision enum not required: units are still read, 100/mm in the log).
- 3151:5030 admitted as a reviewed USB alias of 3613/5054 (`kUsbAliases`), new alias kind `device-log` in
  `approved-aliases.json` and `catalog-comparison.json`: the official record carries the catalog tuple, and the
  pinned excerpt `docs/research/usb-identity-audit/he75v2tmr-3613-device-log.txt` shows the keyboard answering
  board 3613 on 5030. `audit_keyboard_identities.py` checks this kind; protocol test covers both tuples.
- Layout: `build_epomaker_layouts.py` now also extracts 3613: "EPOMAKER HE75 V2 TMR ANSI" (81 keys, gearhub
  Common82_GK8642_v2 geometry, key set equal to the 3613 factory matrix; identical to HE75 V2). Layout reports
  re-pinned, catalog updated, `layout_pipeline.py integrate`.
- Supported: confirmed_devices token 0x6925daef3fae6969 for pid 20564 and 20528 (the notice compares the PID the
  keyboard reports); `check_rongyuan_stream_profiles.py` resolves approved alias PIDs for confirmed devices.
  supported_layouts, README, SUPPORTED_HARDWARE.

Not done: the user's precision-enum requirement (HallJoy reads units anyway) and their checker/evidence type.
Wireless (receiver 5038) not admitted.

## Checks and Sheet

- First build failed correctly: the layout catalog audit rejects identical same-brand presets. Added the reviewed
  merge `EPOMAKER HE75 V2 / HE75 V2 TMR ANSI` to `g_layoutMerges` (keyboard_layout.cpp), as for M1 V5 HE / TMR.
- KEYBOARD_IDENTITY_AUDIT PASS (6 aliases), test_rongyuan_batch OK, research_reference_checks --record PASS, native
  checks EXIT=0, build_release EXIT=0 (LAYOUT_CATALOG PASS, SUPPORTED_LAYOUTS PASS 78). EXE SHA256
  `90c89f4a4c72655c444e011a8b05ce621f1b80a5cd90eff3d8306ed3cf279d0c` (local, not released).
- Publication policy: new vendor chunk `docs/research/rongyuan-stream/1666eace.js` denied. Found while checking:
  `docs/research/remaining-layout-sources-20260914/atk-index-M8vylvoC.js` (older ATK vendor bundle) is not in
  denied_paths; left for an owner decision.
- Sheet: Main!C259 EPOMAKER / HE75 V2 TMR "Not investigated" -> "Supported", B259:C259 green (revision-guarded;
  intended state PASS with the three --sheet gates first). Readback A258:C260: value, ONE_OF_LIST, colors,
  neighbours unchanged.
