> 2026-10-08 AK820 80B1 probe log: 0x68 = echo only (frame_version 0); stream carries several keys but is
> change-only, so the MINI60 50 ms expiry released still held keys = the "one key" bug. Fix (80B1 only): hold last
> depth, release on 0 or inside the keyboard's top dead zone (GET_GAME_MODE 0x11), Driveall packet rule; rejected
> packets logged. Firmware DOES provide the data; not "unsupported". EXE 3a4e06e2. See AJAZZ_AK820MAX_SONIX_80B1.
> 2026-10-08 OWNER: AK820 80B1 probe build, no time/log limits. Official commands only: raw device info
> (frame_version/rt_precision), 0x68 status read outside calibration at rest + per new held-key set, stream 0x66;
> new SupportLog_Evidence store (kept whole + first 2000/newest 8000 stream records), chord summaries.
> Digital key events are diagnostics only, never analog (owner). Memory reads outside documented data are
> not used. EXE 20b60e5e. See AJAZZ_AK820MAX_SONIX_80B1 doc.
> 2026-10-08 AK820 80B1 log 16: connected (82 keys, stream OK) but log had no chord/key-record data (agent error,
> owner angry). Screenshot: WASD "Unassign" = Driveall advanced keys (SOCD/RS/DKS/MT/TGL pages 8..12) -> now keep
> factory key (AK820 only). Ordinary log capture store now has worker identity/records/checkpoints (capture.ak820w),
> press/release/expire/chord samples (capture.ak820) and capture.key. EXE 7d0a3558. See AJAZZ_AK820MAX_SONIX_80B1 doc.
> 2026-10-08 OWNER: support the user's AK820 MAX HE 0C45:80B1. Done as yellow on the MINI60 stream backend
> (protocol 16): official Driveall config 3141:32945 "AK820MAX" = same AA/55 commands, 55 FB stream, units and
> key-record format as MINI60; 82-key map from Driveall; existing AK820 MAX HE ANSI layout token; notice group
> AjazzAk820Sonix. No firmware published (Driveall API empty; ajazz.net "AK820 Max Driver" is the mechanical BY
> model). PENDING: Sheet row "AK820 MAX HE (wired, RGB, 0C45:80B1)" yellow (no Sheets connector this session).
> All gates PASS (build_release + native checks), EXE 500c9eea. See AJAZZ_AK820MAX_SONIX_80B1_2026-10-08.md.
> 2026-10-08 OWNER: Discord invite expired -> new https://discord.gg/X5QsZJdN5a in app (community_links.h +
> regenerated banner QR, decoded back with OpenCV), README/docs/static audit. Needs a rebuild to reach users.
> See DISCORD_SETTINGS_2026-09-09.md latest section.
> 2026-10-08 Log "message (15).txt" (user claims AK820 MAX HE): device is 0C45:80B1 "AK820MAX", a different
> revision from the supported 0416:7372 RGB unit; nothing admitted. No support change. See AJAZZ_AK820MAX_REVIEW.
> 2026-10-07 OWNER: finish Red Square Alumix 68 and give the tester an EXE bound to the firmware version.
> CORRECTION of 2026-10-06 line below: the owner overruled the rejection; the v1.30-only path (exact
> scan-map + dispatcher-code fingerprint, fail closed + logged otherwise) IS the support path. Backend
> shipped since 2026-10-06; now documented (ALUMIX68_RSQ20058_2026-10-06.md), catalog name = Sheet name
> "Alumix 68 Yotei (Magnetite Ice)", README/SUPPORTED_HARDWARE, Sheet Main!C658 yellow. Live checks:
> support_notice_catalog --sheet PASS (287), sheet structure PASS (150). Tester EXE b8f43358. Pending item below resolved.
> 2026-10-07 Sheet access restored (owner reconnected Google Sheets). Live Main!C321 FuryCube M35HE ->
> Implemented; awaiting hardware testing (yellow, generic BY); structure PASS (150 blocks). PENDING owner
> decision: Red Square Alumix 68 backend (protocol 31, redsquare_alumix68_backend.cpp, 2026-10-06, yellow
> notice, in every build) has no current doc, OWNER_CONTEXT still says "rejected", catalog model name
> "Alumix 68" != Sheet "Alumix 68 Yotei (Magnetite Ice)" which is still Not investigated ->
> support_notice_catalog.py --sheet fails only on this row.
> 2026-10-07 OWNER: yellow support for every keyboard on a known protocol + anonymous 100% default layout.
> Done for the safe families: BY/IPI (any 372E except HERO84; unknown UUID -> keys/map/calibration read
> from the keyboard) and Logitech HID++ 0x1B08 (any 046D device reporting it; keys learned). RongYuan NOT
> generic (shares VID/PID with ATTACK SHARK). New notice flag GenericProtocol. Default preset = "Generic
> 100% ANSI". FuryCube M35HE covered by the BY path. EXE b8f43358. See GENERIC_PROTOCOL_SUPPORT_2026-10-07.md.
> 2026-10-07 FuryCube M35HE (log 58, 372E:10A3): BY/IPI platform, UUID 0x110000000065 (official firmware
> V1.6 + web driver he.furycube.com). Firmware has the 94/02 live read HallJoy uses; 37 keys. Not admitted
> (10A3 not on the IPI path). Research only, no build. See FURYCUBE_M35HE_2026-10-07.md.
> 2026-10-07 Logitech + FUN60 2304 + layout-save build: EXE a47370a0 (all gates PASS).
> 2026-10-07 OWNER: fork BoneDrk/HallJoy found (build-time patch for FUN60 Pro board 2304, 3151:502D).
> Only its facts used, no code (dual license). Official MonsGeek driver: board 2304 class has the exact 2600
> matrix (61 keys incl. [ ' \) -> USB alias with canonical board 2600; telemetry/log use the reported board.
> FUN60 Pro stays yellow. See FUN60_PRO_2304_2026-10-07.md.
> 2026-10-07 Logitech PRO X2 RAPID log V6 (every key pressed once): 28 learned pairs + PrtSc (key-up only)
> and Fn (raw capture) -> all 84 keys in the static `kKeyMapV2`. Stays yellow. See LOGITECH_RAPID doc.
> 2026-10-07 Logitech PRO X2 RAPID: tester confirms analog in the gamepad (2 games); F2-F12, arrows, some
> system keys not analog (not in table). Log v5: 5 = 0x0f, 6 = 0x0e (54 ids). New run-time learning: an
> unknown id is paired with the Windows key-down of the same keyboard (unique candidate only), analog from
> the 2nd press, logged as `logitech.learned_key`. Offline on logs v4/v5: 42 learned, 0 wrong. Yellow. EXE 28e97567.
> 2026-10-06 OWNER: "automatic layout resets after restart". Cause: with a named global profile active
> (owner: `123`) the save path never wrote [KeyboardLayout] (global, base settings.ini only). Fixed with an
> atomic layout-only base-file save + regression test. EXE cb738c6b (also has Logitech v2). See LAYOUT_SAVE_NAMED_PROFILE_2026-10-06.md.
> 2026-10-06 Logitech PRO X2 RAPID logs v4 (+GHUB): format decoded from the capture -> analog published.
> Frames of 5 x [key id][depth16 0.01 mm] per report (`more` byte chains up to 10 keys), released keys
> absent; one-frame hold for lost reports. 52 key ids mapped (49 seen, 3 by column rule; WASD, Space,
> Shift, Ctrl, digits 1-4/7-0, letters all mapped); F2-F12, 5, 6, arrows, nav, right mods not yet:
> logged + raw capture keeps running to complete them. Stays yellow. EXE 7b863a5f. See LOGITECH_RAPID doc.
> 2026-10-06 Logitech PRO X2 RAPID log "Logitech v3" (new build 422776fa): stream on, travel 4.00 mm
> (16-bit 0.01 mm reading confirmed), but the first analog event is a multi-record packet (data in payload
> bytes 1,3,4,7,10,13), not [key][depth16] -> depth_format_mismatch, nothing published (correct). Now an
> unverified feature version publishes nothing and captures the raw stream. OWNER: no time limits, no
> instructions; "open HallJoy, press anything 5-10 s, send the log" must suffice -> new content-bounded
> capture store in the support log (`SupportLog_Capture`, first 8000 records, never evicted, no queue):
> `capture.logitech` raw reports for the whole session + `capture.key` first presses of every key as
> physical reference. Stays yellow. EXE 132d74c2. See LOGITECH_RAPID_2026-10-05.md.
> 2026-10-06 OWNER: (1) no support that depends on one firmware version's internals (e.g. Alumix68
> v1.30 out-of-range 0x16 RAM read): an update would silently break it -> rejected as a support path.
> (2) Logs must carry the data needed to find a bug; over-anonymized aggregates are useless. Tester
> checked: Invert is off. Added bound-key input-chain trace (trace.src/os_key/pad/bindings) to the
> ordinary log. EXE b20cd238. See SUPPORT_DIAGNOSTICS_CONTRACT.md 2026-10-06.
> 2026-10-06 OWNER: refusing a profile in K4 onboard mode locked users out of Default -> profiles are
> never refused for backend limits; the onboard notice is advisory after activation (GAME_PROFILES_V2).
> Redragon K686 log 57: releases == presses, 0 residual keys -> sticking fixed. New tester report
> "sticks reversed, triggers always pressed" matches an inverted curve (global/per-key Invert) or a
> DirectInput view, not W669; config was not logged -> new aggregate `input.config` snapshot line.
> Stays yellow. Logitech log v3 was the old build (no new data). EXE b10a1843.
> 2026-10-05 Logitech PRO X2 RAPID log v2: found, 0x1B08 is version 2 (info "02 05 80 01 ..", TKL v0 had
> travel at byte 3); not connected (nothing published). Now version-aware: v2 tries 16-bit travel in
> 0.01 mm (3..6 mm) with depth checks; otherwise a 30 s shape probe logs the format. EXE 422776fa.
> 2026-10-05 OWNER: Logitech PRO X2 RAPID (046D:C364) + PRO X TKL RAPID (C35B) -> Implemented (yellow):
> new backend `logitech_rapid` (protocol 30), HID++ feature 0x1B08 live depth stream (0.1 mm), facts
> from RigDeck notes (GPL, no code copied). Risks: lossy stream, C364 key ids/feature unverified (logged).
> PRO X2 RAPID is a TKL (84 keys, OLED/roller instead of Pause/PgUp/PgDn); automatic layouts for both.
> EXE 17b3f4b5. See LOGITECH_RAPID_2026-10-05.md.
> 2026-10-05 WLMOUSE Ying75 -> Supported (log 55: connected, 84 keys, 7182 updates, 0 failures; Sheet
> C775 green, notice excluded by token+PID). Redragon K686 log 56 works but tester says buttons stick:
> stays yellow; release floor at the keyboard's minimum actuation + `w669.release` aggregate evidence.
> EXE a068ebe0. See WLMOUSE_YING75 / REDRAGON_K686 docs.
> 2026-10-05 Ying75 log 54: open failed with ERROR_SHARING_VIOLATION (another program holds the
> vendor interface, likely the WLMOUSE Web Hub tab or an RGB service). Ying75 now falls back to a
> shared open with foreign-reply detection. EXE 2aaf8fbb. See WLMOUSE_YING75_2026-10-05.md.
> 2026-10-05 WLMOUSE Ying75 log 52 (local build): detected but never connected (failures 1->9).
> Official client checks no status/checksum and reads 3 reports per travel reply; Ying75 now uses
> those rules plus a factory-map proof, and the support log names any admission failure
> (`mg75.admission_failed`). EXE f70dd8d8. See WLMOUSE_YING75_2026-10-05.md.
> 2026-10-05 OWNER: fix "HallJoy is paused" at start even though it cannot be reproduced. Found:
> startup Resume times out on a busy UI thread and the engine/UI bridge then refuses every later
> request until restart. Fixed the bridge, added automatic Resume retries, a working Resume button,
> honest card text and step diagnostics. See STARTUP_PAUSE_FIX_2026-10-05.md.
> 2026-10-05 WLMOUSE Ying75 (`36A7:F887`) -> Implemented (yellow): same JingTai protocol as IROK
> MG75 Pro; factory matrix and range from the official firmware, layout from the official Web
> Hub width table, Sheet brand block inserted. See WLMOUSE_YING75_2026-10-05.md.
> 2026-10-05 Redragon K686 HE (K686BG-RGB-M US/BR, K686RGB-M UK; `2E3C:C365`) -> Implemented
> (yellow): exact W669 profiles from official iLLumiPC key files, three automatic layouts, Sheet
> C668. Tester log showed it not admitted in 1.6.7. See REDRAGON_K686_2026-10-05.md.
> 2026-10-05 MCHOSE Ace 68 Air III (`41E4:2132`) -> Supported: tester log 14 (1.6.7, 67 keys,
> 0 failures). Sheet row "Ace 68 Air" split into II revision / III revision (green) / Air 2;
> notice excludes PID 0x2132; README + SUPPORTED_HARDWARE updated. The tester's "paused at
> start" is fixed separately (see STARTUP_PAUSE_FIX_2026-10-05.md).
> See MCHOSE_ARM_FAMILY_2026-10-02.md latest section.
> 2026-10-03 OWNER: tab transition duration 0.2 s (was 0.3), quintic ease, motion blur on.
> 2026-10-03 OWNER: one paint standard for all tabs (CustomPagePaintScope: pooled DIB, dirty clip,
> WM_PRINTCLIENT) and 0.3 s camera tab transitions on a Direct2D layer (fly-through, retarget with
> velocity, resize-safe, moving indicator). See TAB_TRANSITIONS_2026-10-03.md.
> 2026-10-03 OWNER: K4 with HJO firmware must use onboard only, re-enumerate only at HallJoy
> start/exit (no deferred stop, no permanent pad). Host fix: REV_1212/1213 metadata marker, wait for
> a returning K4, UAP exclusion, automatic late takeover. Firmware unchanged. See K4_ONBOARD_ONLY_2026-10-03.md.
> 2026-10-03 OWNER: game profiles v2 built locally (not published). Profile selector in the tab
> strip, custom Profiles tab, EXE-name/title rules, alt-tab setting (default keep while running),
> optional notification (off), next/auto/per-profile shortcuts, .hjprofile import/export. Default
> unchanged; profile file format unchanged. EXE 1e95ccfe. See GAME_PROFILES_V2_2026-10-03.md.
> 2026-10-03 PUBLISHED stable/latest 1.6.7: https://github.com/PashOK7/HallJoy/releases/tag/v1.6.7 .
> Owner checked layouts visually and authorized publication. Commit 35d879f, EXE SHA256 5b49adb2...;
> 60 assets, prior 56 unchanged; CI success. See RELEASE_1.6.7_PUBLICATION_2026-10-03.md.
> 2026-10-02 OWNER RULE: when all Supported criteria are met (working path, built-in layout,
> supported_layouts.json, check_supported_layouts PASS), the agent sets Supported itself; trust the
> tester's model name, do not ask for more logs. Applied: Everglide SU75 Pro (tester log 13: 126 keys,
> 0 failures; new 81-key layout from the official driver key map) and Royal Kludge RK68 HE -> Supported
> in README, SUPPORTED_HARDWARE, Sheet; yellow notices removed. See EVERGLIDE_SU75PRO / RK68HE_HUBX.
> 2026-10-02 OWNER: checks too slow. Parallel test compiles + exact-input binary cache, parallel
> research replays/layout checks, MSVC /MP. Same checks. See FAST_CHECKS_2026-10-02.md.
> 2026-10-02 OWNER: support log names keyboards. `hid.candidate` rows now carry
> `name="..."` = USB bus-reported product string (metadata only, no device open;
> sanitized, max 48 chars). Next tester logs show the model name, not only VID:PID.
> See SUPPORT_LOG_PRODUCT_NAMES_2026-10-02.md.
> 2026-10-02 Everglide follow-up: the log proves only an unnamed SparkLink V2 keyboard
> `1CA6:3002`; model name SU75 Pro is unconfirmed (tester may not own one). Ask the
> tester for the exact model / Device Manager product name before trusting the name.
> 2026-10-02 RK68 HE tester log: works (connected, 68 keys, 17982 updates, 0 failures).
> Still yellow pending owner decision on Supported.
> 2026-10-02 Everglide SU75 Pro (`1CA6:3002`): SparkLink V2 (xsyd.top / sparklinkplayjoy
> catalog: public v2 FFB0), same 01 02 / 03 01 / 04 03 01 commands as IROK/EWEADN; only
> missing from the exact identity list. Added + notice + Sheet brand row (283 yellow PASS).
> See EVERGLIDE_SU75PRO_2026-10-02.md.
> 2026-10-02 IO/Pwnage recheck: IO manifest, configurator and Disk unchanged (no
> Magnetic Pro firmware). Pwnage hub 3.4.4 re-uploaded (47 bytes differ), same PIDs, no
> live-travel read; community macOS app v0.1 also none; drivers page timed out. See
> IO_PWNAGE_RECHECK_2026-10-02.md.
> 2026-10-02 OWNER: RK68 HE tester build. 372E:10BF/10C0 admitted to the existing
> read-only IPI UUID path; catalog entries 0x110000000002/...3C from the official RK
> driver (configurator evidence, equal to 3 BY firmware ID tables); ANSI + ISO
> layouts; yellow notice; Sheet row RK68 HE inserted (282 yellow PASS). No physical
> test. See RK68HE_HUBX_2026-10-02.md.
> 2026-10-02 GitHub sidebar still lists `claude` under Contributors, but main, all 17
> tags and all 5 PR refs contain NO Claude/Anthropic co-author trailer (checked via API).
> Stale contributors cache from before the 09-27 history cleanup; only GitHub Support
> can clear it. Do not rewrite or force-push history for this.
> 2026-10-02 RK68 HE log (`372E:10BF`): Addressed backend sees it, never connects
> (IPI UUID path admits only 105C/106C). Official driver rk.hubx.pro = BY/IPI platform,
> UUID 0x110000000002 (UK 10C0 = ...3C). No RK68 HE image published; 24 sibling BY
> images downloaded (.local) all have HallJoy's 94/02 live read; RK68 HE ID set equals
> 3 firmware tables. Support not started. See RK68HE_HUBX_2026-10-02.md.
> 2026-10-02 OWNER: yellow support for every MCHOSE model we can. Done locally:
> 9 more RISC-V boards on the Jet 75 backend (Zero75X, Jet 75 I, Ace 68 I, Air II,
> Ace 60 Pro/Nordic, Ace 60X I/II, Mix 87 I) and 6 ARM boards on the Mix 87 backend
> (Ace 68 III/Air III/Air 2/V2 III/Turbo 8K, Ace 75 8K; emulator replay of 03/E0/06).
> Jet 75 II / Mix 87 III unchanged (Supported). Not done: Ace 60 ARM, GT, Turbo 16K.
> Sheet synchronized (Google Sheets connector): rows 526..542, 281 yellow PASS. build_release +
> diagnostics + native gates PASS, EXE 53e7d826. See MCHOSE_RISCV/ARM_FAMILY docs.
> 2026-10-02 tools/build.ps1 passes again: stale Hex80 text check and two unused
> locals fixed. See BUILD_PS1_FIXES_2026-10-02.md.
> 2026-10-02 OWNER: tester build for MCHOSE Ace 68 `41E4:2116` (USB "Ace68-II",
> M HUB "Ace 68 Pro"). Firmware 1.21 reviewed at base 0x5000: same commands,
> writer, reboot rule, A0 report and byte-7 bit-3 flag as Jet 75 II. The Jet 75
> backend now has a model table; automatic 68-key layout; yellow notice
> `MchoseAce68`. Live Sheet row "Ace 68" NOT changed (owner decision).
> No physical test. See MCHOSE_ACE68II_2026-10-02.md.
> 2026-10-02 Ace 68 attempt log (`previous (2)`): PID 2116, only matched=0,
> no command ever sent; the red Ace 68 row rests on attempts that never
> reached the keyboard. 2116 = Ace 68 Pro / USB "Ace68-II", sold as Ace 68.
> 2026-10-02 MCHOSE all-model static survey (33 M HUB images, no device).
> Every magnetic model with an image uses the Jet75/Mix87 analog design
> (A0 + profile byte 7 bit 3, 55/AA transport): 11 WCH RISC-V images like
> Jet 75 II, 7 ARM like Mix87 III (+ Ace 60 ARM variant), and a new WCH core
> for Ace 68 GT / Turbo 16K. Old RISC-V generation reloads 32 B (flag still
> applies). Sheet unchanged; red "Ace 68" capture was PID 2116 = Ace 68 Pro.
> No support work started. See MCHOSE_FAMILY_SURVEY_2026-10-02.md.
> 2026-10-02 Jet 75 II log8: analog WORKS on fw 1.17 (79 keys, 113 updates, 0
> failures). Fixed idle start: Jet75/Mix87 now report connected right after
> admission, not after the first key press (no false missing-analog banner).
> 2026-10-02 Jet 75 II log7: firmware 1.17 (version binding removal was needed);
> enable Verified, then a system device-change notification ended the session.
> Fixed in Jet75 AND Mix87: device changes no longer end a healthy session
> (unplug ends it via read error). Awaiting the next tester log.
> 2026-10-02 MCHOSE Jet 75 II (`41E4:211A`, stock 1.16) implemented with a yellow notice.
>
> - Same analog design as Mix87 III (A0 stream + saved flag, profile byte 7 bit 3),
>   same automatic enable/disable lifecycle; automatic 80-key layout.
> - OWNER (same day): no firmware-version binding. Any version is admitted;
>   safety comes from data checks (layout before write, readback, A0 checks).
> - New restriction: if the keyboard was just flashed or factory reset, a
>   settings write reboots it once; handled.
> - Owner allowed downloading all M HUB firmware/layouts locally (33 images).
> - Live Sheet B535:C535 -> Jet 75 (II revision) / Implemented; 272 yellow PASS.
> - No physical test yet. See MCHOSE_JET75_2026-10-02.md.
> 2026-10-01 OWNER: yellow support for every ATK keyboard on the Hex80 protocol.
>
> - Implemented 15 hub models (19 Sheet rows: EDGE 60/63/75 HE, 60 RX, RS63 Air,
>   RS6 family, 68 V3/RX, RS6 Air/Cube, RS7 / V2 / Air / Turbo, 68 V2 Pro).
>   They are read-only (no SET), with automatic layouts and the `AtkHex80Family`
>   notice.
> - The live Sheet is synchronized.
> - See ATK_HEX80_FAMILY_2026-10-01.md.
> 2026-10-01 Build closing of an elevated HallJoy is fixed.
>
> - HallJoy now accepts the exit request from an unelevated build.
> - Older elevated builds are closed through a single UAC prompt.
> - The picker shows "MAD68 HE V2 Flagship".
> - See BUILD_CLOSE_ELEVATED_HALLJOY_2026-10-01.md.
> 2026-10-01 OWNER RULE: no Supported status without a built-in layout. The
> check is enforced by `check_supported_layouts.py` in every build and with
> `--sheet`.
>
> - Added layouts: AK820 MAX HE, Mix 87 III, Field75 HE and Apex Pro.
> - Automatic selection works for three of them; Field75 is manual.
> - See SUPPORTED_LAYOUT_COVERAGE_2026-09-30.md.
> 2026-09-30 IO check (owner request: all models and all firmware; this
> supersedes the 09-21 catalog-only note).
>
> - The manifest is unchanged: Type 68 Magnetic V1.37, Type 84 Magnetic V1.17.
>   No Magnetic Pro firmware is published.
> - The published images handle only `0x64..0x67`: no new analog.
> - The configurator has a frameVersion-1 addressed multi-key getter `0x68`
>   (used during calibration V2), which is likely for the Magnetic Pro models.
> - The Sheet is missing Type 84 Magnetic Pro (configurator only).
> - See IO_CATALOG_FIRMWARE_2026-09-30.md.
> 2026-09-30 PUBLISHED stable/latest 1.6.6: https://github.com/PashOK7/HallJoy/releases/tag/v1.6.6 .
> - Source: a3ec1db306ac731a4ac4f567e596b17ed9dba080.
> - EXE SHA256 4c5887c5…e846; the downloaded assets match.
> - Contents: MAD68 HE V2 Flagship Supported, shortcuts, Pause/Resume fixes.
>   There is no K4 in the notes.
> - Sheet structure and 252 yellow rows PASS on a fresh full read.
> - All 52 prior assets are preserved.
> - See RELEASE_1.6.6_PUBLICATION_2026-09-30.md.
> 2026-09-30 AJAZZ name correction (owner): the supported keyboard is AK820 MAX HE wired RGB (SG8994HERGB), not a retail "AK820 MAX RGB" (that was the driver file name). The Sheet row was renamed "AK820 MAX HE (wired, RGB)" Supported. The broad "AK820 MAX HE" row became "AK820 MAX HE (tri-mode, screen)" Not investigated. A new "AK820 MAX HE (wired, no light)" row is Research incomplete. The runtime name, README and hardware doc were updated; published 1.6.2 notes were kept. See AJAZZ_AK820MAX_REVIEW_2026-09-20.md latest section.
> 2026-09-30 OWNER requested a tester build with full MADLIONS MAD68 HE V2 Flagship support.
>
> - Implemented: exact `373B:1125` as a Hex80-family model. It uses the GET-only
>   `02 96 1C` 5x15 travel buffer (firmware V103), a fixed 3.30 mm scale, no SET,
>   automatic MAD68HE ANSI layout and a yellow notice.
> - Sheet: live Main!509 row "MAD68 HE V2 Flagship" inserted as Implemented;
>   awaiting hardware testing. Row readback is verified.
> - RESOLVED later on 2026-09-30: the earlier claim that the connector cannot
>   export a full native snapshot was wrong. A full `get_spreadsheet` read of
>   Main!A1:C1279 (grid data, row metadata, formats, validation) is saved to a
>   file automatically when large. On it, `check_keyboard_sheet_structure.py`
>   passed (148 blocks, 0 issues) and `support_notice_catalog.py --sheet`
>   passed (252 yellow). Snapshot: `.local/release166-sheet.json`.
> - Also 2026-09-30: live Main!C290 Fantech ATOM HE68 (MK811) changed to No usable
>   analog found. MK922 stays Not investigated: it needs its own firmware.
> - See MAD68_HE_V2_FLAGSHIP_2026-09-30.md and FIRMWARE_TRIAGE_2026-09-30.md.
>   No publication.
> 2026-09-28 Alumix104 PHYSICAL log47: 301/301 complete 65-byte packet pairs, no capture/log loss, 320 letter downs/ups and peak five held. The 861 anonymous reports include 687 zero bodies; all 36 captures retaining an index match the latest 55FB selected index, and 102/145 retaining stroke match exactly (135/145 within ten). This is strong evidence of blank/partial/stale selected-report variants, not an independent per-key matrix. Multi-key freshness failed in 108/117 windows, including 28/28 without a letter transition. The sole support.banner_shown line after session.end was a mislabeled backend SupportLog_RequestSnapshot, not proof of UI banner failure; writer corrected and tested in full ordinary Release EXE SHA256 96BA5A1B. Live Main!C644 remains gray Research incomplete with validation/base/effective colors, adjacent row unchanged; all 252 yellow rows freshly match notices. No same-trace retest or support promotion. Exact104 remains gray Research incomplete; see ALUMIX104_YOTEI_LOG37_2026-09-27.md.
> 2026-09-28 Alumix104 log46 UI correction: support.banner_shown was missed in initial analysis. Exact backend was present=1 connected=0, correctly not admitted as analog, but generic UI showed No supported analogue keyboard detected. Raw-packet v2 EXE 90EA5CEF still had this. UI now marks exact104 research state separately, gives a Russian detected/research banner, and logs support.alumix104_research; it does not claim analog support or force a generic missing-source incident. Bounded research logging remains. Ordinary full-catalog build and support-state regression PASS; installed EXE SHA256 A9192413. Physical test pending; see ALUMIX104_YOTEI_LOG37_2026-09-27.md. Sheet stays gray Research incomplete.
> 2026-09-28 Alumix104 OWNER CORRECTION after physical log46: hiding unknown report bytes made the evidence insufficient for decoding. New exact-model v2 ordinary EXE records complete 65-byte FF68:61 unknown vendor frames on first/changed reports (up to 512 paired records), with class/ordinal, held-letter count/revision and last 55FB index/stroke/max/age; no standard keyboard report or text transcription. Raw payload may encode key state, so title requests test letters only and log header marks raw_hid_payload=1. Existing counts, cap/error summary, normal HallJoy.log/Open log, OFF cleanup, no 0x68/calibration/gamepad and full catalog retained. Fake-HID, writer, full Release/image checks PASS; installed EXE SHA256 90EA5CEF, physical run pending. Exact104 remains gray Research incomplete; no Sheet/release status change. See ALUMIX104_YOTEI_LOG37_2026-09-27.md latest section and SUPPORT_DIAGNOSTICS_CONTRACT.md.
> 2026-09-28 Alumix104 PHYSICAL log46 from NEW unknown-frame EXE: exact 0C45:80AC has seven HID collections. Only FF68:61 exposes 65-byte vendor input; FF67:61 has feature65/output4097 but no input stream. In ~21.9s one anonymous non55/AA class delivered 359 reports (305 zero body, 101 changes, 85 with unchanged Raw Input state, 128 during 2+ letters, 38 changed then; 12 body positions varied). This is an undecoded lead, NOT proven analog. Selected 55FB still lacked enough fresh sensors in 17/24 multi-letter windows. 21,379 valid samples, 58 letter downs/ups, no malformed/log drops, clean ON/OFF ACK, gamepad off. Do local frame-format/producer analysis before another physical trial; gray Research incomplete unchanged, no Sheet/release change. See ALUMIX104_YOTEI_LOG37_2026-09-27.md log46 section.
> 2026-09-28 Alumix104 PHYSICAL log45, older batch68 EXE: tester reports keyboard auto-calibration and stabilization disabled (the log cannot read these vendor settings). 0x68 remained exact echo OFF18/18 and ON1152/1152, zero depth candidates despite 51 Raw Input letter downs/ups, peak five held, 16,543 valid 55FB samples. In 13/16 multi-letter windows fewer positive sensors were fresh within50ms than held letters; 196 non-55 frames remain unclassified by this old EXE. Different key actions/duration prevent a causal settings claim. Newer unknown-frame/collection diagnostic is still awaiting physical run. Gray Research incomplete unchanged; no support/Sheet/release change. See ALUMIX104_YOTEI_LOG37_2026-09-27.md log45 section.
> 2026-09-28 Alumix104 NEXT DISTINCT DIAGNOSTIC READY LOCALLY after physical log44: ordinary EXE EFE0E9EF now inventories all seven exact-product HID collections and classifies recurrent non-0x55 frames anonymously by count, variation, same-digital-state changes and changed byte positions; no raw packets/text/key identities. It no longer queries proven-echo 0x68, does not enable calibration or drive gamepad, and retains 0x66/0x67 cleanup. Full Release/full-catalog, fake-HID and writer checks PASS. No physical run of this version or support promotion; fresh live Main!A643:C644 read confirms exact104 gray Research incomplete, strict dropdown/colors and adjacent Alumix68 unchanged. Local research found no documented clean independent reader in pinned official client/captured upper code; missing scanner/producer and other collection contents remain. See ALUMIX104_YOTEI_LOG37_2026-09-27.md latest section.
> 2026-09-28 Alumix104 PHYSICAL log44: exact 0x68 seven-address OFF scan returned 18/18 echoes over all121 positions; ON scan returned 1,981/1,981 echoes, zero depth candidates/timeouts/malformed. The keyboard was active (30 letter downs/ups, peak five held; 28,656 valid 55FB samples), so the calibration-off 0x68 route is physically unproductive, consistent with exact upper-dispatcher replay. Gamepad stayed disabled; clean 0x67 OFF ACK. This does not exclude other interfaces/firmware routes. Seek exact104 producer or independently documented reader, not another same scan or calibration-on shortcut. Live Main!C644 remains gray Research incomplete with strict validation and neighboring Alumix68 unchanged; no Sheet write/support promotion. See ALUMIX104_YOTEI_LOG37_2026-09-27.md log44 section.
> 2026-09-28 Alumix104 READ-ONLY batch68 probe READY LOCALLY: reused the addressed polling scheduler from IPI (capacity seven, bound-first initial sweep) for all 121 exact104 positions in separate 0x66-OFF and 0x66-ON phases; official AA:68 packet with final flag and strict echo/candidate parsing. The selected-key 55FB stream remains reference and cannot drive gamepad after log43. Echoes never count as depth; non-echo per-anonymous-slot positive/zero/change/release and multi-key freshness enter ordinary HallJoy.log. No calibration or digital analog calculation. Normal Release/exact-image full-catalog/diagnostics gates PASS; EXE SHA256 8448BD534B64697072B803941B791BF7F9372545F394B756E410DBDEA6544F4A. Physical 0x68 response and typing coexistence untested; no support promotion/publication. Sheet Main!C644 remains gray Research incomplete from prior verified readback. See ALUMIX104_YOTEI_LOG37_2026-09-27.md newest section.
> 2026-09-28 Alumix104 PHYSICAL log43: exact 55FB stream omits individual held sensors for long periods despite 16 ms maximum global report gap. In a two-key release episode, one sensor sent zeros, the other stayed unreported with its last positive value; HallJoy kept the latter active until the 1000 ms retention expired. Tester confirms 0.5–1 s lag and multikey jumps. With two digitally held letters, 90/101 trace windows had only one sensor fresh within 50 ms; peak six held, never more than two fresh. This establishes the root problem in the tested 55FB path, not a digital-key workaround or defective logging. No shorter timeout can distinguish omitted held from omitted released keys. Seek independent per-key source; do not send another same-stream/timeout build. Live Main!C644 remains gray Research incomplete, strict validation/colors verified; all252 yellow rows freshly reconciled with notices. See ALUMIX104_YOTEI_LOG37_2026-09-27.md log43 section.
> 2026-09-28 Alumix104 owner direction: diagnose the root cause before changing the analog algorithm or claiming support. Neighboring RSQ-20058 calibration flag diverts normal processing in scoped firmware replay; typing coexistence on exact104 is unproven, so calibration commands are excluded. A selected-key stream may omit another held sensor, so missing per-key reports cannot mean release. Local exact104 trace v2 instruments per-sensor timing, repeated values, explicit zeros, consumer reads, bindings, candidate gamepad state and publication; Raw Input is reference only. Physical trace still needed. Sheet Main!C644 remains gray Research incomplete; no support promotion or publication. See ALUMIX104_YOTEI_LOG37_2026-09-27.md and SUPPORT_DIAGNOSTICS_CONTRACT.md.
> 2026-09-28 Alumix104 logs41/42 and tester multi-key feedback SUPERSEDE the trial below. Exact104 0x66/0x55FB sends analog samples and can drive gamepad, but released keys linger 0.5–1.5s, the second held key can arrive late, and six simultaneous keys turn on/off inconsistently. Log42 has 99441 valid samples in ~101s, max packet gap16ms, 114 gamepad publications, Raw Input letter down/up123/123 with peak held5; aggregation lacks per-key timing, so exact cause remains unknown. Owner rejects digital key-up as an analog calculation, including for Rapid Trigger; proposed mask was removed. Trial source/catalog retained for continued investigation; the interim safe local EXE without this path is not a tester handoff or abandonment. No Alumix104 support claim or repeat same-source tester EXE. Seek genuine independent per-key analog source. Exact104 live Sheet Main!C644 remains gray Research incomplete, strict dropdown/colors/neighbors and252 yellow notice reconciliation previously verified; no status write needed. No publication. See ALUMIX104_YOTEI_LOG37_2026-09-27.md newest section.
> 2026-09-28 Alumix104 NEW native gamepad trial READY LOCALLY: ordinary HallJoy.exe SHA256 C4960050A37E44131F4206E0434C72A8D979D3B902BD6905D225650C162AC9F0 admits only exact104, owns official66/67->55FB without the prior research reader, maps103 positions to measured gamepad depths, releases on Pause/exit and logs source/consumer/output/typing/range/cleanup evidence. No test-completion timer or Pause requirement. Normal Release diagnostics, fake HID session, exact-image full-catalog and seven other EXE gates PASS; writer mirror timestamp race fixed. Live Main!C644 remains gray Research incomplete with dropdown/colors/neighbors confirmed; fresh252 yellow notice sync PASS. Await this new physical gamepad trial and HallJoy.log before disabling temporary diagnostics or promoting support. No publication. See ALUMIX104_YOTEI_LOG37_2026-09-27.md newest section.
> 2026-09-28 OWNER review of Alumix104 tester loop: log40 is valid new support evidence (paired mapped analog sensors plus target Raw Input letters); only the v4 completion condition incorrectly demanded a second press. The locally built v5 fixes the hint/gate and separates future range categories, but is not a new support experiment and must not be sent for a repeat run solely to confirm `всё готово`. The next meaningful tester EXE must implement actual exact104 measured analog-to-gamepad input through official66/67->55FB and pinned104 map, with exclusive collection ownership, release/cleanup and ordinary diagnostic log. Full-range accuracy remains unknown because2999 v4 anomaly samples combined zero maximum and above-maximum cases. Current live Sheet status stays gray Research incomplete; no support promotion/publication. See ALUMIX104_YOTEI_LOG37_2026-09-27.md latest decision.

