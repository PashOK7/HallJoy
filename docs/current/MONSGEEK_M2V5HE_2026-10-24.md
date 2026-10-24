# MonsGeek M2 V5 HE: user log, layout, Supported — 2026-10-24

## User report and log `message (13).txt` (HallJoy 1.6.11.0)

User: everything works, but "unstable connection" sometimes showed although nothing else was running, and it
went away by itself. The user also sent a self-made layout `M2 V5 HE.ini`.

- Keyboard `3151:5030` "MonsGeek Keyboard", board 2845 connected on the RongYuan stream
  (`rongyuan.connected value=2845`, reader MMCSS Games + HIGHEST). `rongyuan-stream` failures=0.
- `keyboard.communication_warning value=1` at start-up comes from the Attack Shark backend probing the same
  keyboard: `shark.exclusive_open_failed error=32` (HallJoy's own RongYuan reader holds the handle), then
  `shark.identity_stream_owned 2845`. The communication-health window clears the warning after ~30 s without a new
  anomaly, which is the "fixed itself". Same cause as the M1 V5 TMR warning; fixed locally on 2026-10-22
  (MONSGEEK_M1V5_TMR_LAG_2026-10-16.md: heartbeat while waiting, no anomaly for HallJoy's own stream device).
  Not in 1.6.11.

## Layout

Our layout builder (`build_rongyuan_layouts.py`, ATTACK SHARK client SVGs) held board 2845 ("plain key-mapping
SVG absent"), and the RongYuan CN/IOT clients only have the other M2 V5 board (`ry5088_mjk_m2v5_8k_8k`).

The official MonsGeek web driver (`https://app.monsgeek.com/`, main `index.dc20bdb4.js`
SHA256 386955e9...) registers board 2845 (`ry5088_akko_m2v5_3m_8k_8k`, "M2 V5 HE") with keyLayout
`Common98_MonsGeek_M2V5`; its key-mapping UI info is chunk `234b54ea.js` (SHA256 7ce02b97...), the same object
format as the M1 V5 geometry. `tools/extract_monsgeek_m2v5he_2845_layout.py` (pinned, `--acquire` / check)
writes `docs/research/rongyuan-stream/sources/m2v5he-2845-geometry.js` (publication denied) and the report
`docs/research/rongyuan-stream/monsgeek_m2_v5_he_ansi.json`: 98 keys, key set equal to the factory matrix
(including Fn), no overlaps, scaled 1.05 like M1 V5. Layout catalog entry added, `layout_pipeline.py integrate`;
preset "MonsGeek M2 V5 HE ANSI", token 0x844A661FF1F203A0.

Comparison with the user's `M2 V5 HE.ini` (HallJoy layout preset, 97 keys): the user layout has no Fn key, and
45 keys differ by more than 8 px in position or width (narrower Backspace 90 vs 116, Backslash 58 vs 88, Enter 96
vs 130, right Shift 84 vs 108, wider left Shift 106 vs 97; F row and the navigation/numpad columns shifted).
The built-in layout uses the manufacturer geometry, not the user's drawing.

## Publication policy

Vendor sources added to `docs/legal/PUBLICATION_POLICY.json` denied_paths: this geometry, the two GamaKay
TK75 HE V2 chunks added on 2026-10-23 (`33368eb1.js`, `f3f563b2.js`) and the 2026-10-21 geometry excerpts
(`mars68pro-2438-geometry.js`, `sk75tmr-2518-geometry.js`), which had been missed.

## Supported (2026-10-24)

The user reports the keyboard works; with the built-in layout the criteria are met. Board 2845 moved to
`confirmed_devices` (token 0x844a661ff1f203a0, pid 20528); supported_layouts entry; README and SUPPORTED_HARDWARE.
Same change for GamaKay TK75 HE V2 board 3167 (owner: tester log `HallJoy (14).log` shows board 3167 connected,
"everything worked"); board 3138 stays experimental.

Checks: research_reference_checks --record PASS, native checks EXIT=0, build_release EXIT=0
(SUPPORT_NOTICE_CATALOG=PASS 285, SUPPORTED_LAYOUTS=PASS 77). Intended Sheet state PASS with the three --sheet gates.
EXE SHA256 `019f349ec114a0f99e7fc2607d7bcd07320f66850f28cd19b7af188405eb3f4e` (local, not released).
Sheet (one revision-guarded batch, value and format requests): Main!C331 GamaKay / TK75 HE V2 and Main!C581 MonsGeek /
M2 V5 HE -> "Supported", B:C green pair. Readback A330:C332 and A580:C582: values, ONE_OF_LIST, colors, neighbours
unchanged. Backup `.local/backups/supported-tk75hev2-m2v5he-20261024/`. Not tested by the agent on hardware or visually.
