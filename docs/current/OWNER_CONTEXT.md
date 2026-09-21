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