> 2026-09-28 Alumix104 log40 PHYSICAL v4: exact104 13530 valid55FB samples, two mapped positive sensors during one two-letter Raw Input hold, target alphabetic downs/ups, OFF ACK; no read/log loss. Tester correctly found v4 extra-press bug: release of the already-held pair did not finish because code required a new post-prompt down/up. v5 local EXE 7B6DF7FE completes on both original letters released plus their analog zero transitions, and splits2999 v4 ambiguous range anomalies into safe coarse categories. Full ordinary build, writer, v5 self-test and full-catalog gate PASS; no v5 physical run/backend/support promotion/publication. Exact104 live Sheet Main!C644 remains gray Research incomplete, validated/colors/neighbors read back; all252 yellow rows fresh synced. See ALUMIX104_YOTEI_LOG37_2026-09-27.md latest section.

> 2026-09-28 Alumix104 v4 is a distinct follow-up to log39: v3 only fixed scale and counted letters, while v4 requires two mapped positive sensor samples during one overlapping physical two-letter Raw Input hold; same-letter down/up after that ends automatically. Incomplete runs still log all counters/missing evidence and OFF cleanup on close. Exact26-letter map checked against pinned vendor-derived JSON. Ordinary full-catalog build, v4 synthetic self-test, writer regression and exact-image gate PASS; installed EXE 462853E3. Final title refinement avoids replacing active-stage prompts with a generic silence hint. No physical v4 test, no support promotion/Sheet change/publication. See latest ALUMIX104_YOTEI_LOG37_2026-09-27.md section.

> 2026-09-28 Alumix104 log39 PHYSICAL stream established: exact0C45:80AC,440648 55FB samples/445.406s,70 indices, ON/OFF ACK, no read/log loss. Old EXE 3A9A8967 MIS-SCALED stroke versus maxStroke by10 (official UI divides by100 versus10), so its `ready`/84095 anomalies are not full-range proof; digital letters were not measured. Owner rejects Notepad/Pause handoff. New ordinary EXE 1FA53702 corrects units, counts only target Raw Input physical letter down/up, automatically sends OFF/logs/shows done when evidence arrives; long-log tail retention fixed. Full catalog/gates PASS, no new hardware retest yet. Live Sheet Main!C644 exact104 Not investigated -> Research incomplete, dropdown/gray colors/neighbors verified; Alumix68 unchanged. No analog backend/Supported/publication. See ALUMIX104_YOTEI_LOG37_2026-09-27.md latest section.

> 2026-09-28 OWNER removed arbitrary Alumix104 capture/test durations. Current ordinary EXE 3A9A8967 is readiness-driven: Russian title prompts single-key depth/release, two distinct indices and manual typing check; Pause explicitly ends even a stalled trial. Standard HallJoy.log records stage, sparse idle/sample and calibration/ADC checkpoints, faults, missing evidence and OFF cleanup, preserved by Open log. Full catalog, diagnostics/writer regressions and eight EXE gates PASS. No physical104 trial/support/Sheet/publication change. This supersedes the 25/35-second instructions below; see ALUMIX104_YOTEI_LOG37_2026-09-27.md newest section.

> 2026-09-28 Alumix104 restrictions RE-REVIEWED: exact16 flash route excludes only neighbor68 RAM trick; faults at missing code are unknown, not negative device results. Expanded exact dispatcher1120 candidate payloads60/68/69/6A echo; official66/67->55FB remains best hardware path and no full firmware prerequisite. Ordinary one-EXE bounded stream diagnostic built, full catalog/gates PASS, EXEf339e040; aggregate5s windows and OFF cleanup in HallJoy.log. No hardware trial or support/Sheet change. See ALUMIX104_YOTEI_LOG37_2026-09-27.md newest section.

> 2026-09-28 OWNER: continue Alumix104 research using existing capture and experiments; do not wait for a full firmware as a prerequisite. New strict stateful replay140 mode commands +280 query payload cases PASS:66/67 reversible dispatcher RAM flag,68 remains echo with simulation on/off. Official55FB stream is next candidate; typing/multikey/cleanup outside dispatcher still unknown. No device experiment/new EXE/status change yet. See ALUMIX104_YOTEI_LOG37_2026-09-27.md newest section.

> 2026-09-28 Exact Alumix104 replay rejects neighbor68 RAM path: command16 reads FLASH B000, not depth RAM. Established unsigned16 reads cannot reach lower code0000..8FFF; full dump NOT established.27 copy cases+951blocks verified;512 command/state outcomes classified incl278 missing-code/data faults, not full suitability. Simulation typing/multi-key unknown; no trial activation. Fresh official updater lacks104 image;433BIN+34HEX corpus scan no match. Need exact stock104 firmware or documented interface; no new app build/status. See ALUMIX104_YOTEI_LOG37_2026-09-27.md newest detailed restrictions (including cmd10 write side effect).

> 2026-09-28 HallJoy(38).log is ALUMIX104 (not MAD68): exact model/65-byte report,951 blocks/53248 bytes captured in9.531s; strict decoder PASS, private log38-code-window.bin hash0e3daac3. NOT identical68 firmware,104 address/protocol analysis pending; no analog support claim. MAD68 message(5) only proves decoded pairs, owner reports no analog; remain unresolved. See both current model review documents.

> 2026-09-27 OWNER requires COMPLETE ordinary EXE. FIXED MAD68 omission: limited stock backend and red/no-forced-log classification now unconditional in normal build, alongside Alumix104 shared-log research and Pause/Resume keys. Mandatory independent21-protocol exact-image gate added. FULL static/portable/Windows regression + Release/eight image checks PASS; installed EXEec88ecd8. Live MAD68 C503 red/validation confirmed; all252yellow reconciled. No support promotion/physical retest/publication. See MAD68_V2_DUAL_REVIEW_2026-09-27.md newest record. Never reintroduce trial-only delivery split.

> 2026-09-27 Alumix104 corrected NORMAL EXE workflow BUILT: bounded exact-device background read12, shared HallJoy.log/Open log, no export CLI/ZIP/scripts; Pause/Exit cancels/joins. Retains reviewed capture across log snapshots; no forced continuous logging. Windows writer/decoder/cancellation and standard Release seven gates PASS; installed EXE8930d6bc. Actual104 log still needed, no analog/status/Sheet/publication claim. See ALUMIX104_YOTEI_LOG37_2026-09-27.md newest sections; supersedes pending/rejected handoff below.

> 2026-09-27 OWNER PERMANENT RULE: tester gets ONE HallJoy.exe and returns normal HallJoy.log via Open log. No research ZIP, scripts, CLI flags or separate TXT/BIN exports. Use standard bounded background diagnostics/log writer, retaining privacy, performance and existing logging policy. Local agent tools allowed. Alumix104 ZIP/CMD handoff REJECTED; normal-workflow replacement pending, current EXE9b1e6941 not yet corrected. See AGENTS.md and SUPPORT_DIAGNOSTICS_CONTRACT.md.

> 2026-09-27 Alumix104 read-only code probe BUILT locally, normal Release EXE9b1e6941; private ZIP .local/alumix104-review/HallJoy-Alumix104-research.zip. Explicit CMD only, exact identity/descriptor, read12 bounded code window, no modes/flash. Standard gates + probe self-test/decoder/ZIP PASS. Await actual104 tester export to establish addresses; no native support/Sheet/publication changes. See ALUMIX104_YOTEI_LOG37_2026-09-27.md.

