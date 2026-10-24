# Red "not supported" keyboard notice — 2026-10-22

Owner request (after the AK820 MAX `0C45:80B1` user log "message (11).txt"): users with a
keyboard that HallJoy will not support saw the generic red banner "No supported analogue
keyboard detected / Want to help add support for your analogue keyboard?" with Discord
buttons. That invites a support request for a decided case. Every red Sheet row must instead
get a banner that says plainly the keyboard will not be supported.

## Catalog

`docs/development/unsupported_keyboards.json` lists every red row of the live Sheet (read
2026-10-22, `.local/sheet-red-read-20261022.json`, 10 rows). `tools/unsupported_keyboard_catalog.py`
generates `src/HallJoyProject/HallJoy/unsupported_keyboard_catalog.h`; `--sheet <fresh read>`
fails unless the red rows equal the catalog (brand, model, status). `build_release.ps1` checks
that the header matches the JSON.

| Sheet row | Detected by | Notes |
|---|---|---|
| AJAZZ AK680 V2 | `0C45:80BC/80C1/80C2` + product "AJAZZ AK680 V2" | configurator names; 80C1 shared with a TFT config, 80C2 with 1stplayer |
| AJAZZ AK820 MAX HE (wired, RGB, Sonix 0C45:80B1) | `0C45:80B1` + product "AK820MAX" | tester logs; PID shared with an "AJAZZ KEYBOARD" TFT config |
| Corsair K70 MAX | `1B1C:1BC0` | Corsair firmware catalog |
| MADLIONS TITAN 68 Turbo | `28E9:31FD` | identity of the earlier Titan68 diagnostic backend |
| YUNZII RT80 | `3151:4035` + product "RT80" | tester log; PID shared by four RongYuan boards |
| Fantech ATOM HE68 (MK811) | not detected | USB identity not established |
| IO Type 84 Magnetic (FW 1.17) | not detected | `0C45:80D6` shared with Type 84 Magnetic Pro; red only for FW 1.17 |
| Pwnage Zenblade 65 V2 | not detected | `3662:1001/1002` shared with Zenblade 65 (not red) |
| YUNZII RT75 | not detected | `3151:4015` shared; product string unknown |
| MADLIONS MAD 68 V2 Dual | not detected here | "Analog available; low quality": keeps its own limited-analog notice |

An undetected row keeps the generic red banner until its exact USB identity (and product
string where the PID is shared) is known; then add it to the JSON and regenerate.

## Runtime

- Detection reuses the metadata-only USB scan that feeds the frozen-model notices
  (`native_layout_devices.cpp`, SetupAPI hardware ID + bus-reported description, upper case,
  trailing spaces ignored). No device is opened.
- `keyboard_ui.cpp` asks for it only while the search is complete and no analog source is
  connected; the 1-based catalog index travels in the status snapshot (bits 35..42).
- Banner (`keyboard_page_main.cpp`), shown only when no yellow/limited/busy/communication notice
  applies: red style, title "<model>: not supported", body "HallJoy has checked this keyboard:
  its firmware does not provide analog key travel that HallJoy can use, so this keyboard will
  not be supported. It keeps working as a normal keyboard." Only the Open log button; no
  Discord invite or QR.
- Not an automatic log incident (`ShouldAutoSaveSupportLog`) unless another notice applies.
- Log: `support.unsupported_keyboard value=<index>` on change; every `snapshot.begin` line ends
  with `ui_unsupported=<index> ui_unsupported_model="<name>"`.

## Validation (agent, no hardware, no visual check)

- `unsupported_keyboard_catalog.py --write --sheet .local/sheet-red-read-20261022.json`: PASS, red=10, detected=5.
- `keyboard_support_status_test.cpp`: exact matches, product-string gates for shared PIDs (80B1 vs the TFT
  "AJAZZ KEYBOARD", 80C1, RT80 vs AGK75), padded USB strings, MAD 68 V2 Dual and RT75 PIDs not matched, snapshot
  round trip of the index next to frozen flags, no auto-saved log for an unsupported-only state.
- `keyboard_support_status_static_audit.py`: detection stays in the metadata-only scan (new check).
- `run_native_backend_checks.py --require-compiler` (HALLJOY_NO_TEST_CACHE=1, WinLibs first on PATH): EXIT=0.
  Log `.local/native-checks-red-notice-20261022.log`.
- `build_release.ps1`: EXIT=0 (new catalog gate, MSVC Release, candidate self-tests, profile/layout tests,
  SUPPORTED_LAYOUTS=PASS models=74). Log `.local/build-red-notice-20261022.log`.
  EXE SHA256 `d5293c1b568232e30d3c9e592f4321ebf7a806f321038e5083555338202c2442` (local, not released).
- Not checked by the agent: the banner on screen (owner checks visuals) and detection with a real red-row
  keyboard plugged in.

Backups: `.local/backups/red-unsupported-notice-20261022/`.

Side note: Red Square Alumix 68 Yotei is switched off in code (2026-10-12) but its Sheet row is
"Research frozen; tester needed" (gray), not red, so it is not in this catalog.

## Owner decision (2026-10-22)

The four undetectable red rows (Fantech ATOM HE68, IO Type 84 Magnetic, Pwnage Zenblade 65 V2, YUNZII RT75) stay
on the generic banner until a log shows their USB identity (VID:PID and product string). Do not add them by
guess or by a shared PID. When such a log arrives: add the `usb` entry to `unsupported_keyboards.json`, run
`tools/unsupported_keyboard_catalog.py --write --sheet <fresh read>`, rebuild.
