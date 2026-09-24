# HallJoy documentation

Start with [owner decisions](current/OWNER_CONTEXT.md), then the current documents
below. Dated audits preserve evidence, including superseded conclusions; they
are not all active requirements. Later owner decisions take precedence.

## Current release and user documentation

- [Published 1.6.2 release](current/RELEASE_1.6.2_PUBLICATION_2026-09-24.md).

- [Release notes](releases/README.md).
- [AULA experimental support and status synchronization](current/AULA_EXPERIMENTAL_SUPPORT_SYNC_2026-09-22.md).
- [AULA family review](current/AULA_KNOWN_PROTOCOL_REVIEW_2026-09-22.md).
- [Pwnage correspondence — local only](current/PWNAGE_SUPPORT_CORRESPONDENCE_2026-09-22.md).

- [Published 1.6.1 release](current/RELEASE_1.6.1_PUBLICATION_2026-09-22.md).

- [1.6.1 patch notes](releases/RELEASE_NOTES_v1.6.1.md).

- [Published 1.6.0 release](current/RELEASE_1.6.0_PUBLICATION_2026-09-21.md).

- [Firmware research foundation](current/FIRMWARE_FOUNDATION.md): architecture, evidence contract and acceptance gates.
- [Firmware corpus and batch research](current/FIRMWARE_CORPUS.md): current tools, pinned environment, recovery, image inventory and exact-image emulator evidence.
- [Firmware audit handoff](current/FIRMWARE_AUDIT_HANDOFF_2026-09-20.md): owner requirements, exact state and continuation for a fresh chat.
- [Public keyboard spreadsheet — latest additions](current/KEYBOARD_SHEET_KOREA_EXPANSION_2026-09-20.md): 473 models/configurations / 85 brands; owner scope includes all analog keyboards, independently of HallJoy support. Expansion remains ongoing.
- [Magnetic-model verification and corrected research outcomes](current/KEYBOARD_SHEET_MAGNETIC_AUDIT_2026-09-20.md): all 149 additions rechecked; mixed-switch limitations and tester results recorded.
- [Latest prerelease and verified artifact](current/PRERELEASE_2026-09-21.md).
- [1.6.0 candidate release notes](releases/RELEASE_NOTES_v1.6.0.md).
- [Application README](../README.md) and [hardware compatibility](SUPPORTED_HARDWARE.md).
- [Built-in layout catalog](KEYBOARD_LAYOUTS.md), including the documented local additions.
- [Support on Discord](https://discord.gg/5FQ297yZh).

## Required support-status workflow

- [Synchronize support decisions with the live Google Sheet](development/SUPPORT_STATUS_SYNC.md) before completing support changes or publishing a release.

## Build and regression evidence

- [Camera latency tester](current/GAMEPAD_LATENCY_TESTER_2026-09-21.md): independent controller observation and camera target.

- [Keychron custom FAR firmware and UAP latency review](current/KEYCHRON_UAP_LATENCY_REVIEW_2026-09-21.md): code-controlled waits, protocol findings and bounded claims.

- [Build guide](development/BUILD_README.txt), [testing guide](development/TESTING.md),
  and [repository layout](current/PROJECT_LAYOUT.md).
- [Staged build/replacement lifecycle](current/BUILD_REPLACEMENT_LIFECYCLE_2026-09-19.md).
- [Input lifecycle and Pause fix](current/INPUT_LIFECYCLE_REVIEW_2026-09-20.md).
- [Profile persistence and interrupted saves](current/PROFILE_PERSISTENCE_REVIEW_2026-09-20.md).
- [Automatic layout and reconnect tests](current/AUTOMATIC_LAYOUT_REVIEW_2026-09-20.md).
- [Logging policy and crash-report privacy](current/LOGGING_PRIVACY_REVIEW_2026-09-20.md).
- [Background telemetry work](current/BACKGROUND_WORK_REVIEW_2026-09-20.md).

Routine checks are short and hypothesis-driven. No speculative long soak is
required. Optional continuous logging and mandatory incident reports are distinct.

## Current integrations and layouts

- [AJAZZ AK820 MAX RGB ordinary support](current/AJAZZ_AK820MAX_REVIEW_2026-09-20.md): tester confirmation, local production integration and Sheet readback.


- [MonsGeek / Akko protocol research, Slice75 and EPOMAKER G84 HE references](current/MONSGEEK_AKKO_PROTOCOL_2026-09-22.md): local native integration; validation and support-status evidence.

- [Active K4 HE low-latency firmware/protocol work](current/K4_HE_LOW_LATENCY_PROTOCOL_2026-09-21.md): backups, measured legacy transport and actual source limits.

- [Wooting / Keychron status correction, NuPhy and Razer protocol review](current/BRAND_PROTOCOL_REVIEW_2026-09-21.md).

- [Known-protocol additions: Keychron K6/Q2/Q4 HE and Wooting 80HE+](current/KNOWN_PROTOCOL_ADDITIONS_2026-09-21.md): local implementation, supported Sheet status; see the subsequent protocol review.

- [NA87 / MINI 60 HE Pro](current/NA87_MINI60_STANDARD_SUPPORT_2026-09-19.md).
- [MG75 Pro](current/MG75_PRO_INTEGRATION_2026-09-19.md),
  [HERO84](current/HERO84_INTEGRATION_2026-09-19.md),
  [GravaStar](current/GRAVASTAR_LAYOUTS_2026-09-19.md),
  [O3C](current/SAYO_O3C_CONFIG_2026-09-19.md).
- [ATK Hex80](current/ATK_HEX80_NATIVE_FIXES_2026-09-14.md),
  [IPI / QBZ](current/IPI_NATIVE_SUPPORT_2026-09-14.md),
  [Keychron](current/KEYCHRON_HE_LAYOUT_CATALOG.md).
- [Wooting extended layouts](current/WOOTING_EXTENDED_LAYOUTS_2026-09-19.md),
  [Razer regions](current/RAZER_REGIONAL_LAYOUTS_2026-09-19.md),
  [Redragon ABNT2](current/REDRAGON_K673_BR_LAYOUT_2026-09-19.md).
- [Hidden controls and Wooting grouping](current/HIDDEN_CONTROLS_AND_WOOTING_MERGE_2026-09-19.md).
  Its old broad shutdown policy is superseded by the staged build lifecycle above.

## ATTACK SHARK

X65 Pro has ordinary support approved on 2026-09-21. Other family models retain
testing notices; broader investigation remains paused by owner decision.
Custom remap readback remains deferred.

- [37 exact native profiles](current/ATTACK_SHARK_FAMILY_SUPPORT_2026-09-20.md).
- [Automatic layouts for all admitted profiles](current/ATTACK_SHARK_FAMILY_LAYOUTS_2026-09-20.md).
- [Latest bank/scale limits](current/ATTACK_SHARK_BANK_INIT_2026-09-20.md).
- [Focused input-path diagnostic](current/INPUT_PATH_DIAGNOSTIC_2026-09-19.md):
  no confirmed cause for the tester's Forza/Block Bound Keys report.

## History and research

- [AJAZZ AK820 MAX firmware review](current/AJAZZ_AK820MAX_REVIEW_2026-09-20.md): target RGB revision unresolved; downloaded no-light firmware findings kept separate.

The [preserved previous index](archive/DOCUMENTATION_INDEX_BEFORE_2026-09-20.md)
keeps links to earlier research, audits and releases. The
[previous hardware document](archive/HARDWARE_STATUS_BEFORE_2026-09-20.md)
retains detailed original test evidence. Neither is a current compatibility list.
Documents under `research/`, `archive/` and `v1.4/` remain historical evidence;
do not revive superseded release gates or disabled-model assumptions from them.