> 2026-09-27 Alumix68 broader replay FOUND better path: command16 offset0200 reads128-slot depth RAM with modes off;1536 dispatch scenarios and1024 dynamic values PASS, USB scheduler checked with stub.0x12 reads scan map/code window;0x10 mutates one returned byte. Supersedes selected-key-only best-path conclusion.104 addresses NOT proven; integration pending. See ALUMIX104_YOTEI_LOG37_2026-09-27.md. No support/publication change.

> 2026-09-27 OWNER authorized neighboring Alumix68 firmware as evidence for104 investigation, not flashing. Exact68 Thumb replay544 cases PASS reveals simulation66/67 is SELECTED-KEY ONLY; calibration-off normal gate reachable, not full typing-HID proof. Do not infer unrestricted104 behavior or promote yellow from neighboring firmware. See ALUMIX104_YOTEI_LOG37_2026-09-27.md. No runtime/support/publication changes.

> 2026-09-27 Pause/Resume shortcuts implemented locally: toggle or separate physical keys, analog + pre-block digital detection while active; FULL provider/keyboard release on Pause and ordinary input for Resume. Owner rejects retaining analog on Pause; earlier hypothetical permanently-analog keyboard was NOT an established device limitation. Global settings/capture/atomic profile-independent persistence and model/Windows/private-desktop UI tests PASS; ordinary Release diagnostic/six executable/embedded-resource gates PASS; installed EXE9c1577f4. See PAUSE_HOTKEYS_2026-09-27.md. No publication/support-status change.

> 2026-09-27 OWNER requests Pause/Resume hotkeys independent of Block Bound Keys, accepting analog-only presses, with toggle vs separate Pause/Resume bindings. Owner explicitly REJECTS retaining analog providers on Pause: fully release the keyboard. Existing pause lifecycle must remain. Clarification pending: analog+digital recognition while Active, ordinary keyboard events while Paused; truly analog-only devices with no reports after release require tray/window Resume. No hotkey implementation yet; do not silently retain Mix87 flag/MAD68 typing-blocking mode during Pause.

> 2026-09-27 OWNER: keep README keyboard names concise and consistent; do not append firmware versions, protocol details or analog-quality prose to model names. Removed newly added Mix87/SteelSeries annotations; Mix 87 III retains its actual revision in the name. Detailed compatibility remains in docs/SUPPORTED_HARDWARE.md and research records. Naming-only correction, no support-status change.

> 2026-09-27 PUBLISHED stable/latest1.6.5: https://github.com/PashOK7/HallJoy/releases/tag/v1.6.5 . Source3bf755042dd021d5ba0eae06caacc3e43b3f6d0b; downloaded EXEe414302e matches package. Mix87 III green/live Sheet and252yellow synchronized; tray/UI/editor/diagnostics/provenance included, Profiles hidden, no K4 public notes. Local Release6 gates/diagnostic/static/portable checks PASS (two test-runner packaging fixes). All48 previous release assets and download counts preserved. Hosted Windows SKIPPED. See RELEASE_1.6.5_PUBLICATION_2026-09-27.md.

> 2026-09-27 OWNER supersedes Mix87 manual/red policy: automatic flag ON at start/resume and OFF at pause/exit; green Supported requested after tester confirmation. Implemented guarded mode lease and no flash-retry loops; persistent-write/power-loss/profile-race limits documented. README/runtime/live SheetC532 green synchronized, validation/colors/borders/neighbors read back;252yellow fresh sync PASS. Lifecycle/deadline tests, full static suite and ordinary Release gates PASS; EXE installed. An enabled flag after power loss has no demonstrated negative effect; do not imply otherwise. See MCHOSE_MIX87_LOG35_2026-09-27.md. No publication.

> 2026-09-27 LOCAL diagnostics overhaul delivered: shared manifest capacity and collector, complete schema2 snapshots independent of UI, lifecycle availability, typed event details (metadata never errors), legacy-aware analyzer and mandatory local Release contract gate. Preserve no forced logs for limited/unstable banners. All static, diagnostic collector/writer/session gates and Release six gates/embedded legal checks PASS; installed EXE89e201d9. See SUPPORT_DIAGNOSTICS_2026-09-27.md and docs/development/SUPPORT_DIAGNOSTICS_CONTRACT.md. No keyboard status or publication change.

> 2026-09-27 Mix87 log36: owner relays tester says everything works. Limited/red status remains; README/hardware evidence updated and live SheetC532 red/validation verified unchanged. Found telemetry registry truncation at16; capacity now derived from compiled catalog; architecture regression and ordinary Release six gates/embedded legal checks PASS, installed. See MCHOSE_MIX87_LOG35_2026-09-27.md. No publication.

> 2026-09-27 LOCAL Mix87 III stock1.22: one native-style Analog mode checkbox with saved-setting warning; no technical dialogs. Exact firmware fingerprint/86-key native path, explicit one-bit persistent opt-in only, NO calibration or automatic writes. Red limited analog/no forced log; shallow/event limits remain, no hardware test. Firmware replay3915+512+8, host transactions2048+faults, seven fake-HID scenarios, static +ordinary Release6 gates PASS. README/hardware/SheetB532:C532 red synchronized;252yellow PASS. See MCHOSE_MIX87_LOG35_2026-09-27.md. No publication.

> 2026-09-27 Mix87 III: owner rejects calibration A8/A9; explicitly approves separate enable/disable button with warning for persistent M HUB debug flag. No automatic flash writes. Exact v1.22 offline replay3915 cases +512 opcode routes PASS; shallow cutoff/event serialization remain limitations. Integration/tests/Sheet sync in progress; no supported claim/build yet. See MCHOSE_MIX87_LOG35_2026-09-27.md.

> 2026-09-27 MCHOSE Mix87 log35: exact3837:300D matches official catalog Mix87 III; HID present, UAP ready/error0 but devices0, no production analog route. Exact firmware URL found; not analyzed yet. No user/configurator fault established, no support/build/publication change. See MCHOSE_MIX87_LOG35_2026-09-27.md.

> 2026-09-27 LOCAL dependency provenance: production ABI1 only; retired common/import archives retained privately and denied for next publication. Exact new ABI1 map has seven reviewed Soup objects; runtime record and embedded-byte gates added. ViGEmClient rebuilt from b66d02d; Wooting header matches v0.9.1 be67cbf, component licenses/provenance recorded. Physical ABI + Release six gates + ZIP PASS, EXEe839d000. No publication/support change. General layout checker still flags pre-existing private .analysis backup folder. See docs/legal/DEPENDENCY_PROVENANCE_2026-09-27.md.

> 2026-09-27 PUBLISHED corpus/history cleanup at 4af3fcf4f3f49752500926cf670ea46a012102f5:553 research paths excluded from139 historical trees; main+11 tags updated. All15 releases/48 assets/15 EXEs preserved,520 downloads before/after, no asset counter decrease. Full private research audits, public static checks and isolated publication Release+six gates+legal resources PASS. Do not force-push old mirrors/tags. Unreleased local app changes were NOT published. Other licensing provenance questions remain open; see docs/legal/HISTORY_CLEANUP_2026-09-27.md.

> 2026-09-27 LOCAL distribution remediation: eight firmware headers dual-licensed with owner consent; Keychron attribution restored; licenses embedded/readable, ordinary Release six gates+resource-byte check and ZIP round-trip PASS, elevated publisher restored app (B87FF03B...). Verified backup553 unclear research paths and full Git mirror; review checkout excludes them. NOT publish-ready: private vendor evidence dependencies still break public static runner. History rewrite permission pending. See distribution audit; do not claim full legal clearance or publish candidate yet.

> 2026-09-27 OWNER explicitly authorized a GPL-2.0-or-later alternative for eight shared firmware headers only; no whole-application relicensing. Added scoped grant/GPL text and restored Keychron attribution in descriptor source/patch. Broader distribution remediation remains in progress; no publication.

> 2026-09-27 OWNER requested full repository distribution/royalty review. Inventoried2645 published files;492 vendor payload/web/art/manual files have redistribution permission unestablished (not proof of infringement),415 research files need provenance review. Main MIT/BSD/MPL licenses show no mandatory per-copy royalty, but exact Wooting source/transitive notices, Soup closure, packaging LICENSE and separate Keychron GPL attribution remain open. Owner confirms HallJoy logos/icons generated in Google Nano Banana. Local notices source-link/overlay-count and commercial-license carve-outs corrected; no publication/deletion/runtime change. See docs/legal/DISTRIBUTION_AUDIT_2026-09-27.md before distribution claims or corpus publication.

> 2026-09-27 LOCAL firmware toolkit: reusable USB submit/readiness fault model and MAD68 same-RAM mode lifecycle suite;21 real-firmware scenarios and93 tool tests PASS. Scoped busy-loop and missing release-fragment retry hazards surfaced automatically. Ordinary HID delivery/global USB recovery still unknown. Evidence mad68-usb-lifecycle-verified-20260927; see firmware behavior runbook. No runtime/support/publication changes.

> 2026-09-27 OWNER: pursue progressively broader reusable firmware emulation; move repetitive analysis into tools and reduce agent context cost without weakening evidence. Shared strict Thumb core, waveform scenarios and hash-verified compact JSON-pointer inspector added;89 tests PASS, fresh3-image replay15552 samples+2250 waveform passes; see docs/development/FIRMWARE_BEHAVIOR_REVIEW.md. No app build/device access/publication.

> 2026-09-27 OWNER: improve firmware/emulator pipeline; disclose all restrictions before proposing integrations. If no unrestricted path is established, select the best reviewed limited analog path, potentially with a red banner. Stop compiling in the MAD68 diagnostic directory. New behavior matrix/candidate comparison and all-index MAD68 replay documented in docs/development/FIRMWARE_BEHAVIOR_REVIEW.md; no runtime/support change.

> 2026-09-27 OWNER: retain MAD68 V2 Dual limited implementation despite confirmed typing loss; tester finds analog usable. Red firmware-limit banner, NO forced log due to this banner (shared UI/writer policy); manual logging retained. SheetC503 red Analog available; low quality, strict validation/base/effective colors read back;252 yellow models synchronized. Trial rebuilt+6 gates and Windows writer regression PASS; SHA25698a60fcc3dc1050966e64b3bc360b01a2a7eb651caebca7df9583c4935f7db1d. No Supported promotion, normal Release admission or publication. See MAD68_V2_DUAL_REVIEW_2026-09-27.md. Owner clarified error5 was another HallJoy in tray; no guard fix needed for this task.

> 2026-09-27 OWNER: MAD68 V2 Dual gets a PRIVATE limited stock-analog trial first; if unusable owner will ask tester about flashing. No flashing now and no Supported/yellow promotion. Exact28E9:3265 compile-time trial, real bindings/ViGEm, red limitations/Open log, mandatory36:00 cleanup. Parser+fake-HID session+ARM replay+Release6 gates PASS. EXE build/bin/Mad68DualTrial/Release/x64/HallJoy-MAD68-V2-Dual-Trial.exe SHA25664d0b87d7397bd71d24a25789431b83867c9a5e7c362a1db3b90dd860ef3baaf. Live Sheet missing model added gray Research incomplete atC503; native structure/colors and252 yellow sync PASS. Ordinary app untouched. See MAD68_V2_DUAL_REVIEW_2026-09-27.md newest section. No publication.

> 2026-09-27 OWNER: MAD68 V2 Dual support ONLY without reflashing. Deeper offline stock review: official history has1.06/1.07/1.09; expanded ARM audit PASS for768 opcode routes, simulation limitations on all three, binary factory82 and12 HID GET_REPORT cases. No clean gameplay analog transport found; no blind integration/status promotion. See MAD68_V2_DUAL_REVIEW_2026-09-27.md newest section. No hardware commands, build, support sync or publication.

> 2026-09-27 MAD68 V2 Dual firmware acquired: exact28E9:3265 confirmed by FGG V2 catalog and both1.07/1.09 descriptors. Offline hash-pinned ARM replay PASS: command36 diverts ordinary key processing, clamps small travel and delays stable small changes31 additional per-key passes. No clean gameplay source established; no blind PID addition/status promotion. Tool mad68_v2_dual_firmware_audit.py; evidence and provenance (including API MD5 mismatch) in MAD68_V2_DUAL_REVIEW_2026-09-27.md. No flash/runtime change/publication.

> 2026-09-27 OWNER: Open log in red/yellow banners; communication instability alone must NOT force logging. Implemented async fresh snapshot/open in Notepad; automatic missing/experimental-support log, manual/continuous paths preserved. Writer regression + Release6 gates PASS. Installed hash-verified candidate and restored app via elevated standard publisher. Commands remained medium-integrity after owner app restart; RunAs publisher succeeded. See BUILD_LIFECYCLE_2026-09-27.md. MAD message(4) log exists (hidden extensions confusion), no373B;28E9:3265 candidate needs exact identification. See MAD68_V2_DUAL_REVIEW_2026-09-27.md. No support change/publication.

> 2026-09-27 OWNER: temporarily hide Profiles. Production page and automatic foreground service creation now disabled; code/data retained. Ordinary Release+6 gates PASS; previously running app restored. EXE SHA25697af684f80837d5effd975980bc9bce37b8fa9e9526bc633a63874cb5bd1d2d7. MAD68 V2 Dual user has red unsupported banner and cannot find log even with logging enabled. Official V2 identities absent from current recognition; DuckBread compatibility lead, exact identity/log pending. See MAD68_V2_DUAL_REVIEW_2026-09-27.md. No support-status change or publication.

> 2026-09-27 LOCAL game profiles delivered: separate Profiles tab, browse vs activate, exact EXE associations, event-driven automatic switching, session manual override, create/duplicate/rename/delete with recovery copies, autosave and activation-checkpoint undo. Existing profile storage preserved. Hidden UI rebuilds deferred; current route preflight and runtime history reset. Production profile/private-desktop tests +18 recovery cases, static audits and ordinary Release+6 gates PASS. EXE SHA256 690114cab1d26ed20a5ef1392c2f22f9e485023e17ad1e5404a0e0720c6ed63a. See GAME_PROFILES_2026-09-27.md. No publication; visual/gameplay evaluation remains with owner.

> 2026-09-27 LOCAL editor rendering: owner rejected 13–16ms as sufficient. Profile identified grid11.2ms; world-space coverage raster, cached rows and direct pixel transfer reduce final 82-key offscreen frame to3.921ms (grid0.476/keys3.000/rulers0.445), from54.111ms initially. No frame cap; direct mouse repaint. Grid/gesture/profile+18 recovery checks and ordinary Release+6 gates PASS. SHA256 e772678cf4755f5f691e55165704d9bc0ec2c3e18fd06af061cd6295e72e436f. See LAYOUT_EDITOR_RENDER_2026-09-27.md. No publication or measured displayed-FPS claim.

> 2026-09-27 LOCAL UI: collapse hidden override row in global Configuration; remove disabled camera-test toolbar gap in Tester. Layout editor uses right-drag to pan, short right-click still deletes guides. Geometry/gesture/profile tests and Release+6 gates PASS; app restored. EXE SHA256 c9e8794319a7b6554465ac59c716fd7b8f8d048f50ec5b2f8f04869a3aeefca6. See UI_SPACING_AND_RMB_PAN_2026-09-27.md. No publication.

> 2026-09-27 tray pause indicator: amber pause badge and Paused tooltip follow confirmed engine state; original icon returns on Active. Event-driven, cached native icon, Explorer recovery preserved. Private-desktop icon/state tests, profiles+18 recovery cases, owner static audit and ordinary Release+6 gates PASS; app automatically restored. EXE SHA256 6b7cc81a57c579b8b67c1ae00e89554cc1160e7fb1ea7394539fa647b7a08dea. See TRAY_WINDOW_BEHAVIOR_2026-09-27.md. No publication.

> 2026-09-27 tray follow-up: removed hint and spacing; added state-aware Pause/Resume and Input Overlay Start/Stop. Stop overrides watchdog startup intent; overlay controls invalidated after external actions. Menu/profile/recovery tests and relevant static checks PASS; Release+6 gates PASS, app restored. SHA256 5aa866eea2e5bada3bf8ea99566ecefb2eda7ab3e1fb06f5eafc00c7a357f818. No publication. See TRAY_WINDOW_BEHAVIOR_2026-09-27.md.

> 2026-09-27 LOCAL tray implemented: independent minimize/close-to-tray preferences in Global Settings, default off, application-wide and profile-independent. Tray Open/Exit, second-launch restore, Explorer recovery, explicit build exit and hidden-UI suspension. Release+6 gates, static audits, private-desktop lifecycle and profile/recovery tests PASS. Previously running HallJoy automatically restored. See TRAY_WINDOW_BEHAVIOR_2026-09-27.md. No publication; game profiles not implemented.

> 2026-09-27 OWNER: when HallJoy was running before a build, restore it automatically afterward; normal/minimized as appropriate, never hidden. No new permission required. This supersedes earlier leave-closed guidance. Fixed build identity detection using limited-information Win32 image queries: Process.Path was blank on the actual running app. Unknown identity now blocks replacement instead of silently treating it as closed. See BUILD_LIFECYCLE_2026-09-27.md.

> 2026-09-27 OWNER: remove Polling rate/UI refresh controls and ignore all old values. Local implementation fixes scheduling to prior fastest defaults, suspends hidden/minimized visual updates and retains engine maintenance; tray/game profiles explicitly next task. See FIXED_TIMING_AND_HIDDEN_UI_2026-09-27.md. Full static audits, production profile/obsolete-settings/private-desktop visibility tests,18 recovery scenarios and ordinary Release+6 gates PASS. EXE SHA256bd42a5eba5dfa9f9561f1cede57f69cd61064928ade57b48a271c62050d47b52. HallJoy left closed; no hidden relaunch or publication.

> 2026-09-26 startup incident resolved: agent had restored the interactive HallJoy using Start-Process -WindowStyle Hidden at21:59:36 (PID8060). EnumWindows confirmed hidden WootingVigemGui/HallJoy; its children remained active and correctly held the instance guard. Normal close was attempted, then this verified process tree was terminated and1.6.4 launched visibly at owner request (PID14760). Do not restore the interactive app with Hidden: reserve hidden launches for headless self-tests/helpers. If a visible interactive relaunch is not authorized, leave HallJoy closed and say so. This was an agent launch error, not evidence of a release deadlock; no source or public release modification.

> 2026-09-26 PUBLISHED stable/latest1.6.4: https://github.com/PashOK7/HallJoy/releases/tag/v1.6.4 . Source3af768e949f6aef4fd8ddad58694fe3368f74da3; downloaded EXE SHA2565948c5286b684437c247aae0cb6db483c75aa0c65df4497f7272ea729b36bbd5 matches local. SteelSeries/EPOMAKER, detection/conflict fixes and accumulated local work included; K4 intentionally absent from public patch notes. Sheet252 yellow/65 supported synchronized, ordinary Release+6 gates and clean-source static PASS. Hosted Windows SKIPPED. See RELEASE_1.6.4_PUBLICATION_2026-09-26.md.

> 2026-09-26 release1.6.4 owner clarification: omit K4 custom onboard changes from the public patch notes; only the owner uses this firmware so far. Keep implementation and internal technical documentation.

> 2026-09-26 LOCAL: owner confirms EPOMAKER HE108 analog after closing vendor driver; Supported in README/runtime/live Sheet C257. Log34 itself records pre-recovery sharing error32, not successful analog. Startup contention now reports common communication warning; ordinary stream diagnostics added. Seven exact EPOMAKER layouts added, HE68 Mag held for factory/UI mismatch. Sheet252 yellow/65 supported reconciled. Full static+health/source regression, production profile/layout+18 recovery scenarios and ordinary Release+6 gates PASS. EXE SHA25604bbb74b59c63302ca417806cdc851aff5dc610d5bdff12cadf0bc2e5247d395. See EPOMAKER_HE108_2026-09-26.md. No publication.

> 2026-09-26 K4 r6 installed: precise sparse telemetry A9/7E, measured162.667 fresh snapshots/s vs48.1463 legacy on same firmware; high-resolution UI hint clock. Old paths preserved, no smoothing. Full flash readback+neutral lifecycle/watchdog+portable protocol/static checks+ordinary Release6 gates PASS. EXE b9e3ee02b4f6dbf3f4a786a9160533037ba8c2e1696d7cd0965ed9773557977c. Visual FPS not measured; owner evaluates. See K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md. HallJoy closed, no publication. Existing permission to close HallJoy remains; blank Process.Path alone is not reason to ask owner to close it. Verify window/name/CIM process tree, attempt normal then permitted forced close.

> 2026-09-25 K4 preview: added coalesced event-driven invalidation after changed onboard UI values; timer fallback preserved, no interpolation or firmware/gamepad changes. Removes timer waiting, not the ~48.6Hz historical telemetry bottleneck. UI static guard and ordinary Release+6 gates PASS; visual result awaits owner. EXE SHA256 e95009d55bc2eb3c06791afa1eb4358a5579c103ab0626883b914eb50623098d. See K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md newest section. No publication.

> 2026-09-25 LOCAL SteelSeries expansion delivered: original TKL1614 + full-size Gen3 1640 enabled on firmware4.16.8, yellow; original1610 stays Supported. Official ARM handlers/descriptors, targeted protocol+notice tests, static audits, production profile/layout+18 recovery scenarios and ordinary Release+6 gates PASS. Live Sheet/README/runtime synchronized253 yellow; seven newer models gray Research incomplete. Modern wired dispatch lacks legacy ADC/calibration commands; wireless path unresolved, not impossible. EXE SHA256605a0b8b8f980335dcff16de4076905ec50887054b2ef3729ca788493816aa1e. See STEELSERIES_FAMILY_REVIEW_2026-09-25.md latest section. No publication.

> 2026-09-25 OWNER CORRECTION: tester explicitly said working now before message (3). Original full-size Apex Pro is Supported; no new tester run needed. Runtime/README/hardware/live Sheet C695 synchronized,251 yellow; ordinary Release+6 gates PASS. Neighbor review found actual ARM read-handler compatibility in original TKL1614 and full-size Gen3 1640, but neither enabled yet; Mini/new TKL/wireless remain separate research. See STEELSERIES_FAMILY_REVIEW_2026-09-25.md. No publication.

