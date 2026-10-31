# HallJoy documentation

Read [active owner context](current/OWNER_CONTEXT.md) first. Both workspace and
project AGENTS.md apply. Later owner decisions override dated audits. This index
routes work; it is not a release log or an extra compatibility catalog.

From HallJoy-main: `python tools/project_context.py --check`.
Use `--topic <name>` for a focused source map and `--search "model or feature"`
for current records and preserved decisions. No application/device is started.

## Task entry points

| Task | Read first | Code / tool entry |
| --- | --- | --- |
| Workspace / Codex | [Agent workflow](development/AGENT_WORKFLOW.md), [project layout](current/PROJECT_LAYOUT.md) | `tools/project_context.py` |
| Build / tests | [Build](development/BUILD_README.txt), [testing](development/TESTING.md) | `build_release.ps1`, `run_native_backend_checks.py` |
| Support / Sheet | [Support sync](development/SUPPORT_STATUS_SYNC.md), [Sheet rules](development/KEYBOARD_SHEET_RULES.md) | `supported_layouts.json`, `support_notice_catalog.py` |
| Layout | [Pipeline](current/LAYOUT_PIPELINE.md), [catalog](KEYBOARD_LAYOUTS.md) | `layout_pipeline.py summary <Brand>` |
| Protocol / firmware | [Behavior review](development/FIRMWARE_BEHAVIOR_REVIEW.md), [foundation](current/FIRMWARE_FOUNDATION.md), [corpus](current/FIRMWARE_CORPUS.md) | `native_analog_backends.def`, `firmware_inspect.py` |
| Logs / tester issue | [Diagnostics contract](development/SUPPORT_DIAGNOSTICS_CONTRACT.md) | `analyze_support_log.py`, `support_log.cpp`, `input_trace.cpp` |
| UI / scrolling | [UI architecture](development/CUSTOM_UI_ARCHITECTURE.md), [transitions](current/TAB_TRANSITIONS_2026-10-03.md) | `custom_page_surface.h`, `custom_page_controls.h` |
| Profiles / persistence | [Profiles v2](current/GAME_PROFILES_V2_2026-10-03.md), [recovery](current/STARTUP_PROFILE_RECOVERY_2026-09-11.md) | `global_profiles.cpp`, `profile_ini.cpp`, `settings_ini.cpp` |
| K4 onboard / wireless | [Onboard](current/K4_ONBOARD_ONLY_2026-10-03.md), [wireless](current/K4_WIRELESS_GAMEPAD_2026-10-24.md) | `keychron_onboard_backend.cpp`, `firmware/` |
| Publication / legal | [Policy](legal/PUBLICATION_POLICY.json), [distribution audit](legal/DISTRIBUTION_AUDIT_2026-09-27.md), [history cleanup](legal/HISTORY_CLEANUP_2026-09-27.md) | `release.py`, `check_publication_inputs.py` |

C++ filenames are under `src/HallJoyProject/HallJoy/`; Python/PowerShell tools
are under `tools/`. The focused command prints verified repository-relative paths.

## Current baseline and recent work

- [Last locally recorded publication: 1.6.13](current/RELEASE_1.6.13_PUBLICATION_2026-10-26.md).
  Read `version.h`, `.local/release-state.json` and the actual EXE for current local
  values. A remote/latest claim requires a live GitHub read.
- [AG75](current/AULA_AG75_2026-10-29.md),
  [Mix 87 III](current/MCHOSE_MIX87III_FIRMWARE_VERSION_2026-10-29.md),
  [K552-M](current/REDRAGON_KUMARA_K552M_2026-10-28.md),
  [RT75 Pro](current/YUNZII_RT75PRO_2026-10-28.md),
  [X82 HE retry](current/X82HE_BUSY_INVENTORY_2026-10-26.md).
- [Unreleased notes](releases/RELEASE_NOTES_NEXT.md) and [published notes](releases/README.md).
  Draft notes alone are not a complete ledger of changes since the last release.
- [Product README](../README.md), [hardware evidence](SUPPORTED_HARDWARE.md),
  [public Sheet](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit).
- [Codex migration and connection verification](current/CODEX_WORKFLOW_2026-10-31.md).

## History and evidence

The [prior owner ledger](archive/OWNER_CONTEXT_BEFORE_CODEX_2026-10-31.md) and
[prior index](archive/DOCUMENTATION_INDEX_BEFORE_CODEX_2026-10-31.md) preserve all
earlier decisions and topic navigation. Relative links were rebased; exact original
bytes are in the migration backup. Dated model reports remain in `docs/current/`.
`docs/v1.4/` and older audits are historical evidence, not universal startup gates.
Private correspondence and vendor inputs without redistribution rights stay local.

Keep this index for routes, OWNER_CONTEXT for active decisions, and the relevant
model/feature document for evidence. Do not prepend routine tasks/releases here.