> 2026-09-25 Apex message (3): no repeat reset storm in 12.641s; last early snapshot has no analog, cause unavailable. Fixed our missing ordinary Release diagnostics (DebugLog calls were compiled out): structural apex.* stages now use SupportLog, without per-frame input logging. Ordinary build+6 gates PASS; EXE SHA256 a1abfbe55647662e58abadf68f3ffeb93e56ef23aaaa3e1015e2bb85f34b4414. Fresh tester log needed; experimental status unchanged, no publication. See STEELSERIES_APEX_PRO_IMPLEMENTATION_2026-09-25.md.

> 2026-09-25 LOCAL Apex Pro1038:1610: native read-only analog for official FW4.16.8, yellow status; no physical test. Owner confirms reset loop starts with HallJoy. Found legacy SparkLink broad FFxx probe sends01 02, which executes Apex SYSRESETREQ (reproduced in ARM emulator). Removed generic probing: exact SparkLink IDs+FFB0:1 only; UAP skips unsupported USB vendors and ignores unchanged source topology. Rebuilt embedded UAP; private ABI, full native suite, ordinary Release+6 gates, production-linked layouts/profiles and18 recovery scenarios PASS. Final EXE ce57a133fe5aa2d4e9239d04ac3733be04b847472d4411cef0e1b203951fd955. README/hardware/live Sheet synchronized252yellow,647models. See STEELSERIES_APEX_PRO_IMPLEMENTATION_2026-09-25.md. No publication; tester retry remains necessary.

> 2026-09-25 Apex Pro user report reviewed: current HallJoy/UAP has no SteelSeries analog route; this is not a missing-PID-only case. USB1038:1610 alternates present/absent;11 UAP exits0xE0484456 are deliberate device-refresh restarts, not crashes. Cause of USB churn unknown. See STEELSERIES_APEX_PRO_LOG_2026-09-25.md. No support-status change or publication.

> 2026-09-25 GitHub Topics updated and read back (20-topic limit): added AJAZZ/Akko/EPOMAKER/MonsGeek/Keydous; replaced IPI/SayoDevice/MADLIONS/Lemokey. Retained analog-keyboard, hall-effect, gamepad, xinput and remaining brand tags. Editorial discoverability selection, not a measured sales/search ranking or support change. Full set: ajazz, akko, analog-keyboard, atk, attack-shark, aula, drunkdeer, epomaker, gamepad, gravastar, hall-effect, irok, keychron, keydous, monsgeek, nuphy, razer, redragon, wooting, xinput. Prior values: .local/topics-before-20260925.json.

> 2026-09-25 PUBLISHED stable/latest1.6.3: https://github.com/PashOK7/HallJoy/releases/tag/v1.6.3 . Source a00182f6d1ccd2676fa7f50e4bbfab65e967c30f; downloaded EXE matches local SHA25673cbc3ba203f96366eef95ae0cdd83711775718a8ab75c3192579aaabb0054c3. All accumulated local changes before this note are released. Local checks PASS; hosted Windows SKIPPED. See RELEASE_1.6.3_PUBLICATION_2026-09-25.md.

> 2026-09-25 release1.6.3 authorized; all accumulated support/layout/detection changes and final README included. Fresh Sheet reconciliation251 yellow/63 green PASS, ordinary Release+6 gates and clean-source static/profile/recovery checks PASS. Publication verification: RELEASE_1.6.3_PUBLICATION_2026-09-25.md.

> 2026-09-25 README: owner explicitly requested the Google Sheet link in the shared keyboard-section introduction. Added full keyboard list and support statuses link beside navigation to both support groups. This supersedes the earlier no-Sheet-link preference for this location. No publication.

> 2026-09-25 README owner edit: custom-firmware requirement/link moved inline to Keychron brand line. Removed standalone Not every hardware revision has been tested and Use a wired USB connection sentences. Documentation presentation only; actual protocol/connection limitations and model-specific qualifiers unchanged. No publication.

> 2026-09-25 README navigation approved: Compatible keyboards for gamepad mode is the shared heading; introductory links point to Supported and Experimental support subheadings and ask readers to check both lists. No physical-testing claim for all Supported entries. Compact brand lines retained. No publication.

> 2026-09-25 README: owner requested removal of the standalone Tartarus Pro explanation after the experimental list; removed. Inline requires Razer Synapse remains. Keyboard-section naming is under discussion; do not call all primary entries physically tested. No publication.

> 2026-09-25 README spacing refinement: within each keyboard list, use Markdown hard line breaks (two trailing spaces plus newline) between bold brand lines, not blank paragraphs. Preserve two separate support sections and all model text. No HTML or font-size tricks; no publication.

> 2026-09-25 README final owner preference (supersedes table choices below): compatible and experimental keyboard lists are plain paragraphs, one per brand, formatted **Brand:** model, model. No tables or HTML; one brand occurrence per paragraph. All 102 brand entries, complete model text and order preserved. Keep this format for future additions. No support-status changes or publication.

> 2026-09-25 README rendering correction: owner screenshot shows raw HTML table markup as text in their viewer. Restored both tables to plain Markdown, left-aligned Brand | Models; all 102 brand rows preserved. Do not reintroduce HTML width controls. Plain Markdown cannot guarantee column percentages; width is determined by the renderer. No publication.

> 2026-09-25 README owner preference: both keyboard tables use Brand | Models, with each brand written once per row and model names without repeated brand prefixes. Readability takes precedence over speculative SEO. Converted both tables with exact reconstruction checks preserving every full model name, order and qualification. Presentation only; no support/status changes or publication. Keep this format in future model additions.

> 2026-09-24 USB identity audit:290 RY boards vs11 saved catalogs,10 differences. Three alternate tuples enabled: Akko MOD007S V3 HE ANSI/ISO and Valkyrie VK Mag75 Max; seven held for protocol/map/retail ambiguity. Exact rejection diagnostics + actual USB telemetry corrected; build-time evidence audit added. Model colors/counts unchanged, live Sheet rows39/727 verified. See USB_IDENTITY_AUDIT_2026-09-24.md.
 Full native checks (resumed after stale-test corrections), final alias regression, firmware packet replay and ordinary Release+6 gates PASS. EXE SHA256 65efa2d73fe5ca4935bacea67b27e5f18a28e410f789768076a6181ef68d320e. No publication.

> 2026-09-24 General communication anomaly warning implemented across native protocol observations + SDK/UAP; explicit RY5088 identity-loss event. No process detection/extra HID polling/forced logging. Owner confirms R68 tester had web configurator open. Regression and ordinary Release+6 gates PASS; no publication/status promotion. See KEYBOARD_COMMUNICATION_WARNING_2026-09-24.md.

> 2026-09-24 R68 HE log32: admission fix confirmed; accepted sample publication starts, but periodic identity check fails twice (worker exit8) with reconnect. No I/O errors; cause of bad recheck reply unknown, contention only a hypothesis. Ask whether vendor app/web HID client was open; no stable-depth/gameplay claim or status promotion. See ATTACK_SHARK_R68_PRELOG_2026-09-24.md latest section.

> 2026-09-24 R68 HE log31 root cause: board3650 replies correctly on PID5029, but catalog profile permits only502D. Local exact alternate pair admitted; original pair preserved. Ordinary Release+6 gates PASS, EXE eb4d6cc93c8f79863c03a1c10e8b7fed271a5f6753fe823ddb2856e889892265. Tester must retry depth; status stays yellow (live Sheet row97 verified). No publication. See ATTACK_SHARK_R68_PRELOG_2026-09-24.md latest section.

> 2026-09-24 OWNER CLARIFICATION: easy visual-layout work covers ALL yellow models, not only IROK. Audited251 entries; added41 models across26 brands (34 presets).97 models now have at least one exact preset;154 remain without one, and regional gaps are explicit. Full static suite, production-linked layout/analog checks,18 recovery scenarios and final ordinary Release+gates PASS; all149 old presets unchanged. No analog status change/publication. See ALL_YELLOW_LAYOUTS_2026-09-24.md.

> 2026-09-24 OWNER SCOPE: visual layouts only for easy, evidence-backed batches; skip difficult/ambiguous geometry. Five JingTai model presets integrated (Mercury68/Pro grouped), missing-preset startup bug fixed; static and linked regression checks,18 recovery scenarios, ordinary Release+mandatory gates PASS. All144 old presets unchanged; no publication or support-status changes. See LAYOUT_EASY_BATCH_2026-09-24.md.

> 2026-09-24 OWNER-REPORTED GAP: recent protocol additions did not include all exact visual layouts/autoselection. Compiled catalog confirms missing presets for five new JingTai V1 models and no Game Arena/EWEADN entries.251 yellow means enabled analog paths, not complete geometry coverage. Prioritize full support-to-layout reconciliation and batch geometry integration. See LAYOUT_EXPANSION_GAP_2026-09-24.md.

> 2026-09-24 LOCAL JingTai V1 expansion: IROK NA87 Pro/ND63/Mercury68/Mercury68 Pro and IYX MU68 Pro enabled experimentally;28 exact VID/PID/product tuples. Native detection, source/firmware-proven physical maps, independent analog, bindings/gamepad; ordinary4mm and Cyan3.6mm ranges from10 saved firmware images, Mercury3.5mm provisional. MG75 Pro preserved. Full native suite, final targeted regressions, ordinary Release+6 gates, production-linked profiles/layouts and18 recovery scenarios PASS. All251 yellow models synchronized with live Sheet:646 models/148 blocks; base/effective colors and unaffected cells verified. EXE58e7156d31b2da5219ae1d10372437e4131fd412a90ee8b13c90416285a6f763. No publication/hardware-test claim. See JINGTAI_V1_EXPANSION_2026-09-24.md.

> 2026-09-24 LOCAL protocol follow-up: Game Arena GKX68 MAGNUM enabled experimentally for two exact USB revisions. RongYuan169 labels/253 revisions; all246 yellow models synchronized with live Sheet (645 rows,147 blocks). Source audit, native stream regression, ordinary Release+6 gates PASS; EXE d71611a705000e9a390abfd98c09b7bbf4ed01db276872e6001621b8e18927b6. YC500 and FL SparkPlayJoy research holds documented with concrete wire differences. No publication/hardware-test claim. See NEXT_PROTOCOL_REVIEW_2026-09-24.md.

> 2026-09-24 LOCAL EWEADN SparkLink batch:17 additional experimental labels /21 wired USB identities, pinned official E HUB3.3.2 SDK/catalog. Existing enabled live layout/travel backend, exact notice tokens and generic banner title. README/hardware/next notes/live Sheet synchronized:245 yellow/63 green/332 gray/4 red,644 models/146 blocks; structure plus base/effective colors PASS. Release+6 gates PASS; EXE3551d1242e2b30a67d4f577a106ef931afff328457caae666773a98c57b79b93. Full native suite PASS (static audits + portable C++); see EWEADN_SPARKLINK_BATCH_2026-09-24.md. No publication or hardware-test claim.

> 2026-09-24 Sheet presentation follow-up: owner screenshot showed repeated brand labels and missing yellow fills despite correct API effectiveFormat. Exact client cause not established. Materialized brand visibility and status colors in base cell formats for all632 model rows; CF retained. Independent readback: base/effective colors PASS, structure146 blocks PASS, all non-color data/formats/rules unchanged. Planner now recomputes base colors after insertion/status edits; structure CLI includes presentation audit;9 Sheet tests PASS. Earlier batch7 API PASS was insufficient to establish client-visible rendering. See SHEET_PRESENTATION_FIX_2026-09-24.md.

> 2026-09-24 LOCAL batch7: 40 new experimental model/variant labels + six existing-model revisions;50 USB revisions /28 brands. RongYuan168 labels/251 revisions, global228 yellow/63 green. Full enabled paths; source/native regression and ordinary Release+6 gates PASS. EXE36fb11b45fe54cf5e82d73213344655c818e5b8684c912a07d1cef59c34625e2. Live Sheet synchronized:632 rows (228 yellow/63 green/337 gray/4 red), structure PASS146 blocks; previous cells/validation/formats verified. Whole479-record catalog ledger:201 prior +50 new,149 retail-mapping leads,44 technical holds,35 other-backend references. New native Sheet batch planner with regression coverage. No publication/hardware-test claim. See RONGYUAN_BATCH_7_2026-09-24.md.

> 2026-09-24 LOCAL sixth RongYuan batch:11 models /18 revisions /8 brands (ATWO, HAVIT, UluGames, GamePro, LOMZ, M4G, Fuego, XINMENG). Full analog/gamepad paths and yellow notices. Source audit128 models/201 revisions, native regression and Release+6 gates PASS. EXE a576807f529a71065384dab4a2e0b6351e18e619ec9dec73db884c6cdb63681c. Live Sheet all188 yellow match; structure PASS130 blocks.595 total/63 green. Batch generator used; mechanical-switch-option hold and sparse Sheet audit fixed. No publication. See RONGYUAN_BATCH_6_2026-09-24.md.

> 2026-09-24 LOCAL batch automation: use tools/rongyuan_batch.py for reviewed RongYuan batches. Inventory479 records:201 revisions need retail mapping,278 held (includes existing). No new support claims. Generator coordinates code/docs/notices and Sheet intent; guarded apply backs up and checks hashes.8 tests PASS. See KEYBOARD_BATCH_AUTOMATION_2026-09-24.md and ../development/KEYBOARD_BATCH_PIPELINE.md. Statuses/build unchanged; no publication.

> 2026-09-24 LOCAL fifth RongYuan batch:7 models /13 revisions /6 brands (BOYI, COLORFUL, MageGee, Royal Kludge, Sunsonny, XINMENG). Full analog/gamepad paths and yellow notices. RK auxiliary-read subclass reviewed/pinned. Source audit117 models/183 revisions, regression and Release+6 gates PASS. EXE c06153d5cf85d7cce19916043aad5322bb13a542db7755b5fcf9aec893ae8b12. Live Sheet all177 yellow match; structure PASS123 blocks.584 total/63 green. No publication. See RONGYUAN_BATCH_5_2026-09-24.md.

> 2026-09-24 LOCAL fourth RongYuan batch:14 models /24 revisions /11 brands. Exact USB admission and full analog/gamepad paths, yellow notices. Source audit110 models/170 revisions, native regression and Release+6 gates PASS. EXE 3be109215b48b3f584a338c7c8efa986d4d3f400d039e15281b2240668cb6ecc. Live Sheet all170 yellow match; structure PASS118 blocks.577 total/63 green. No publication. See RONGYUAN_BATCH_4_2026-09-24.md.

> 2026-09-24 LOCAL third large RongYuan batch: 22 models / 33 revisions / 7 brands (Blackstorm, EWEADN, FREEWOLF, GAMEBOOSTER, Oniverse, Rampage, Valkyrie). Exact USB admission, full analog/gamepad path, yellow notices; ranges provisional 4mm. Strengthened source audit rejects unreviewed class behavior including arrow overrides. Source audit96 models/146 revisions, full-map regression and Release+6 gates PASS. EXE 7878cb3fa33ac04fb1d0e480afdbeef76fb19f457afd14f044bc56c25de30f24. Live Sheet all156 yellow models match; structure PASS107 blocks. 563 total, 63 green. No publication. See RONGYUAN_BATCH_3_2026-09-24.md.

> 2026-09-24 LOCAL second large RongYuan batch: 21 models / 39 revisions / 8 brands (AJAZZ, ARDOR GAMING, ASTROMEDA, DSPIXEL, HATOR, KiiBOOM, MAMBASNAKE, PIIFOX). Exact USB profiles, full analog/gamepad path and yellow notices. Source audit 74 models / 113 revisions, all-key regression, Release + 6 gates PASS. EXE bbde7c72b00d67ee05f4e889f7dad04ca8b2d3a9af51347589f2790c984b31d1. Live Sheet all 134 yellow entries match; structure PASS 102 blocks. Existing values/validation/formatting preserved except three authorized status changes. No GitHub publication. See RONGYUAN_BATCH_2_2026-09-24.md.

> 2026-09-24 LOCAL large RongYuan batch:17 models/22 revisions/10 brands added (AIM1 US, EvoFox, FL ESPORTS, GAMEPOWER, KYSONA, MEETION, Nyfter, OUSAID, SAVIO, Syntech). Exact USB profiles; full input path/yellow/manual layouts; ranges provisional. Source audit53 models/74 revisions and strengthened generic alias regression PASS. Release+6 gates PASS, EXE051baf33b4654db8f4bcb5b780374478cbbeafeea5e04fc7b9a59d4a5438791c. Live Sheet all113 yellow entries match; structure PASS96 blocks; existing values/colors/validation preserved. No GitHub publication. See RONGYUAN_BATCH_2026-09-24.md for full list and deferred leads.

> 2026-09-24 Sheet structure repair: prior cell-only checks missed inherited separator heights and stale block borders. Fixed EPOMAKER/GamaKay/IROK/Keydous/MSI/Skyloong borders, five16px model rows, and567 empty-row dropdowns. Native audit PASS87 blocks; values/model validation/colors/column widths/conditional rules unchanged. Added tools/check_keyboard_sheet_structure.py and mandatory post-insertion procedure in KEYBOARD_SHEET_RULES.md. Before/after snapshots retained in .local. No application/support change or publication.

> 2026-09-24 LOCAL EPOMAKER follow-up: HE65 Mag2376 and HE1083365 enabled experimentally over USB, exact factory matrices, manual layouts and yellow notices. Range4000um/3300um respectively; switch/hardware validation pending. Source audit36 models/52 revisions, all-key regression, Release+6 gates PASS. Live Sheet/README/runtime synchronized: all96 yellow entries match, validation/colors/neighbors preserved. EXE1b5ec538429d681dd268d16ba32056877e1540e31d69e35d484ac29422558945. No GitHub publication. See NEO65_AND_PROTOCOL_EXPANSION_2026-09-24.md latest section.

> 2026-09-24 LOCAL Akko V3 follow-up: MOD007S V3 HE and MOD007B V3 HE exact ANSI/ISO profiles enabled experimentally; do not extend to all original MOD007B/Year of Dragon editions. Profile audit34 models/50 revisions, all-key regression, Release+6 gates PASS. Live Sheet/README/runtime synchronized, all94 yellow entries match. EXE3f999e9cfefd3d2b8036450097deeb46e4a838a5df3bcd5a4596299582c3a6bd. GamaKay TK75v5-to-HEV2 mapping remains unresolved; YUNZII B75pro/generic entries not promoted. No GitHub publication. See NEO65_AND_PROTOCOL_EXPANSION_2026-09-24.md latest section.

> 2026-09-24 LOCAL earlier-Keydous follow-up: exact NJ81-CP2454 and NJ98-CP2576 profiles enabled, not inferred V2/V3 identities. Yellow, USB/manual layouts, switch-scale validation pending. Source audit32 models/46 revisions, all-key regression, Release+6 gates PASS. Live Sheet/README/runtime synchronized, all92 yellow entries match, validation/colors/neighbors preserved. EXE8e038c16e1fbbf89e2373841d8a2a4447ce5a9d86899cb9a1d5a11e278be98b8. LK75 not promoted due mechanical retail-name conflict. No GitHub publication. See NEO65_AND_PROTOCOL_EXPANSION_2026-09-24.md latest section.

> 2026-09-24 LOCAL follow-up: Keydous NJ68 Pro-CP and Skyloong GK61 HE/GK68 HE/GK75 HE enabled with exact board admission and yellow notices; mechanical/optical/MIX namesakes excluded. Source/profile audit30 models/44 revisions, all-key protocol regression, Release+6 gates PASS. Live Sheet/README/hardware/notes synchronized, all90 yellow entries match with validation/colors preserved. EXEbb0d8e4daa5b390b57b934f97bb83ee0b6adfa0c8f5bc88e5474c1d621ab42e8. No hardware-test claim or GitHub publication. See NEO65_AND_PROTOCOL_EXPANSION_2026-09-24.md latest section.

> 2026-09-24 LOCAL follow-up completed: EPOMAKER HE60 Wired/HE60 Wireless/HE75 V2 and IROK Mercury68 SE (JingTai V2) enabled, USB only, exact revision scope; HE75 V2 TMR and other Mercury68 SE protocols not promoted. HE60 is manufacturer-announced/catalogued, not physically tested. All86 yellow entries synchronized with live Sheet/runtime/README. Profile audit26 models/39 revisions, stream/SparkLink tests, Release+6 gates PASS. EXE4ba1a87dd6c2b73a7304781fca18709de274d885f4bf3e20dbb4484ba0b69f2b. No GitHub publication. See NEO65_AND_PROTOCOL_EXPANSION_2026-09-24.md latest section.

> 2026-09-24 LOCAL next wave complete: HE68 Mag, FUN75, STRIKE700 HE and IROK/CAROTMAS Mars75/Mars75 Pro/Mercury68 Max enabled experimentally; exact board/USB scope, manual layouts, switch/hardware validation pending. RongYuan23 models/36 revisions; all82 yellow rows synchronized with live Sheet/README/runtime, validation/colors preserved. Profile audit, stream/SparkLink regressions and Release+6 gates PASS. EXE ec9da51201473cd6910e01cbc5a4944c47380d963eef2b5488f8d37199468ee5. Mercury68 SE remains mixed-protocol research, no blanket promotion. No publication. See NEO65_AND_PROTOCOL_EXPANSION_2026-09-24.md latest follow-up.

> 2026-09-24 LOCAL follow-up: EPOMAKER HE75 Mag and GamaKay NS75 enabled experimentally over USB, exact factory maps/identities and yellow notices; nominal4mm range, switch-specific/physical validation pending. RongYuan20 models/32 revisions. Profile audit, all-key protocol test, Release+6 gates PASS; EXE78dbb3680524df4c8afdffcf3fd5169253133d9b8519c153416915c85c32df06. README/hardware/next notes/live Sheet synchronized; all76 yellow rows reconciled, validation/colors/neighbors preserved. No GitHub publication. Details: NEO65_AND_PROTOCOL_EXPANSION_2026-09-24.md follow-up.

> 2026-09-24 Completed LOCAL expansion: seven new yellow models (Neo65 Sonic HE+ ANSI/ISO, Redragon K617 HE ANSI/BR, EPOMAKER HE68 Lite, IROK MG68 Plus/ND63 Ultra/ND68 Pro/RA68 JingTai V2). Fixed manual-layout RongYuan startup/notices and W669 physical alias release. README/hardware/next notes and live Sheet synchronized; all74 yellow models reconcile, validation/colors/neighbors preserved. Ordinary Release+6 gates and full native suite PASS; EXE464a685ced0c87a94f630653293110da2a7a2a09091544e77d82c553edbd88ee. Ace60 remains research-only/gray: firmware1.20 debug enable rewrites flash configuration; no device writes performed. OEM inventories:475 RongYuan candidate records and44 IROK group symbols, NOT unique retail models or exhaustive global coverage. See NEO65_AND_PROTOCOL_EXPANSION_2026-09-24.md and ../research/protocol-catalogs/README.md. No hardware-test claims or GitHub publication.

> 2026-09-24 Completed LOCAL Tartarus Pro native passive integration: 20 physical analog channels / 256 levels, Synapse required, factory-position bindings, no mode/configuration writes. Final ordinary Release +6 gates and full native static/portable checks PASS. README/hardware/next notes and live Sheet C459 synchronized; all67 yellow models reconcile with validation/colors/neighbors preserved. EXE f396d350c2ef0dee235be39e9133f6cfb77bfe7ad80d1f1006d9a95a89829356. No physical tests or GitHub publication. See TARTARUS_PRO_INTEGRATION_2026-09-24.md.

> 2026-09-24 Continuation completed:8 more experimental USB models (Akko MOD 007 V5 HE/Ray68 HE; GamaKay NS68/TK75 HE; MonsGeek FUN68 HE/M2 V5 HE/M3 V5 HE; Womier M68 HE Pro). Native stream now17 models/26 revisions. All-key protocol regression, source audit, Release+6 gates PASS. README/hardware/notes and live Sheet synchronized; all66 yellow models reconcile with preserved validation/neighbors. Final EXE b720886f1714ec226e6419c8e7abc727cc7eda67f4a883cdc3840626a4bc5d66. LOCAL ONLY, no GitHub publication or physical tests. See RESEARCH_STREAM_INTEGRATION_2026-09-24.md second batch.

> 2026-09-24 Completed LOCAL experimental RongYuan USB stream integration:9 models/15 exact boards (Akko TAC75 HE; GamaKay TK75 TMR;3 Keydous HE models; MonsGeek FUN60 Pro/M1 V5 TMR; Womier SK75 TMR; YUNZII RT75 Pro). README/hardware/next notes/runtime and live Sheet synchronized; all58 yellow models reconcile, validation/colors/neighbors verified. Full native checks,18 recovery scenarios, layouts and Release+6 gates PASS. EXE SHA256 c23913389e942396f20f95de98bfeb6df467486e0572dec028ccb8df8296d440. No hardware test, flashing, forced logging or GitHub publication. See RESEARCH_STREAM_INTEGRATION_2026-09-24.md.

> 2026-09-24 Published stable/latest HallJoy1.6.2 (395fdbe), EXE ceb47c21bfb7ff12efccd6e15853c8fddc6a2793e8dfc95f39bc121dbe4a0183. Owner accepted layout fix. All local checks and portable CI PASS; GitHub Windows job verified SKIPPED, now manual-only per owner. Sheet/README/runtime reconciled. See RELEASE_1.6.2_PUBLICATION_2026-09-24.md. Prior local-only restriction superseded for released changes; private correspondence still excluded.

> 2026-09-24 Owner authorized stable1.6.2 publication including layout reconnect fix. Windows GitHub CI must not run for this release; Windows job now manual workflow_dispatch only, portable CI retained. Local Windows release gates/tests remain mandatory. Live Sheet re-read: R85 HE/AJAZZ RGB green, all49 yellow match runtime; no edits needed. Publication in progress.

> 2026-09-24 Follow-up layout fix: startup cache alone did not cover K4 USB reenumeration after first detection. Searching/Missing now preserve geometry as an unlocked preview while clearing live remap ownership; same factory preview revalidation keeps render snapshot. Multiple/invalid-map fallback unchanged. Linked tests/18 recovery scenarios and ordinary Release +6 gates PASS; EXE ceb47c21bfb7ff12efccd6e15853c8fddc6a2793e8dfc95f39bc121dbe4a0183. See AUTOMATIC_LAYOUT_STARTUP_CACHE_2026-09-24.md. Owner visual check pending; local only.

> 2026-09-24 Fixed startup layout jump: last confirmed automatic preset name is now saved separately from manual fallback and restored as geometry-only preview pending discovery. Live identity/remaps still revalidated; first updated run must detect/save before future starts benefit. Linked profile/layout/cache tests and18 recovery scenarios PASS; updated ordinary1.6.2 +6 gates PASS. See AUTOMATIC_LAYOUT_STARTUP_CACHE_2026-09-24.md. No support-status change or publication.

> 2026-09-24 Owner approved R85 HE Supported. Prepared ordinary 1.6.2: common Shark shared-access fallback enabled; R85 HE yellow removed without affecting Ultra shared layout; AJAZZ AK820 MAX RGB and K4 retained; no forced diagnostics. README/hardware/patch notes synchronized; live Sheet C83 Supported green, C14 AJAZZ remains green, all49 yellow models match runtime. Release and6 linked gates plus notice regression PASS. LOCAL ONLY, no GitHub publication. See RELEASE_1.6.2_PREPARATION_2026-09-24.md.

> 2026-09-24 R85 log30 confirms real analog in diagnostic revision3: exclusive open fails32, shared RW succeeds, exact3123/USB0511,4727 valid pages, changing/releasing WASD,0 I/O errors. Conflicting handle owner unknown. Ordinary fallback remains disabled; production integration and range review still pending before support promotion/sync. No status change or publication in log analysis. See ATTACK_SHARK_R85_LOG28_2026-09-24.md.

> 2026-09-24 R85 pre-send audit: revision3 removes timer-arm/wait failure as a silent pre-command blocker, using logged cancellable fallback. Actual Win32 failed-timer/cancel regression and all7 gates PASS; latest EXE replaces revision2. No physical R85 communication guarantee; next targeted log still needed. See ATTACK_SHARK_R85_LOG28_2026-09-24.md.

> 2026-09-24 R85 log29 localizes failure to exclusive CreateFile before ANY protocol command; exact Feature65/FFFF:2 collection is found and digital60/60 recorded. Fixed diagnostic error-code loss caused by timer initialization; revision2 adds R85-only shared/feature-only opening fallbacks, actual Win32 regression and passes7 gates. Hardware success pending new log, no firmware/support-status conclusion or GitHub publication. See latest ATTACK_SHARK_R85_LOG28_2026-09-24.md section.

> 2026-09-24 Owner explicitly requests detailed R85 HE diagnostic, superseding ordinary-log-only next step. Targeted PID5029/id3123 wire/caps/digital/page diagnostics implemented; exact manufacturer client reviewed. Exact R85 firmware API still Record not found and separate installer download stalls; related-firmware component emulation is NOT an R85 root-cause proof. Await targeted tester log, no support-status change or GitHub publication. See ATTACK_SHARK_R85_LOG28_2026-09-24.md.

> 2026-09-24 Owner confirms K4 works perfectly after restoring onboard support in the build. Previous shallow-press latency report is resolved by owner feedback; preserve mandatory K4 linked catalog gate. New R85 HE log28 shows expected USB PID but no connected analog source; ordinary support report lacks failure-stage data. Investigating discovery, not yet a support-status change or proof of unsupported firmware. See ATTACK_SHARK_R85_LOG28_2026-09-24.md. No GitHub publication.

> 2026-09-22 Owner reports ~250ms shallow-press delay on K4 everywhere. Found host-build regression: ordinary build script omitted custom K4 onboard flag despite previous docs saying retained. Restoring flag and mandatory linked catalog gate in both release/K4 builders; firmware r5 unchanged. This is a confirmed build defect, NOT yet proof of the exact symptom's cause or resolution. See newest K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md section; owner retest pending. No GitHub publication.

> 2026-09-22 Owner authorized ordinary AJAZZ AK820 MAX RGB support, local only. Enabled exact tester firmware via native raw stream, retained tested dynamic bounds/Fn, no forced logging; corrected Windows-topology-triggered ViGEm startup restart loop. README/hardware/next patch notes updated. Sheet exact RGB row A14:C14 Supported/green with validation readback; broader AK820 MAX HE and Ultra unchanged. All50 yellow models reconciled. No new physical test or automatic AJAZZ geometry claim. See latest AJAZZ_AK820MAX_REVIEW_2026-09-20.md section for final validation/artifact. Do not publish to GitHub.

> 2026-09-22 AJAZZ AK820 MAX RGB revision5: owner relays successful hardware test; log27 confirms532083 valid raw packets/82 ready keys. Still diagnostic-only, ordinary Release disabled. Sheet C13 corrected to Research incomplete pending production integration; gray/validation/neighbors verified, all50 yellow rows reconciled. Separate unresolved old-build ViGEm generation churn (2670 generations/89s) retained for investigation. No tester re-confirmation needed for revision5 success; no code/build or GitHub changes. See AJAZZ_AK820MAX_REVIEW_2026-09-20.md latest section.

> 2026-09-22 Delivered LOCAL experimental USB support for MonsGeek M1 V5 HE, Chilkey Slice75 HE and EPOMAKER G84 HE: native matrix reads, exact maps/automatic layouts, bindings/gamepad and yellow notices. Full static/portable/Windows checks and ordinary Release build + five linked gates PASS. EXE SHA256 23a50bbe22164c6da74377b11bd1836f1a7341acaa13aaf53d97ca95fedce840. README/hardware and live Sheet C142/C209/C406 synchronized; all50 yellow models read back and reconciled. No hardware test, flashing, forced logging or GitHub publication. See MONSGEEK_AKKO_PROTOCOL_2026-09-22.md.

> 2026-09-22 Active MonsGeek/Akko integration research authorized. Downloaded official firmware; four pinned snapshot getter fragments and a current-depth producer pass synthetic MCU execution. Host freshness, ranges and exact maps remain pending; no runtime/support claim yet. Additional Slice75 project retained as a separate protocol lead. See MONSGEEK_AKKO_PROTOCOL_2026-09-22.md. Continue implementation and mandatory status sync; no GitHub publication requested.

> 2026-09-22 Discoverability: owner wants organic discovery without renaming or awkward public copy. Added and read back 19 GitHub Topics: analog-keyboard, hall-effect, gamepad, xinput; atk, attack-shark, aula, drunkdeer, gravastar, ipi, irok, keychron, lemokey, madlions, nuphy, razer, redragon, sayodevice, wooting. Each brand already has supported models; no support change. Repository search `irok gamepad repo:PashOK7/HallJoy` now returns 1 (previously 0), without in:readme. Name, description and README unchanged. No external search ranking claim.

> 2026-09-22 Owner authorized publication after editing README. Published exact owner README (including blank table headers) to main: 5285c67974249c7d907e4a126a0f72424e9d0654. GitHub description updated to the prepared draft. Remote README bytes and description read back successfully. Support inventory/status groups unchanged. Prior no-publication instructions for this README draft are superseded; private correspondence remains local only.

> 2026-09-22 README draft revised after owner feedback: restored original structure/copy, removed repeated steering/movement explanations. Both compatibility tables now use one Models column with full brand-prefixed model names; alphabetical brand grouping and statuses unchanged. Description shortened. Still LOCAL ONLY; no publication authorized.

> 2026-09-22 Owner requested README/search-discoverability rewrite and GitHub description draft, explicitly NO UPLOAD. Local README updated; proposed description and qualitative research in README_SEARCH_DRAFT_2026-09-22.md. Remote README/description/topics untouched. Same support models/statuses; preserve single-author voice. Do not publish these edits until owner authorization.

> 2026-09-22 Owner authorized removal of obsolete GitHub branches. Deleted v1.4-integration (07fc13bf0fb6d5cf48f7484232836d5e3a8db622) and codex/explain-code-interaction-capabilities (a3f7dcf8cf6d473978f97f1341f1b19c3620ebde) after a fresh fetch and merge-base --is-ancestor checks against origin/main. GitHub readback confirms only main remains. Commit history and release tags are preserved.

> 2026-09-22 Repository root reorganized at owner request: patch notes in docs/releases/, detailed hardware inventory in docs/SUPPORTED_HARDWARE.md, commercial terms in docs/legal/, contribution guide in .github/. Links updated; root keeps README, LICENSE, THIRD_PARTY_NOTICES, BUILD.cmd, AGENTS and dotfiles. See PROJECT_LAYOUT.md. No binary or support-status change. Local-only Pwnage correspondence excluded from publication.

> 2026-09-22 Owner supplied Pwnage email history: wrote to support@pwnagesupport.zendesk.com on Sep 15; followed up Sep 21. Dimas replied Sep 21, 3:00 PM PDT, promising to consult engineering on live analog access for Zenblade 65 V2 and other Pwnage keyboards. Awaiting engineering follow-up; no protocol/firmware or compatibility confirmation yet. Local correspondence summary: [Pwnage contact record](PWNAGE_SUPPORT_CORRESPONDENCE_2026-09-22.md). Do not publish correspondence without explicit request.

> 2026-09-22 Published stable/latest v1.6.1, tag 7e12928, EXE SHA256 d67c8f4d1128de4dac19405e93b4ea031f75b02bba7fbaa39da407ad5b7badf5. All required local gates and Linux CI PASS; Windows CI still running at publication, not a claimed pass. Owner questioned waiting for duplicate remote checks; release proceeded with completed local evidence. See [publication record](RELEASE_1.6.1_PUBLICATION_2026-09-22.md).

> 2026-09-22 Owner authorized GitHub release 1.6.1. Source commit bc670e9 pushed; full local build and GitHub Linux/Windows CI are in progress. Public release/assets are not published yet. Release gates corrected for exact expanded HERO UUID list and MINI60 vendor Fn action. Live Sheet rechecked: all 47 yellow models match catalog; support decisions unchanged.

> 2026-09-22 README now separates all 47 yellow models into an alphabetically sorted experimental Brand/Models table, without internal-document or Sheet links. Ordinary table excludes those models. RELEASE_NOTES_NEXT.md is an unreleased draft with the same full experimental inventory and the new AULA additions; published 1.6.0 notes remain unchanged. Documentation only: no new support decision, binary or Sheet changes.

> 2026-09-22 Owner clarified yellow means a COMPLETE implemented input path (detection, analog, bindings and gamepad), with remaining hardware/range nuances; a physical tester is NOT a prerequisite. Research-only or disabled support must not be colored yellow. Delivered nine AULA family additions and model-specific runtime notices; all 47 yellow Sheet models reconciled with generated catalog and live readback. NuPhy unresolved report-mapping variants are gray; Aurora 75 shares supported QBZ75 route. Build and five linked gates PASS; K4 onboard retained, camera test disabled, no forced logging, no GitHub publication. See [integration](AULA_EXPERIMENTAL_SUPPORT_SYNC_2026-09-22.md).

> 2026-09-22 Camera latency test disabled completely at compile time; source retained. Added wired base AULA MINI60 HE (V1.18) using verified existing stream, own identity/layout. New build and all four linked checks PASS, K4 onboard retained. README/hardware/Sheet synchronized (base C122 Supported green; MINI68 catalog added, remaining reviewed models Research incomplete for specific technical gaps). No physical tests or GitHub publication. See [AULA review](AULA_KNOWN_PROTOCOL_REVIEW_2026-09-22.md).

> 2026-09-22 MINI 60 HE MAX wired support implemented and delivered; separate MAX layout/identity, common analog protocol, vendor Fn mapping. Build and linked checks PASS for PRO/MAX; K4 onboard retained. Sheet C123 Supported/green readback verified; README and SUPPORTED_HARDWARE synchronized. USB only; no receiver/Bluetooth or physical-test claim, no GitHub publication. See [MAX integration](AULA_MINI60_MAX_SUPPORT_2026-09-22.md).

> 2026-09-21 Keyboard Sheet: owner prohibits cell comments/notes. Read [mandatory Sheet rules](../development/KEYBOARD_SHEET_RULES.md). Added 12 missing AULA variants and IO Type 68 Magnetic Pro Wireless (catalog only; no firmware research). MINI 60 HE MAX V1.52 has the existing PRO command/report family but current HallJoy rejects its identity; yellow Known protocol; integration required, not Supported. See [AULA audit](AULA_CATALOG_GAPS_2026-09-21.md).

> 2026-09-21 r5 precision firmware installed/readback verified. Onboard gamepad now consumes fresh pre-gate ADC through existing calibration/polynomial and float curves, bypassing legacy5-count gate and241-level travel. Digital/rapid-trigger paths unchanged; UI key telemetry still241levels, real pad tester independent. Owner W capture2560samples observed842precise depth values vs237legacy; separate release512samples allzero. Noise remains, not842proven stable positions. Existing EXE compatible/unchanged; HallJoy closed and recording finished. See K4 checkpoint for backup/hardware evidence.

> 2026-09-21 Owner confirms improved in-game latency vs UAP and accepts current feel. New focus: increase meaningful analog resolution and find measured ceiling without adding latency. Current nominal241levels/key; actual shared mapper enumerates241 stick/trigger codes with linear curve. Saved raw spans825..1173 counts show potential headroom, not stable resolution proof. Preserve calibration, measure pre-gate raw noise before replacing legacy5-count gate/8-bit travel. See K4 checkpoint.

> 2026-09-21 Owner requires an ANALOG scale, not binary camera flashes. Replaced camera tester patch with buffered signed per-axis scale and0..100% triggers, raw value/percentage, no smoothing. Direct fast OS read preserved; build and linked checks passed, EXE delivered. White-stuck report is not proven firmware stuck input; previous any-nonzero/max-axis visualization was unsuitable. See GAMEPAD_LATENCY_TESTER_2026-09-21.md.

> 2026-09-21 Delivered Gamepad Tester > Camera latency test: independent direct Windows controller observation/render thread, high-contrast patch and explicit CSV export. Headless real-device poll gaps median0.9974ms / p95 1.0648ms; not physical latency or GUI validation. Firmware remains r4. Owner camera check next. See [tester checkpoint](GAMEPAD_LATENCY_TESTER_2026-09-21.md).

> 2026-09-21 K4 onboard r4 installed and new EXE delivered. Owner confirmed r3 functionality but slow HallJoy UI despite smooth browser tester. Fixed real Windows controller monitoring independently of HID telemetry/profile upload; compact key snapshots measured48.6Hz, exact travel retained. Final production worker/headless lifecycle, static audits, release build and linked checks PASS. Full flash readback verified; calibration preserved; HallJoy closed. Owner visual smoothness check is next. Legacy scanner remains~2.3ms: no1000freshHz/onset claim. See current K4 checkpoint.

> 2026-09-21 Experimental K4 onboard r3 installed with full flash readback verified. First integrated HallJoy candidate delivered to build/bin/Release/x64/HallJoy.exe (SHA256 8d3485914b037a0b53254fabd399853c8f89d79fc523dda5326972c11339ecc9). Exact native claim, one firmware-owned gamepad, profile worker, actual pad-report monitor, pause/close admission and no ViGEm duplicate are implemented behind HallJoyKeychronOnboardExperimental. Real neutral-profile worker test PASS; owner application/binding test is next. Legacy scanner ~2.3 ms and slow diagnostic telemetry remain; do not claim ideal latency/1000 Hz or final UI smoothness. No agent GUI run or GitHub publication. See current K4 checkpoint.

> 2026-09-21 Owner may have pressed other keys during WASD recording, so small arrow depths do not establish sensor noise. S has silicone O-rings limiting mechanical travel. Preserve individual calibration and recalibration; never hardcode travel/calibration for this owner. Observed 100% means current calibrated range, not proof of mechanical bottom.

> 2026-09-21 Owner confirms r2 ordinary typing works and Windows joy.cpl is empty with HallJoy closed. User-visible controller disappearance is verified alongside XInput/DirectInput diagnostics. Legacy WinMM metadata remains unresolved and does not block host integration. New HallJoy provider/output ownership and scanner redesign are still pending.

> 2026-09-21 Latest hardware state: experimental K4 onboard r2 is installed and running in ordinary keyboard mode (not DFU). Full flash readback verified. XInput appears during a neutral-profile diagnostic session and disappears on heartbeat loss; DirectInput reports zero attached game controllers afterwards. WinMM still returns an old K4 entry despite no HID gamepad usage, so full user-visible disappearance is not yet declared verified. Next owner check: ordinary typing and Windows joy.cpl list with HallJoy closed. Full app integration/scanner redesign remain pending; no release EXE changed.

> 2026-09-21 K4 hardware integration is now real: exact original flash backed up twice; first experimental onboard firmware flashed and full readback verified. Native XInput neutral-profile cycle and watchdog disappearance PASS; saved calibration unchanged for all 100 keys. Legacy scanner remains ~2.3-2.4 ms, no 1000-Hz claim or HallJoy UI integration yet. WinMM retained old generic-joystick identity; revision 2 adds a stable serial suffix to invalidate that stale collection identity. Software DFU maintenance command works, so further flashing does not require Esc. See [live checkpoint](K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md).

> 2026-09-21 Owner resolved onboard scope: first deliver ONE native K4 gamepad without mouse. Mouse handling is future work; additional gamepads will later use ViGEm, not part of this implementation. Owner requires native gamepad to disappear outside HallJoy and accepts brief whole-keyboard USB reconnect on mode changes. Do not substitute a permanently enumerated neutral controller. Portable session/mapping/curve cores now pass tests; firmware/app integration and scan redesign still pending. See [checkpoint](K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md).

> 2026-09-21 Owner now explicitly authorizes autonomous implementation, hardware diagnostics and flashing the connected K4 HE. Earlier no-flashing-authorization notes are superseded. Preserve backups/recovery and validate before flashing; stop only for necessary owner questions/actions. No flash has yet been performed.

> 2026-09-21 Confirmed onboard K4 requirements: native gamepad mode must only operate while HallJoy runs; never persist an always-on enable flag. Default OFF at boot, explicit session activation, orderly disable and firmware watchdog on lost host. Analog UI must remain smooth without coupling rendering or telemetry to gamepad delivery; no artificial low-FPS policy. See [lifecycle and telemetry design](K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md). These are requirements, not delivered behavior.

> 2026-09-21 Latest K4 direction: owner proposes running bindings/curves inside the keyboard and using its native XInput instead of ViGEm, with HallJoy as editor and analog monitor. This supersedes the pending question about sacrificing native XInput for bulk analog transport: retain native XInput and evaluate onboard execution. Legacy UAP/ViGEm stays for other devices. Existing onboard prototype is incomplete; no native-driver readiness, fresh-1000-Hz or zero-latency claim. See [active work](K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md).

> 2026-09-21 K4 owner clarification: rewrite scanner and transport in his new firmware; legacy FAR need not remain in that image. HallJoy must support BOTH legacy UAP for existing users and explicitly negotiated new firmware. Preserve legacy binaries/source backups. Scanner redesign is authorized; native standalone XInput preservation is the pending transport tradeoff. See [active work](K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md).

> 2026-09-21 Active K4 HE firmware/protocol work: connected ANSI device confirms FAR; current full-matrix request median 4.999 ms. Legacy sources and firmware backed up and hash-verified. Final compiler preprocessing confirms K4 ADC /8: scanner source has 1.6213 ms minimum sequential scan work (earlier /4 estimate superseded), NOT a measured hardware maximum. Preserve old UAP fallback; no flashing, sensor tradeoff or new protocol delivery yet. See [checkpoint](K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md).

> 2026-09-21 Read-only Keychron FAR/UAP latency review: 1 ms request start cadence; representative Q1 scanner has 40 us settling per column (600 us per scan); all-fragment publication and depth threshold matter. Event-driven host delivery, no mandatory 50/8 ms input delay. No end-to-end timing measured; no finite fault-path transaction bound. See [review](KEYCHRON_UAP_LATENCY_REVIEW_2026-09-21.md). No runtime or support-status change.

> 2026-09-21 Mandatory support-status synchronization: changes to support, tester conclusions, known restrictions or release support claims must be reconciled with README, SUPPORTED_HARDWARE and the live Google Sheet in the same task. Read back the edited Sheet cells and record evidence before declaring completion; if access fails, explicitly record synchronization as pending. See [required workflow](../development/SUPPORT_STATUS_SYNC.md). ATTACK SHARK X65 Pro HE Main!C93 corrected from obsolete Block Bound Keys retest pending to Supported; green and unchanged validation verified.

> 2026-09-21 Owner correction: individual physical-device testing is NOT a mandatory Supported gate when the established protocol and implementation are supported by evidence. Wooting 80HE+ is Supported; Keychron K6/Q2/Q4 HE ANSI are Supported with custom firmware. NuPhy Field75 HE is Supported; BH65, Field75 HE V2, Halo65 HE and wired WH80 share the manufacturer protocol route but remain yellow for a specific unresolved depth-field/scale mapping, not merely lack of a tester. Razer firmware download was not attempted; Tartarus Pro uses a different report format. See [protocol review](BRAND_PROTOCOL_REVIEW_2026-09-21.md).

> 2026-09-21 Owner limited expansion to known protocols and available Keychron source; no different-protocol reverse engineering or closed-firmware work. Implemented K6/Q2/Q4 HE ANSI with custom FAR firmware, independent Fn2 and correct fragment count; Wooting 80HE+ ordinary/split ANSI/ISO. Four Sheet rows are yellow, awaiting testing. No new Razer admission. See [implementation](KNOWN_PROTOCOL_ADDITIONS_2026-09-21.md).

> 2026-09-21 Read-only Sheet review of Wooting/Keychron/Razer: existing 9/14/5 supported entries are correctly present. Remaining 80HE+ has plausible generic-protocol compatibility; extra Razer revisions lack explicit admission; several Keychron rows have unfinished known layout/physical-key work and should not be called wholly uninvestigated. No blanket Supported promotion or Sheet edit performed. Preserve the confirmed Keychron custom-firmware premise. See [review](KEYBOARD_SHEET_WOOTING_KEYCHRON_RAZER_REVIEW_2026-09-21.md).

> 2026-09-21 HallJoy has one author, not a team. Public copy must address the author in the singular ("write to me"), never imply a team with "us" or "we".

> 2026-09-21 Owner rejected the unsolicited support-report guide and long user instructions. Public support guidance must simply invite users to Discord for problems or new keyboard requests. README simplified; report template removed from the support page. Do not restore checklists, required fields or log-collection instructions in this flow without owner request.

> 2026-09-21 Owner requested alphabetic brand ordering in the README compatibility table. Sorted all 15 rows case-insensitively; model descriptions unchanged.

> 2026-09-21 Owner explicitly removed Known limits from the 1.6.0 public changelog. Do not add that section back to release notes without owner instruction. Published release body and maintained RELEASE_NOTES_v1.6.0.md updated; binary unchanged.

> 2026-09-21 Published stable/latest v1.6.0 after owner approval and passing Linux/Windows CI. Final EXE SHA256 9d42bc7c, release tag 1c244bdb. NA87/MINI60 were already public in 1.5.3. See [publication record](RELEASE_1.6.0_PUBLICATION_2026-09-21.md). Bounded failure prehistory retained per owner confirmation; no further logging optimization requested.

> 2026-09-21 Owner requested dual continuous logging: with Enable logging on, write HallJoy.log to the normal AppData root and beside EXE; OFF stops the EXE-side copy. Portable mode keeps one data-root log beside EXE. Implemented independent bounded writer destinations and retry/error state; production HallJoyCrash.txt now uses the app data root too. No forced logging or keyboard-specific tests. See latest [prerelease record](PRERELEASE_2026-09-21.md).

> 2026-09-21 Owner requested AppData-only private UAP DLL storage. Implemented for ordinary and portable mode; executable-directory fallback removed. Rebuilt 1.6.0, verified extraction/reuse and exact bytes without UI/hardware tests; old local delivery DLL backed up and removed. Latest artifact hash is in [prerelease record](PRERELEASE_2026-09-21.md). Normal support log stays in the app data root; crash-only HallJoyCrash.txt can still appear beside EXE.

> 2026-09-21 Owner approved ordinary ATTACK SHARK X65 Pro support and requested a normal 1.6.0 prerelease without forced logging or keyboard-specific diagnostic builds/tests. NA87 and AULA MINI 60 HE Pro remain ordinary support, prepared locally but not yet published on GitHub. New tester screenshots confirm browser gamepad output and now working gameplay; earlier Forza/Roblox input-mode switching is no longer reproducing, cause unknown. Remove the X65 Pro testing notice; retain other family notices and protocol limits. See [prerelease record](PRERELEASE_2026-09-21.md).

> 2026-09-21 ATTACK SHARK tester log26 received for the open Block Bound Keys issue. X65 Pro identity2308, input-path-r1: native analog and controller publications continue with Block ON; 113 hook events suppressed, no native I/O/output errors. Game consumption and exact symptom remain unknown; do not call fixed. See [log26 review](ATTACK_SHARK_BLOCK_LOG26_2026-09-21.md). No runtime/EXE change.

> 2026-09-20 Owner prioritized a reliable firmware research foundation before broad acquisition and authorized autonomous preparation until a consequential question. First foundation implemented: unified extraction/recovery, image/source/evidence layers, pinned environment and exact-image emulator dispatcher. 73 tests PASS; existing no-light AJAZZ suite actually rerun with mocked peripherals, no RGB/hardware claim. See [architecture](FIRMWARE_FOUNDATION.md) and [current runbook](FIRMWARE_CORPUS.md). No runtime, EXE, Sheet or support-status changes.

> 2026-09-20 Firmware corpus continuation: atomic object/report publication and resumable ZIP jobs implemented; 20 tests pass, 31 real ZIP jobs complete, all 736 existing objects preserved. Native job migration and cross-format budgets remain pending. See [corpus checkpoint](FIRMWARE_CORPUS.md) and [handoff](FIRMWARE_AUDIT_HANDOFF_2026-09-20.md). No runtime, EXE or support-status changes.

> 2026-09-20 Fresh-chat entry point: [firmware audit handoff](FIRMWARE_AUDIT_HANDOFF_2026-09-20.md). Preserve the owner's requirements for reusable batch acquisition/extraction/analysis, no repetitive manual per-image research and no protocol conclusions from naive byte/text matches. The existing corpus is a pilot, not a finished universal analyzer.

> 2026-09-20 Owner requested a reusable firmware acquisition/extraction and later batch-analysis system while awaiting Attack Shark/AJAZZ testers. Initial corpus pilot uses the 473-record/85-brand Sheet and official Illumi manifest. No support changes from fingerprints; see [firmware corpus](FIRMWARE_CORPUS.md).

> 2026-09-20 Owner selected AJAZZ immediate dynamic per-key limits (revision5), explicitly preliminary. First raw sample=release; log25 full-press minima/median1353 seed; expand both bounds independently. No learning wait. Title/log label; future Configuration per-key bounds/calibration/persistence documented. SHA25607d71c70. See [AJAZZ review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 AJAZZ revision4 candidate SHA2567fb3d4ae: automatic session-local per-key learning from fresh analog pairs;0x23 output only once curve ready, no manual procedure/digital fallback.82-position RGB mask, Fn250->1033. First-use learning, accuracy and drift NOT hardware-verified; no factory parameters/persistent calibration claimed. See [AJAZZ review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 AJAZZ log25 confirms RGB0x23:189706 valid raw rows,82 sensor positions; startup406ms. S event cache30/40 despite raw in observed release range. Fn0xFA has real depth/raw data. Mapping has107 assigned slots vs82 sensors; normalization/physical map still required. See [AJAZZ review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 AJAZZ revision3 research EXE delivered, SHA256f6dfd99b: exact identity/map claim before other providers;0x21 plus0x23 raw rows logged. AJAZZ analog publication disabled pending lossless-state/normalization proof. Numeric map assignments survive log redaction.15 mock scenarios, firmware emulation,4 EXE gates and local startup PASS; RGB raw support still needs tester. See [AJAZZ review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 AJAZZ log22/tester rejects play readiness: stuck analog after release and Fn not detected. MAD68/addressed delay gone, W669 still15.5s; play starts19.766s. Map IDs redacted; Fn diagnosis unresolved. Do not call candidate fully working; see [AJAZZ review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 AJAZZ playable diagnostic revision2 delivered (SHA25674e1673d). Confirmed0x21 events now drive analog using0x10 device map; no digital substitution/expiry releases/0x23. Removed unrelated enumeration string requests; W669 identity fallback preserved lazily. Local worker starts250ms; tester startup and missed releases still need verification. Automatic log beside EXE, manual layout; see [AJAZZ review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 AJAZZ log21 confirms real RGB M484 SG8994HERGB V1.13.17 depth events (2192 / 31 positions), but diagnostics start only after88s due to other provider probes. Depths remain log-only; simultaneous reliability unproven. No-light firmware command0x23 full raw ADC sweep passes emulation; normalization/RGB verification pending. See [AJAZZ review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 AJAZZ full local startup/normal-close check PASS after log20: fixed support-log filter, added provider/engine stages and UI health. Eleven mocked diagnostic scenarios and linked log-route gate PASS. UAP/device/gamepad startup observed, not physical key validation. Target RGB unavailable; missing worker startup in log20 still unexplained. EXE20101bd4 at build/bin/Release/x64/HallJoy.exe. Normal tester launch has no timeout. See [AJAZZ review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 AJAZZ tester EXE delivered at build/bin/Release/x64/HallJoy.exe, now diagnostic (not ordinary release). Opt-in build_ajazz_diagnostic.ps1; automatic single log, no duration cutoff, exact M484 SG8994HE/SG8994HERGB gate, depths logged only. Generic UAP-refresh restarts suppressed only in this diagnostic; not a production hotplug fix. Eight mock scenarios and four EXE checks PASS. See [AJAZZ record](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 AJAZZ follow-up: owner explicitly authorized further analysis of downloaded no-light SG8994HE firmware. Real matrix-loop and command tests reproduce four cached depths/one event, overwritten release, and no recovery on stationary samples or resubscription. Synthetic ADC/RAM and scheduling limits apply; RGB not tested; other top-level reads not exhausted. See [review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md). No runtime changes.

> 2026-09-20 AJAZZ target correction: owner means AJAZZ x NACODEX AK820 MAX RGB, white/lilac, wired/no screen/no Bluetooth. Downloaded SG8994HE V1.13.02 is NO-LIGHT, not the target RGB firmware. Its event overwrite fragment test passes but cannot establish RGB behavior. Exact identity/RGB firmware still needed; no runtime change. See [AJAZZ review](AJAZZ_AK820MAX_REVIEW_2026-09-20.md).

> 2026-09-20 latest catalog expansion: Main now A1:C558, 473 records / 85 brands. Added ABKO 1, COX 3, Monstargear 6, WAIZOWL 1; all Not investigated. Preserved all 462 prior records/validations; Pwnage V2 now C423. See [Korean-market expansion](KEYBOARD_SHEET_KOREA_EXPANSION_2026-09-20.md). No runtime changes.

> 2026-09-20 previous Japan-focused catalog expansion: Main now A1:C543, 462 records / 81 brands. Added AIM1 1, ELECOM 3, REALFORCE 2 (capacitive), Wraith 1, ZENAIM 3; all Not investigated. Preserved all 452 prior records/validations; Pwnage V2 now C410. AZLA ordinary OCTOPUS 8K excluded; HE remains deferred. See [Japan-focused expansion](KEYBOARD_SHEET_JAPAN_EXPANSION_2026-09-20.md). No runtime changes.

> 2026-09-20 previous optical/TMR expansion: 452 records / 76 brands. Added Razer revisions, VARO, Sony and Lofree Hyzen (announced). See [optical/TMR expansion](KEYBOARD_SHEET_OPTICAL_TMR_EXPANSION_2026-09-20.md).

> 2026-09-20 previous regional expansion: 440 records / 73 brands. Added IROK Mars/Mercury/ND63 families, VTER, Durgod, MelGeek and Neo. Jeet65 identities remain deferred. See [regional expansion](KEYBOARD_SHEET_REGIONAL_EXPANSION_2026-09-20.md).

> 2026-09-20 previous expansion: 419 records / 70 brands. Added Darmoshark, EWEADN, Machenike, ROCCAT and WOBKEY. Distinguish magnetic TOP75/DEEP80 from mechanical; Isku+ Force FX has six analog keys. See [five more brands](KEYBOARD_SHEET_FIVE_MORE_BRANDS_2026-09-20.md).

> 2026-09-20 previous mainstream expansion: 401 records / 65 brands. Added Acer Predator, Cooler Master, Ducky, HyperX and Pulsar. MSI STRIKE 600 V2 excluded due conflicting switch descriptions. See [mainstream analog expansion](KEYBOARD_SHEET_MAINSTREAM_ANALOG_2026-09-20.md).

> 2026-09-20 permanent catalog scope: Owner explicitly wants ALL analog keyboards, including inductive sensing, not only Hall Effect/TMR. Label inductive models explicitly. Previous expansion reached 391 records / 60 brands. See [analog expansion](KEYBOARD_SHEET_ANALOG_EXPANSION_2026-09-20.md).

> 2026-09-20 previous owner-suggested expansion: 377 records / 55 brands. Added CIDOO C75/C80 and Red Square Alumix 68/104 Yotei among 21 entries. Keep Red Square distinct from IO. VALOR/BIGATECH unconfirmed; CyberLynx M68HE retail-only evidence. See [owner-suggested expansion](KEYBOARD_SHEET_OZON_BRANDS_2026-09-20.md).

> 2026-09-20 previous catalog expansion: Main then A1:C405, 356 records / 49 brands. Added AJAZZ 13, VGN 8, Fantech 2, CIDOO 1, Shortcut Studio 1 and RAKKA 1. Sources and exclusions: [seven-candidate expansion](KEYBOARD_SHEET_SEVEN_CANDIDATES_2026-09-20.md).

> 2026-09-20 previous catalog expansion: Main then A1:C373, 330 records / 43 brands. Added 26 magnetic configurations from DAREU, Higround, Keydous and SIKAKEYB, all Not investigated; NJ81 MAX-CP TMR explicitly marked announced. All prior 304 records and per-row validation preserved. Sources and model/variant caveats: [four-brand catalog](KEYBOARD_SHEET_FOUR_BRANDS_2026-09-20.md).

> 2026-09-20 catalog audit and owner correction: Rechecked all 149 additions for magnetic sensing; no conventional-only model established. Some boards have mixed key types or accept mechanical switches, so do not claim all keys analog or USB depth support. Owner confirms failed tester attempts for MCHOSE Ace 68 and MADLIONS TITAN 68 Turbo: sheet now red No usable analog found. All four frozen entries (including the owner's live MG75 V2 edit) now Research frozen; tester needed. Historical static candidates and the Ace PID-mismatch log remain evidence, not erased. See [per-model audit](KEYBOARD_SHEET_MAGNETIC_AUDIT_2026-09-20.md). Six status cells changed; 266 models / 32 brands preserved, no runtime change.

> 2026-09-20 catalog expansion: Added 149 source-backed models and previous research outcomes; Main now A1:C298, 266 records / 32 brands / 31 spacers. All 117 existing records preserved. IO Type 84 is red No usable analog found (FW 1.17), scoped to the reviewed deepest-key route, not a permanent impossibility claim. IO Type 68 and other unfinished investigations use Research incomplete; disabled frozen research uses Research frozen. Both are neutral gray. No cell source notes. See [catalog expansion and provenance](KEYBOARD_SHEET_EXPANSION_2026-09-20.md). Worldwide completeness is not yet established; no runtime support added.

> 2026-09-20 sheet refinement: Owner requires Not investigated (replaces Not assessed) and no source notes in sheet cells. Official source URLs remain in local catalog documentation. Added ATTACK SHARK AK029 (AJAZZ collaboration), M36 HE and R86 HE, all Not investigated. Existing 28 native-profile model statuses retained; catalog block now 31 entries. Main A1:C132 contains 117 records. See [ATTACK SHARK sheet catalog](ATTACK_SHARK_SHEET_CATALOG_2026-09-20.md) for sources and retail/driver naming discrepancies. No runtime change.

> 2026-09-20 owner catalog direction: The Google Sheet is to cover ALL existing magnetic keyboards across all brands, including devices not supported by HallJoy. ATK expanded from Hex80 to 23 official-source models; 22 additions use neutral gray Not assessed, not Support impossible. Main now A1:C129, 114 records, same 15 brands and 14 spacer rows. Source URLs are model-cell notes. Existing support statuses preserved. See [ATK sheet catalog](ATK_SHEET_CATALOG_2026-09-20.md) for sources, scope and verification. No runtime or README compatibility expansion.

> 2026-09-20 sheet follow-up: Main now spans A1:C107 with 14 blank spacer rows between the 15 bordered brand blocks, preserving all 92 records. Scripts must skip rows without a model. QBZ75 corrected to Supported (C56, green): IPI_LAYOUTS_2026-09-14.md already records physical evidence for the QBZ75-compatible addressed route. The later integration audit reports no NEW physical tests, not absence of all earlier evidence. Do not assign the whole IPI family an untested status; other models remain individually untested. Current hardware reference corrected. API readback confirms original data preserved except this status, intact borders, conditional colors and duplicate-brand visibility. Backup: .local/backups/google-sheet-before-brand-gaps-20260920.json.

> 2026-09-20 Google Sheets styling: Canonical workbook 1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c, Main sheetId 0, A1:C93. Owner requested brand blocks and status colors. Applied 15 outer block borders, dark frozen header, fitted widths, 32px rows, no merges or blank separators. All 92 records remain unchanged. Brand values remain in every row for scripts; a conditional white-font rule hides repeated adjacent brands visually. Four status color rules and strict status dropdowns extend through row 1000: green supported, yellow implemented/untested, lime supported/awaiting tester (including X65 Pro HE retest), red Support impossible (no existing rows). API readback verified unchanged values and effective colors/borders. No browser automation used. Borders are static and must be regenerated from contiguous brand groups after structural edits or sorting; colors/duplicate visibility are dynamic. Before-state and applied batch are preserved in .local/backups/google-sheet-*-20260920.json. Future scripts must read raw values, not infer missing brands from displayed appearance.

> 2026-09-20 owner roadmap: Automatic game-based profile switching is useful and deferred until after the 1.6 release; do not implement it before release. Created a plain three-column supported-keyboard workbook (Brand, Model, Support status), sorted by brand/model, with 92 model rows across 15 brands. Includes implemented untested routes with explicit statuses; excludes disabled research and merges regional/revision duplicates. Sources: SUPPORTED_HARDWARE.md and the existing 37-profile ATTACK SHARK manifest. Draft: outputs/01a09a0a-9d81-7661-8744-5bfb49f8d8e0/HallJoy_supported_keyboards_draft.xlsx. Workbook content checked after export; app/runtime unchanged.

> 2026-09-20 owner README refinement: Remove the selective wired-USB-only sentence for ATTACK SHARK and AULA MINI 60 HE Pro from the user-facing README. Keep actual connection limitations in technical compatibility records; this editorial change does not enable wireless support or establish a general battery/protocol claim.

> 2026-09-20 owner README refinement: List only ATTACK SHARK X65 Pro HE, X68 Pro HE and X82 Pro HE by name in the concise README table; describe other compatible models as expected to work but untested. This is a presentation change, not new hardware confirmation or a change to the 37 admitted profiles.

> 2026-09-20 owner README correction: Keep one concise Brand/Models compatibility table, no Notes column, regional splits, release-version commentary or separate experimental/catalog section. MG75 Pro and GravaStar belong in that table without experimental name suffixes. Owner confirms GravaStar support was reported working by a user; exact tested revision was not restated. Preserve detailed per-revision evidence and runtime notices outside the README. Redragon exact implemented profiles are K673RGB-M BR/UK and K673WB-RGB-M US; only BR has recorded physical evidence. Other compatible magnetic models may work but are untested.

> 2026-09-20: [1.6.0 release readiness](RELEASE_1.6.0_READINESS_2026-09-20.md). README, hardware status and documentation index reconciled with current integration records. Previous hardware/index snapshots preserved under docs/archive. Candidate awaits owner EXE review before GitHub publication; no new device research or runtime behavior changes in this documentation pass. Owner approved naming this accumulated release 1.6; source version is now 1.6.0. Historical 1.5.4 audit labels remain unchanged.

> 2026-09-20 owner testing policy: Do not run speculative long-duration stability/soak tests without a concrete failure hypothesis or user report. The proposed30–60 minute soak is declined and must not be started. Routine checks should be targeted and short (up to10 minutes is sufficient for the proposed generic check); elapsed duration alone does not prove stability. Investigate longer-running failures when concrete evidence arrives. No generic soak is requested now.

> 2026-09-20: [Background work review](BACKGROUND_WORK_REVIEW_2026-09-20.md). Owner emphasizes regression avoidance. Reused the current UI telemetry snapshot for diagnostic hashing, eliminating a duplicate collection on Configuration/Tester; input polling/timers/recovery unchanged. Tests and ordinary EXE delivered. Short process samples are not controlled idle/Pause or leak proof.

> 2026-09-20 owner clarification: Enable logging is OPTIONAL continuous logging, not a ban on automatic reports. Crashes, missing keyboards and recognized special failures MUST log even when OFF. Preserve this policy. [Logging review](LOGGING_PRIVACY_REVIEW_2026-09-20.md): excluded raw stack/register memory from ordinary crash reports; support writer privacy/burst tests pass. Detailed diagnostic crash dumps have a separate privacy scope. Ordinary EXE delivered.

> 2026-09-20: [Automatic layout review](AUTOMATIC_LAYOUT_REVIEW_2026-09-20.md): no new production defect found. Expanded simulator coverage for freshness boundary, host failure,32 unplug/replug cycles and manual-mode isolation passed, alongside existing coherence/multiple-device/remap tests. Ordinary EXE and user settings unchanged.

> 2026-09-20: [Profile persistence review](PROFILE_PERSISTENCE_REVIEW_2026-09-20.md): isolated production-linked save/load/migration/failure tests and18 startup scenarios PASS. Added orphan temporary-file restart cases and simulator-only overlay test diagnostics. No confirmed persistence defect; initial overlay test failure did not recur and remains unexplained. User settings/runtime EXE unchanged.

> 2026-09-20: [Shared input lifecycle review](INPUT_LIFECYCLE_REVIEW_2026-09-20.md). Owner confirms Block Bound Keys works locally; no blocking-policy defect or Forza root cause established. Fixed confirmed Pause/failed-Resume reset-before-realtime-join race; tests and ordinary EXE delivered. Block behavior/analog scale unchanged. ATTACK SHARK research remains paused.

> 2026-09-20 owner decision: Pause further ATTACK SHARK family investigation and development until a tester is available. Preserve current support, layouts, experimental notices and research; do not disable existing implementations or change scaling. The X68 MAX per-bank scaling risk and X82 physical endpoint remain unresolved as documented in ATTACK_SHARK_BANK_INIT_2026-09-20.md. Do not resume the earlier proposed firmware investigation without new tester availability or an explicit owner instruction.

> 2026-09-20: [ATTACK SHARK bank initialization](ATTACK_SHARK_BANK_INIT_2026-09-20.md) supersedes the unresolved selector/startup statements below. X82 v503 application scatter initializes seven identical730-ceiling tables. X68 MAX v504 FC switch bytes map to banks0/1/2/3/5/6; banks1–3 cap350, banks5/6 are anomalous. Component tests PASS; physical endpoints/full scanner lifecycle are not proved. No blanket scaling change, no new EXE, no remap readback.

> 2026-09-20: [ATTACK SHARK depth-range audit](ATTACK_SHARK_DEPTH_RANGE_2026-09-20.md): X65 HE v309 six lookup banks saturate at350; X68 MAX v504 bank0 at700 but banks1–4 at350 (active selection unresolved).22,550 real lookup-block cases PASS. X82 Pro v503 uses RAM-backed tables; endpoint unresolved. Do not replace full travel with UI actuation limits or claim all37 endpoints verified. No EXE/scaling change; remap readback remains deferred.

> 2026-09-20: [ATTACK SHARK family layouts](ATTACK_SHARK_FAMILY_LAYOUTS_2026-09-20.md): all37 admitted revisions now have exact automatic ANSI layout selection;16 visible groups including numpads, factory-correct Beat75 Right Ctrl, legacy Pro aliases preserved. Owner explicitly deferred remap readback. Overall catalog121 source/98 visible, production tests and ordinary EXE delivered. Supersedes manual-layout limitation in the previous entry.

> 2026-09-20: [ATTACK SHARK family enabled](ATTACK_SHARK_FAMILY_SUPPORT_2026-09-20.md): owner approved all reviewed RY5088 revisions; 37 exact profiles now active (31 added), individual factory/Fn maps, all 128 positions, page3 polling and bounded slow-transport freshness. Orange testing notices; unknown IDs/receivers excluded; new geometry remains manual pending review. Ordinary EXE delivered with all-slot linked tests. Supersedes the research-only entry below.

# Постоянный контекст владельца

> 2026-09-20: [ATTACK SHARK family evidence](../research/ATTACK_SHARK_FAMILY_2026-09-20.md): 37 exact manufacturer-listed magnetic revisions share RY5088 class; all queried, X65 v309 / X68 MAX v504 / X82 Pro v503 available. X68 MAX independent depth verified in component emulation. Exact-target firmware is not mandatory for explicitly experimental family support; retain exact identities/maps and amber notices. No additional models enabled by this research.

> 2026-09-19: [GravaStar layouts](GRAVASTAR_LAYOUTS_2026-09-19.md): V75/Pro/Lite legacy exact profiles now have shared 79-key ANSI geometry, individual automatic identities and session remaps. WIN60 MAX token loss on map refresh corrected. X68 HE non-Pro firmware recheck still returns no records for 2270/2472/2902; do not substitute Pro or X65 evidence.

> 2026-09-19: [O3C layout/config reader](SAYO_O3C_CONFIG_2026-09-19.md): firmware-proven read-only base assignments, Z/X/C factory geometry, automatic/manual separation. Digital mapping inference and residual synthetic depth removed. Owner correction: automatic O3C always retains three independent physical keys; known assignments supply labels, missing/complex assignments show Key 1/2/3. Manual layout uses Z/X/C. New config path awaits hardware validation.

> 2026-09-19: [K673 BR resolved](REDRAGON_K673_BR_LAYOUT_2026-09-19.md): exact manufacturer ABNT2 image confirms a wide /? key, not Right Shift. ABNT2 preset and verified-product selection added; native analog unchanged. Earlier BR deferral is superseded.

> 2026-09-19: [Razer regional layouts](RAZER_REGIONAL_LAYOUTS_2026-09-19.md): JIS for Huntsman V2 Analog / V3 Pro / V3 Pro Tenkeyless and ISO for V3 Pro. Exact source review; manual region selection; native analog unchanged. Other regional variants remain pending geometry evidence.

> 2026-09-19 owner direction: X65 Pro diagnostic has already been sent; wait for tester results. MG75 V2 firmware is available, but compatibility remains undetermined; defer further reverse engineering until a tester is available. Owner then clarified: remove the MG75 V2 mention from README entirely; keep the waiting decision only in internal context. Next approved layout batch: Razer ISO/JIS.

> 2026-09-19: [MG75 Pro native integration](MG75_PRO_INTEGRATION_2026-09-19.md): owner approved ordinary-build support without a tester. Exact SparkLink V1 backend, independent depth for 81 keys including Fn, automatic layout/base assignments, fixed vendor 3.5 mm range and orange Discord testing notice. Hardware validation remains pending.

> 2026-09-19: [MG75 Pro/V2 firmware review](MG75_PRO_V2_REVIEW_2026-09-19.md) supersedes the earlier Pro family inference: SparkLink V1 SDK and Pro firmware both provide independent 6x21 travel reads; runtime integration remains pending. MG75 V2 v1.21 normal monitoring is firmware-proven winner-only; alternative independent read remains unconfirmed.

> 2026-09-19: [MG75 Fn review](MG75_FN_REVIEW_2026-09-19.md): Max uses JingTai V2 row reads and native Fn action F101; fixed through extended publication. Pro uses separate JingTai V1 5C framing; do not describe it as a confirmed SparkLink device. Layout retained, hardware support unconfirmed.

> 2026-09-19: [HERO84 integration and map audit](HERO84_INTEGRATION_2026-09-19.md): exact UUID automatic layout, complete live/base map split, typed modifiers and physical Fn analog. Earlier manual-only/no-Fn statements are superseded; experimental warning and observed-range scaling remain.

> 2026-09-19: [Additional AULA layouts](AULA_ADDITIONAL_LAYOUTS_2026-09-19.md): HERO84 manual ANSI and KP-TE153 vendor UK/ISO added. HERO84 source has 84 positions; omitted apostrophe position 53 corrected in native polling. Experimental status and unsupported Fn assignments remain. Catalog now 97 source / 79 visible variants.

> 2026-09-19 owner correction: [Build/replacement lifecycle](BUILD_REPLACEMENT_LIFECYCLE_2026-09-19.md) supersedes the earlier broad shutdown rule. Leave HallJoy open during investigation, compilation and file-only tests. Use tools/build_release.ps1 for ordinary delivery: stage/validate first, then close only the exact replacement target, atomically install and reopen only if previously running. Unchanged or failed candidates do not close the app. No global MSBuild shutdown hook.

> 2026-09-19: [Complete catalog audit](LAYOUT_CATALOG_AUDIT_2026-09-19.md). Owner clarified no cross-brand layout merges. 95 source variants resolve to 77 built-in visible variants after exact Razer, MADLIONS and IPI within-brand merges. All old names, edited legacy precedence and automatic selection tested; README links the compiled catalog table.

> 2026-09-19: [Hidden controls and build shutdown](HIDDEN_CONTROLS_AND_WOOTING_MERGE_2026-09-19.md). Owner requests automatic project HallJoy closure before builds/tests; implemented in shared script and MSBuild. Ordinary 60HE v2 shares the 60HE / 60HE+ group; Split stays separate. Configuration visibility regression fixed at redraw synchronization.

> 2026-09-19: [Wooting extended layouts](WOOTING_EXTENDED_LAYOUTS_2026-09-19.md): five 60HE v2 / split / UwU presets added. Owner approved independent physical split analog channels. Ordinary EXE delivered; earlier layout deferrals superseded. Regional/split selection remains manual; hardware validation pending.

> 2026-09-19: [X65 Pro input-path diagnostic](INPUT_PATH_DIAGNOSTIC_2026-09-19.md). Block OFF/ON gives identical real backend reports in isolated tests; green circles are digital indicators. No confirmed Forza root cause. Focused diagnostic EXE automatically writes one aggregate log, without changing the saved logging preference or adding input emulation.

> 2026-09-15: [HERO84 enabled with testing notice](HERO84_ENABLED_UNVERIFIED_2026-09-15.md). Owner explicitly enabled its retained analog backend in ordinary HallJoy, like NA87. Earlier HERO84 disabled-build requirements are superseded; other frozen protocols stay disabled.


> 2026-09-15: [Frozen support notices](FROZEN_SUPPORT_NOTICES_2026-09-15.md): NA87 remains enabled but frozen pending tester logs; amber warning. Other frozen protocols stay disabled by owner decision.


> 2026-09-14: [preview stability fix](AUTOMATIC_LAYOUT_COHERENCE_2026-09-14.md):
> failed seqlock reads are unavailable evidence, not device removal. Coherent
> metadata publication and source-count checks prevent false layout switching.
> Status-only changes no longer rebuild keyboard controls or cancel dragging.

> 2026-09-14: [UAP automatic-layout fix](AUTOMATIC_LAYOUT_UAP_FIX_2026-09-14.md):
> DuplicateSafeId denotes a collision-resistant device ID, not multiple devices.
> Removing its incorrect rejection fixes a general UAP selection defect.
> UAP matrix telemetry now uses the shared reviewed Keychron identity table.

> 2026-09-14: [Automatic layout](AUTOMATIC_LAYOUT_2026-09-14.md): enabled by default;
> exact match locks layout choice, ambiguity/failure restores manual choice.
> NA87/IPI session remaps apply only in successful automatic mode. Manual mode
> uses factory assignments. This supersedes the old first-run-only policy.

> 2026-09-14: [ATK Hex80 native fixes](ATK_HEX80_NATIVE_FIXES_2026-09-14.md).
> Corrected19 matrix slots;87 factory keys including Fn now publish analog.
> Supports32/128-byte payloads, per-key freshness and correlated chunk replies.
> Historical82-key table/report-size assumptions below are superseded.

> 2026-09-14 owner clarification after IPI delivery: lack of local hardware is
> not a blocker for continuing support/layout work. Complete source/firmware
> analysis and software checks, record evidence limits, and proceed. Users will
> report physical-device issues; do not repeatedly stop for hardware confirmation.

> 2026-09-14 implementation update: [IPI native support](IPI_NATIVE_SUPPORT_2026-09-14.md).
> Exact UUID profiles, complete live maps, device calibration, Fn and alias
> publication now replace the historical IPI fallback. Eight models/four layouts.
> Earlier layout-only/no-EXE statements below describe the preceding step.
> Physical USB tests and AURORA65W receiver forwarding remain unverified.

> 2026-09-14: [IPI firmware reverse](IPI_FIRMWARE_REVERSE_2026-09-14.md) now covers18 images/eight UUIDs.
> Addressed analog handlers and exact physical-ID sets confirmed in firmware;
> 216 synthetic handler/helper cases PASS. Empty-map request and calibration
> policy gaps remain in HallJoy. No hardware test or EXE change in this step.

> 2026-09-14 follow-up: [IPI layouts](IPI_LAYOUTS_2026-09-14.md): four merged ANSI presets for
> eight models. Official demo defaults resolve the previously missing labels;
> the separate Addressed fallback discrepancy remains documented at its table.
> O3C and Plus revisions remain excluded. No new analog routes enabled.

> 2026-09-14: [добавлена ATK Hex80 ANSI](REMAINING_LAYOUTS_2026-09-14.md), 87 отображаемых клавиш.
> O3C исключён владельцем; другие кандидаты требуют отдельного разбора и отложены.
> Только раскладка: текущая Hex80-таблица аналоговых слотов отличается от официального
> профиля, протокол не изменён. Проверки/сборка PASS; основной HallJoy.exe обновлён.

> 2026-09-14: [устранена дорогая запись настроек при смене раскладки](SETTINGS_SAVE_LATENCY_2026-09-14.md).
> Полное сохранение настроек/привязок пакетное: 1117 мс → 17–27 мс в изолированном тесте.
> Полный набор тестов профилей и событий редактора PASS. По прямому запросу владельца
> работающий HallJoy закрыт, основной EXE заменён; папка Optimized удалена.
> Для обновлений использовать прежний путь EXE; владелец разрешил закрывать HallJoy
> для замены сборки, не создавать дополнительные папки выдачи.
> Отдельные K2/K3 — JIS; старый Q1 ANSI сохранён как override с UniformGap=6.

> 2026-09-14: [переработано хранение раскладок](LAYOUT_STORAGE_OPTIMIZATION_2026-09-14.md).
> Заводские раскладки остаются в памяти; при запуске INI не создаются и не переписываются.
> Пакетное чтение/запись, атомарное сохранение и старые пользовательские правки сохранены.
> Изолированный замер каталога: чистый запуск 67 с → 8 мс; 63 старых файла 1,31 с → 34 мс.
> Это время каталога, не всего приложения. Проверки и новая сборка EXE PASS.

> 2026-09-14: добавлены [четыре раскладки MADLIONS](MADLIONS_LAYOUTS.md) по запросу владельца:
> MAD60HE, MAD68HE, MAD68R (поддерживаемая ревизия10A7) и MAD 68 Pro R, ANSI.
> Проверки и сборка PASS; выбор вручную в MADLIONS. Визуал оценивает владелец.

> 2026-09-13: по новому запросу владельца добавлены [IROK MG75 Max / Pro ANSI](IROK_MG75_LAYOUTS.md),
> по81 клавише, выбор в каталоге IROK. Проверки/сборка пройдены; визуал оценивает владелец.

> Актуально: [первая сборка с реальным аналоговым вводом NA87](IROK_NA87_NATIVE_SUPPORT_2026-09-13.md).
> Поток глубин подтверждён аппаратным логом; новый backend публикует его в HallJoy.
> ANSI подтверждена владельцем. Прежний сборщик заменён непрерывным вводом,
> логирование сокращено до агрегатов. Автотесты пройдены; нужен аппаратный прогон
> нового EXE для проверки сочетаний и отпусканий.

> После локального лога: [исправления runtime](IROK_NA87_RUNTIME_FIXES_2026-09-13.md).
> Устранён дефект владения Windows debug handles, ограничены повторные логи,
> остановлен цикл перезапусков при невалидном command event. Автотесты пройдены;
> Повторный запуск16:08 завершился штатно: прежние сбои не повторились.
> Текущий EXE можно передавать NA87-тестировщику; аналог NA87 ещё не подтверждён.

> Правило языка владельца: интерфейс HallJoy, все сообщения диагностического EXE,
> логи и комментарии в коде — только на английском. Общение с владельцем — на русском.
> Язык общения не переносить в приложение.

> Актуально: [обычный HallJoy.exe с автоматической диагностикой NA87](IROK_NA87_HALLJOY_DIAGNOSTIC_2026-09-13.md).
> Прежний ZIP отклонён. Передавать только EXE: обычный геймпад сохранён,
> пользователь нажимает клавиши30–60 секунд, закрывает окно и присылает HallJoy.log.
> На локальном ПК Keychron; NA87 здесь нет. Работающий HallJoy владельца не закрывать.
> Полный тест instance guard/профилей здесь блокируется уже запущенным HallJoy
> (Windows error5); остальные доступные проверки и самотесты диагностики пройдены.

> Продолжение после ревью: [исправления ND75](../research/IROK_ND75_IMPLEMENTATION_FIXES_2026-09-13.md).
> Wire format и отбрасывание некорректной глубины исправлены; полнота состояния
> при потере событий остаётся нерешённой. Итоги проверок — в новом документе.


> Актуальное уточнение 2026-09-13: [ревью ND75](../research/IROK_ND75_REVIEW_2026-09-13.md).
> Найдены ошибки wire opcode и публикации состояния в experimental backend;
> исправлена одна serializer fixture. Прежний вывод об отсутствии любых
> альтернативных чтений слишком широк: сохранён частичный результат52/81.
> Pro заморожен; эксперименты вне штатной таблицы настроек приостановлены
> по последнему согласованному направлению, продолжается обычное ревью.


Этот документ читается в начале новой сессии через корневой AGENTS.md.
Здесь фиксируются уточнения владельца, которые нельзя каждый раз заново
превращать в вопросы или блокеры. Новые решения добавлять с датой и областью
применения; при реальном противоречии сначала изучить данные, затем объяснить его.

## 2026-09-09 — Keychron HE: кастомные прошивки под UAP

Владелец подтвердил: используются кастомные прошивки. Уточнение 2026-09-12:
это не готовый инструмент-патчер. Keychron публикует исходники, а AnalogSense
показывает изменения full analog report, которые переносятся в исходники
нужной модели с последующей сборкой. Для обсуждаемых Keychron HE поддержка
аналога считается обеспеченной этим путём.

Контекст обсуждения: расширение каталога раскладок K4 HE, Q1 HE, Q3 HE, Q5 HE,
K2 HE и соответствующих вариантов ANSI/ISO/JIS. При добавлении их раскладок
не ставить повторное исследование аналогового протокола или наличие всех этих
физических клавиатур условием работы. Не предлагать снова проверять, «есть ли
вообще аналог», ссылаясь только на ограничения заводской прошивки.

Что действительно требуется проверять:

- соответствие модели и варианта ANSI/ISO/JIS;
- правильные VID/PID, интерфейс и матрицу для идентификации;
- соответствие координат, размеров и назначений клавиш;
- запись в каталоге автоподбора без неоднозначного выбора устройства.

Границы факта: это подтверждённая владельцем исходная информация, а не новый
аппаратный тест агента. Она не означает, что все стоковые прошивки дают хороший
аналог, что все будущие модели автоматически внесены в код, или что можно
прошивать устройство образом другой модели. Источник: https://analogsense.org/firmware/.
Проверенный коммит AnalogSense/qmk_firmware:
e21916b695ec3b31fbbbb325e32849be5cdb9e46 (keychron-far, Add full analog report).
Изменены analog_matrix.c (команда полного отчёта 0x31 и маркер 0x45) и
keychron_raw_hid.c (суффикс +far). Это изменения исходников, не бинарный патчер.
Не блокировать добавление раскладок повторным исследованием этого протокола.

## Уже принятые решения по раскладкам

- Импортированные координаты могут быть точнее ручных. Старая K4 — ориентир,
  не эталон для подгонки.
- Новая импортированная K4 одобрена владельцем. Старый пресет K4 снят с каталога;
  старые сохранённые ссылки перенаправляются на новую раскладку.
- Автовыбор только при первом запуске после завершения поиска, при одном
  подходящем устройстве и точном соответствии. Ручной/сохранённый выбор сохранять.
- Подробности: `LAYOUT_IMPORT.md` и `FIRST_RUN_LAYOUT.md` в этой папке.

## 2026-09-09 — Основное окно не управляется клавиатурой

Игровые нажатия не должны открывать списки, нажимать кнопки, переключать вкладки,
двигать ползунки или запускать встроенные сочетания команд. Разрешены только
явно назначенные пользователем бинды (например Block Bound Keys), режим их
захвата и ввод текста/чисел в поля, которые пользователь открыл мышью.
Последнее отдельно подтверждено владельцем. Самостоятельное окно редактора
раскладок не входит в эту задачу: его точное клавиатурное редактирование сохраняется.
Подробности: `MAIN_WINDOW_KEYBOARD_POLICY.md`.

## 2026-09-09 — Охват раскладок остальных брендов

Владелец явно подтвердил: добавляем раскладки для поддерживаемых устройств,
включая модели встроенного UAP, а не только физически проверенные командой.
Это не разрешение добавлять все модели бренда по сходству названия или VID.
Экспериментальные и замороженные ROG Azoth 96 HE, AULA HERO84 HE, IROK ND75,
Attack Shark X68 HE в этот этап не входят. Не переспрашивать, включать ли UAP.

Добавлять строго по одному бренду за этап, затем останавливаться на проверку
владельцем. Не выкатывать несколько новых брендов одним пакетом. Следующий
после Keychron этап: Lemokey P1 HE ANSI/ISO, явно перечисленные во встроенном UAP.

Lemokey ANSI/ISO принят владельцем. Текущий следующий бренд — DrunkDeer.
UK/FR/DE не являются отдельными категориями раскладок: использовать английские
подписи и различать физическую геометрию. Для A75/Pro/ISO исследован запрос
модели официального Antler; не считать одинаковый VID/PID неразрешимой
неоднозначностью. См. `DRUNKDEER_LAYOUT_IDENTIFICATION.md`.

Для расхождения четырёх навигационных клавиш G65 владелец ответил:
«доверяем официальному драйверу и его позициям». Использовать Antler offsets
35/56/77/98 для Delete/End/Page Up/Page Down вместо ранее предположенных
36/57/78/99. Повторно требовать пользователя с G65 для этого решения не нужно.
Остальные измеренные позиции сохранять. Это выбор источника, а не новый
физический тест; не распространять автоматически на противоречия внутри Antler.

Этап DrunkDeer реализован и собран: семь раскладок, подтверждённая запросом
модель, автоподбор первого запуска и модельные карты UAP. Текущий результат:
`DRUNKDEER_LAYOUTS.md`, EXE `F9FF13072B96C85FC0078EB5B0A45B98D9BAE6599FF14844E75914102213680A`.
Ждём оценки владельцем этого бренда, следующий бренд пока не начинать.

## 2026-09-09 — Экономный конвейер добавления раскладок

Позднее владелец разрешил подготовить общую архитектуру импорта на примере Aula.
Новая работа начинается с `LAYOUT_PIPELINE.md` и команды
`py tools/layout_pipeline.py summary <Brand>`, а не с чтения огромных UI-файлов
или повторного исследования уже поддерживаемого протокола. Источники и решения
сохранять; новое семейство драйвера требует адаптера, модель в известном семействе
добавляется данными. Экономия контекста не отменяет проверок и точной геометрии.
Aula Standard WIN60/WIN68 и WIN60 MAX встроены через общий генератор;
подтверждённая идентификация передаётся из нативной сессии в first-run.
Подробности: `AULA_LAYOUT_PIPELINE.md`. После проверки этого бренда владельцем
выбирать следующий; HERO84 остаётся замороженной.
Не выдавать подготовленные файлы за выпущенную поддержку/аппаратный тест.

Следующий разрешённый этап после Aula — Redragon: K673WB-RGB-M ANSI и
K673RGB-M ISO. См. `REDRAGON_LAYOUTS.md`. BR-карта требует отдельного решения:
официальный источник и существующий backend публикуют IntlRo вместо правого
Shift. Не менять работающий аналоговый протокол и не подставлять UK-идентичность.
Этот этап не даёт разрешения выкатывать следующий бренд без проверки владельцем.

## 2026-09-10 — Завершающий пакет раскладок перед релизом

Владелец принял обе раскладки Redragon и явно разрешил одним пакетом добавить
Razer, NuPhy и Wooting, затем остановить расширение каталога ради выпуска версии.
Это исключение из прежнего правила «один бренд за этап». Не начинать остальные
бренды после этого пакета. Непроверенные геометрии не заменять похожими.

## 2026-09-10 — Объединение раскладок и подготовка релиза

Владелец разрешил объединить проверенные одинаковые раскладки и выполнить всю
локальную подготовку релиза, затем сообщить, что осталось ему. Объединены только
13 пар из LAYOUT_DUPLICATION_REVIEW.md; изменённые пользователем старые модели
сохраняются отдельно. Политику протоколов и распознавания не менять. Кандидат
готовится как 1.5.0; публикация на GitHub и подпись требуют отдельного решения.
Актуальное состояние подготовки: RELEASE_PREPARATION_2026-09-10.md.

## 2026-09-10 — Пользовательская документация релиза

README и патчноут для GitHub остаются английскими. Не писать о «команде HallJoy»
и ежедневном использовании K4 автором. Таблицы должны помогать выбрать устройство,
а не объяснять внутренние протоколы: сохранить полезные ограничения MG75 v2 и O3C.
У Redragon отделять подтверждённую работу от ожидаемой, но не протестированной;
пока подтверждение есть для K673RGB-M BR, не объявлять остальные проверенными.
Веб-драйверы часто конфликтуют; закрытие десктопного ПО вроде Synapse не обязательно,
если всё работает. Не повторять инструкции обращения за поддержкой в нескольких
разделах: использовать один раздел и шаблон. В патчноуте явно отметить появление
Discord-сообщества. Эти уточнения не меняют поддержку устройств или код программы.

## 2026-09-10 — Отзывы перед релизом 1.5.0

Владелец сообщил: последнюю сборку отправил четырём пользователям, все сообщили
о нормальной работе без жалоб. Это пользовательская проверка, а не утверждение
о тестировании всех моделей клавиатур. Запрошена финальная проверка кандидата
и текущего GitHub перед релизом. Проверенный EXE не менять без причины;
публикацию, коммит и тег не считать уже выполненными. Результат проверки:
FINAL_RELEASE_AUDIT_2026-09-10.md.

Владелец разрешил загрузить текущие исходники и выпустить релиз на GitHub.
Уведомление о 100% Actions storage требует экономного хранения артефактов;
не повышать платные бюджеты и не удалять чужие/старые данные без разрешения.
Автоматические CI-проверки сохраняются; архив Actions только при ручном запуске,
срок хранения 3 дня. Пользовательские релизы размещаются в GitHub Releases.

Релиз v1.5.0 опубликован: https://github.com/PashOK7/HallJoy/releases/tag/v1.5.0.
Обе CI-проверки успешны; тег указывает на 31470d22db96111095e5625a5707f44bfcd053c3.
Проверенный пользователями EXE не заменён. Итог: GITHUB_PUBLICATION_2026-09-10.md.

## 2026-09-10 — Один EXE для пользователя

Пользовательский релиз распространяется одним HallJoy.exe, без обязательного
архива и сопутствующих файлов. Владелец явно потребовал убрать ZIP из v1.5.0.
Публиковать проверенный EXE напрямую; архивы сборки остаются внутренними.
SHA-256 EXE: F6CF016FA3D8B15D80EB2AF83BCAE1D8E16F989FE142EBC92D96CA454232079B.

Уточнение владельца: отдельные лицензионные файлы в Assets разрешены и нужны;
ограничение касается запуска — пользователь скачивает только HallJoy.exe.
Публиковать THIRD_PARTY_NOTICES.md и LICENSE как сопроводительную документацию,
с явным объяснением в релизе, что класть их рядом с EXE для запуска не требуется.

## 2026-09-11 — Ошибка профиля не должна закрывать HallJoy

Владелец столкнулся с отказом запуска 1.5.0 на другом ПК со старыми настройками.
Разрешено автоматическое восстановление: сохранить читаемые части и проверить
бекапы, затем сохранить оригиналы и запуститься со сбросом невосстановимого.
Не требовать ручного удаления INI. При невозможности безопасной записи продолжать
в памяти без автосохранения, не уничтожать оригиналы. Не сбрасывать чужие профили
и раскладки. Подробности: STARTUP_PROFILE_RECOVERY_2026-09-11.md.

Владелец подтвердил запуск исправленного EXE на проблемном ПК и разрешил
публикацию 1.5.1. Выпускать именно проверенный EXE с SHA-256
4BA1CD93D1F1BED775B67DB9B65DBB9EE294F03D47AFE6EC948147303DF776C4.

1.5.1 опубликован как Latest: https://github.com/PashOK7/HallJoy/releases/tag/v1.5.1.
Windows/Linux CI успешно прошли на e72cca95d1e1d4cb091768889e8aa4eb4645b037.

## 2026-09-12 — README и единый стиль релизов

README — краткая пользовательская инструкция, не технический отчёт. Не добавлять
в начало текущую версию, ссылки на патчноут/внутренние документы или обещания
работы с прошивками от имени владельца без согласования. Владелец удалил
верхнюю приписку на GitHub; не возвращать её из старой локальной копии.
Таблица совместимости должна быть понятна сама по себе, без extended compatibility
list и дублирующей страницы. Сохранить полезные ограничения и различие между
проверенными и предполагаемо совместимыми устройствами. Experimental firmware
route удалён как несогласованное обещание. Технические документы не удаляются.
Названия релизов: «v<версия>: <краткое описание>», без префикса HallJoy.
Правки оформления не меняют теги, EXE и исторические патчноуты.
Раздел Support располагается сразу после таблицы совместимости и её оговорки,
перед Input Overlay; текст раздела при переносе не меняется.

Владелец явно попросил ссылку на кастомные прошивки в строке Keychron:
https://analogsense.org/firmware/ — готовые образы, изменения исходников и
инструкции сборки. Это согласованная полезная ссылка, не лишняя техническая
страница. Указать возможность подготовки прошивки для других Keychron HE;
не выдавать чужой образ за подходящий для любой модели или ANSI/ISO/JIS.

## 2026-09-12 — Релиз 1.5.2

Владелец подтвердил исправления нумпадного бинда и задержки Block Bound Keys.
По его разрешению опубликован 1.5.2, только с коротким английским патчноутом,
без изменений README. Linux/Windows CI PASS; тег c2e5d287f439311b655afc577642b93e41192661.
Итог: RELEASE_1.5.2_2026-09-12.md. Старый нумпадный бинд рекомендуется назначить
заново один раз; в новой версии он не зависит от Num Lock.

## 2026-09-13 — Текущее исследование IROK/IYX, переход в новый чат

Для продолжения реверса NA87/ND75/MU68 и Pro сначала читать
`IROK_REVERSE_HANDOFF_2026-09-13.md` в этой папке. Там источники, локальные
бинарники, команды, точные результаты, ограничения и порядок дальнейшей работы.
Обычная NA87 ближе к ND75/Witmod, не к Pro/JingTai. В ND75 компонентной эмуляцией
найдена потеря событий (включая отпускание); в десяти Pro-образах найдены массивы
глубин. Это не выпущенная поддержка и не аппаратные тесты. Владелец просил
продолжать исследование без железа: полезная статическая работа ещё не исчерпана.


## 2026-09-13 — Продолжение IROK до внешнего блокера

Владелец попросил продолжать до блокера или выполнения работ перед реальной
клавиатурой. Итог в `../research/IROK_PRE_HARDWARE_STATUS_2026-09-13.md`.
Witmod SDK преобразует host0x29 в wire0x21; полный ND75 scanner теряет releases.
Pro: дескрипторы10 образов, карты87/68/64, serializers эмулированы, полный
producer NA871.0.8 проверен; queue теряет chunks при full/busy. Есть offline
parser и общий прогон11 scripts. Это не аппаратный PASS и не включение поддержки.
Дальнейший вывод об ordinary NA87/MU68 требует точного firmware/capture;
вывод о качестве Pro — аппаратных USB timing/input tests. Старое указание
«offline работа ещё не исчерпана» относилось к этапу до этого продолжения.


## 2026-09-13 — Новое решение: Pro заморожен, углублённое ревью ND75

Владелец попросил сохранить/заморозить NA87 Pro и копать дальше ND75/Witmod.
См. `IROK_PRO_FROZEN_2026-09-13.md`: проверенный ZIP+SHA256 manifest,71 файл.
Активно ревьюить код, эмуляцию, возможные альтернативные read paths и
обоснованность выводов об ordinary NA87. Не превращать прежний вывод
о блокере в запрет продолжать исследование; не продвигать Pro без нового решения.


## 2026-09-13 — Реальный тестировщик только NA87; один расширенный пакет

Владелец уточнил: тестировщика с ND75 нет, есть с обычной NA87.
Метаданных недостаточно: нужен один пакет, который перебирает доступные
стандартные способы обмена и проводит несколько аналоговых сценариев за один
запуск, без выдачи новой сборки под каждую гипотезу. Создаётся отдельный
инструмент tools/irok_na87_diagnostic; план и статус в его PLAN.md.
ND75 diagnostic не выдавать за NA87 test. Pro остаётся замороженным.


## 2026-09-17 — AULA MINI60 HE Pro diagnostic, completion by evidence

The owner requested one robust integrated HallJoy.exe for the MINI60 HE Pro tester,
and then questioned a fixed-duration run. Follow the current implementation/status in
`AULA_MINI60_DIAGNOSTIC_2026-09-17.md`: complete by observed presses, depth variation,
multiple held keys and releases; do not restore a fixed 40/60-second typing schedule.
Keep normal HallJoy/gamepad functionality, English UI/logs/comments, one returned
HallJoy.log, and the existing IrokNa87Diagnostic delivery directory. Wired 0C45:80A2
is the command target; 0C45:FEFE is the receiver and receives no speculative commands.
Software checks passed; the first real hardware diagnostic result is still pending.


## 2026-09-17 — AULA MINI60 HE Pro playable native support

Supersedes the previous pending-first-hardware-result statement. Tester logs 13
and 14 confirmed changing analog depth on firmware V1.52; log 14 confirms three
simultaneously held keys. The owner requested real gamepad use with continued
logging. See `AULA_MINI60_NATIVE_SUPPORT_2026-09-17.md` for the completed
experimental implementation, exact artifact and checks. Same existing delivery
folder and single HallJoy.log; ordinary HallJoy bindings/gamepad remain active.
No fixed diagnostic time window, no eight-key completion requirement for gameplay.
Wired only. Automatic layout reads supported assignments, manual uses factory map.
Per-position 50 ms freshness is an initial policy, not a hardware quality guarantee.
Software validation passed; first gameplay feedback remains pending.


## 2026-09-19 — NA87 and MINI60 ordinary support approved

The owner accepted the existing NA87 hold behavior after logs15/16 and requested
ordinary support for both NA87 and AULA MINI60 HE Pro. NA87 is no longer frozen;
remove its amber notice, preserve the other frozen models. Do not add arbitrary
NA87 expiry or require another tester confirmation as a release gate. This does
not establish the missing final depths from log16. Current implementation and
artifact: `NA87_MINI60_STANDARD_SUPPORT_2026-09-19.md`. Both native backends are
included by default; wired AULA only; one support log, no timed test or special
window title, same delivery path. Native AULA operation must not depend on logging.


## 2026-09-19 — ATTACK SHARK research, all three are Pro

Owner has heard from three users and requests further research of X65 Pro HE,
X68 Pro HE and X82 Pro HE. Explicit clarification: all Pro. Preserve exact
revision distinction; old non-Pro X65 reference is not their firmware. Newly
available exact X82 dev2935 v503 has been acquired and its table getter/scanner
publication blocks component-tested. See ../research/ATTACK_SHARK_PRO_REVIEW_2026-09-19.md.
No new diagnostic or production backend was delivered by this research step.


## 2026-09-19 — ATTACK SHARK integrated test build delivered

Owner currently has a tester only for X65 Pro HE and requested a test build.
Supersedes the research-only delivery status above. See
`ATTACK_SHARK_PRO_DIAGNOSTIC_2026-09-19.md` for the exact artifact and validation.
One HallJoy.exe in the existing IrokNa87Diagnostic directory; one adjacent log.
The reader recognizes six exact Pro revision/PID pairs but X65 hardware remains
untested. Page reads run continuously, completion depends on evidence, not time.
No stream/calibration/settings modes are enabled. ATTACK SHARK data is diagnostic
only in this build, not gameplay analog. Existing ordinary native support and
gamepad behavior remain enabled. All UI, logs and code comments stay English.


## 2026-09-19 — neutral delivery directory and ATTACK SHARK preflight

Owner explicitly objected to continued use of the IROK-named delivery folder.
Current delivery is build/bin/Release/x64/HallJoy.exe, with adjacent HallJoy.log;
this supersedes earlier instructions to keep the IrokNa87Diagnostic directory.
Existing artifacts and owner log were moved, not duplicated. The reviewed owner
log shows Keychron, zero eligible ATTACK SHARK collections, no Feature commands,
and clean shutdown. Revised shark-2 build improves rejection/failure evidence;
see ATTACK_SHARK_PRO_DIAGNOSTIC_2026-09-19.md. Hardware read validation is pending.


## 2026-09-19 — ordinary release candidate, owner test before publication

Owner requested GitHub publication of NA87 and AULA MINI60 HE Pro support, then
explicitly required testing the ordinary EXE first. This is a publication gate:
do not push release sources, create/publish a GitHub release or upload its assets
before the owner's confirmation after testing. Prepare a normal build with
ATTACK SHARK diagnostic disabled. Preserve the already-sent shark-3 tester EXE
in .local/backups/HallJoy-attackshark-shark3.exe. Delivery remains the neutral
build/bin/Release/x64/HallJoy.exe. Do not create an application ZIP.

Candidate1.5.3.0 is ready for owner testing; exact artifact and checks are in
RELEASE_1.5.3_CANDIDATE_2026-09-19.md. Publication remains gated on owner feedback.


Owner caught forced release logging despite Enable logging off. The ordinary
1.5.3 candidate now excludes unconditional diagnostic trace/support macros;
native NA87/AULA do not imply tester logging. Existing settings-aware support
logging and automatic incident reports remain. Candidate document records the
fixed artifact; publication still waits for owner testing.


## 2026-09-19 — publication authorized after owner check

Owner reported the corrected EXE looks fine and explicitly requested GitHub
publication. The test-before-publication gate is now satisfied. Publish1.5.3
with the tested ordinary EXE SHA256
5481aa57e84ae7378193ef559d530b20cd34f771f8904d615e3e39afbbd26621.
Keep ATTACK SHARK diagnostic disabled. Verify source and CI before publication;
ship HallJoy.exe with license notices, no application ZIP.


## 2026-09-19 — standing authorization to close HallJoy

Owner explicitly authorizes closing running HallJoy whenever needed for builds,
EXE replacement, diagnosis or automated tests, without asking again. Prefer
graceful window closure and wait for shutdown; if hung, terminate the verified
HallJoy process and its children. Do not create extra build directories merely
to avoid closing a running EXE. This permission persists across sessions.
The ordinary HallJoy and its children closed gracefully before local release
profile checks; no additional owner confirmation is required.


## 2026-09-19 — v1.5.3 published

Stable/latest GitHub release v1.5.3 is public, at source2d9cf1395aeaa3b2bf02966bbe1b55e5fa194186.
Both jobs in CI35444148705 passed. Published EXE matches the owner-tested
SHA2565481aa57e84ae7378193ef559d530b20cd34f771f8904d615e3e39afbbd26621.
See RELEASE_1.5.3_CANDIDATE_2026-09-19.md for final verification and history.


## 2026-09-19 — X65 Pro playable test iteration

After tester log17, owner requested the next EXE. Current local delivery is
shark-play-4 at build/bin/Release/x64/HallJoy.exe. Exact X65 Pro dev2308/USB0x0314
publishes gameplay analog with a fixed provisional350 raw endpoint, factory
key assignments, timestamped shared pages and150ms stale release. High-resolution
waits and page bursts improve the host polling plan; actual hardware rate awaits
tester evidence. Other Pro revisions remain diagnostic-only. One adjacent log,
no deadline. See ATTACK_SHARK_PRO_DIAGNOSTIC_2026-09-19.md for final hash/tests.
Published GitHub1.5.3 remains unchanged; its approved EXE is backed up at
.local/backups/HallJoy-v1.5.3-tested.exe. No new release publication authorized.


## 2026-09-19 — ATTACK SHARK family prerelease and Fn

Owner confirms X65 Pro works in a game, asks for prerelease EXE, Fn correction,
and inclusion of X68 Pro/X82 Pro. Local1.5.4.0 now includes normal native support
for all six known exact revision/PID profiles, plus factory analog Fn+F1-F12
routing where defined by each vendor map. No digital event is used to generate
depth or choose Fn. Logger is optional under ordinary settings; diagnostics are
not forced. Existing released1.5.3 is unchanged; do not publish without owner
approval. See ATTACK_SHARK_PRO_DIAGNOSTIC_2026-09-19.md final section for SHA,
tests and limitations (provisional3.5mm, Fn activation inferred from nonzero
physical travel, no custom remap/macros, X68/X82 not physically validated).
Delivery stays build/bin/Release/x64/HallJoy.exe. User should enable logging in
normal Settings when a support log is needed for the next test.


## 2026-09-19 — ATTACK SHARK Pro visual presets

Current 1.5.4.0 prerelease also includes X65/X68/X82 Pro HE ANSI visual layouts
and automatic selection by exact native revision while connected. Same delivery
path; no publication. Factory geometry is extracted from pinned official driver
SVGs, no runtime SVG parsing. Custom remaps are not read. Dev2901 vendor catalog
versus matrix discrepancy is documented in ATTACK_SHARK_PRO_DIAGNOSTIC_2026-09-19.md;
other revisions still require physical confirmation as previously recorded.


## 2026-09-19 — mandatory: no digital keyboard depth emulation

Owner reiterates a standing prohibition: production HallJoy must never invent
analog keyboard depth from digital key states, elapsed hold time or fallback
curves. An unavailable analog source must not silently become simulated travel.
Do not reintroduce this via hidden INI settings or compatibility modes.
Synthetic samples are permitted only in explicitly isolated automated tests;
they are not hardware support evidence. Real analog-to-virtual-gamepad conversion
remains the purpose of HallJoy. See DIGITAL_DEPTH_EMULATION_REMOVAL_2026-09-19.md.


## 2026-09-19 — README keyboard catalog refreshed

README now includes NA87/MINI60 HE Pro (1.5.3), all eight reviewed IPI models
and the expanded Keychron HE model catalog with the custom-firmware requirement.
ATTACK SHARK X65/X68/X82 Pro is explicitly marked as 1.5.4 prerelease, with X65
tester evidence and the unresolved Forza blocking report; other revisions are
not claimed physically validated. HERO84 is experimental, disabled frozen models
remain unavailable, and layout presence is not protocol evidence. Quick start
documents automatic/manual selection. Production digital depth emulation is
explicitly prohibited. English README only; no binary change or publication.


## 2026-09-19 — four remaining Razer ANSI layouts

Owner authorized the proposed Razer batch: V2 Analog, Mini Analog, V3 Pro,
V3 Pro Tenkeyless. Added exact-model manual-guide ANSI geometry (104/61/104/84).
TKL has no separate PrintScreen/ScrollLock/Pause; no generic 87-key substitution.
Manual selection only, no PID-to-ANSI guess. ISO/JIS need exact regional evidence.
See RAZER_FOUR_LAYOUTS_2026-09-19.md. Ordinary 1.5.4.0 EXE at the existing delivery
path; no publication. Source PDFs are offline research inputs, not runtime assets.
> 2026-09-20 additional catalog brands: Added 38 manufacturer-confirmed magnetic configurations from Arbiter Studio, Glorious, IQUNIX, LUMINKEY, Meletrix, Womier and YUNZII. Main now A1:C343, 304 records / 39 brands. All 266 existing records and statuses preserved. New entries are Not investigated, not support claims. See [sources and exclusions](KEYBOARD_SHEET_NEW_BRANDS_2026-09-20.md). API readback verified values, borders, repeated-brand hiding, gray status colors and validation; no cell notes or runtime changes.
