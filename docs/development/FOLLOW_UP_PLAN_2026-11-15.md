# Follow-up to the accepted Windows installer contribution

Base: upstream `main` at `35ba83ac1f42fe849bd896721503a880a8cf4c61`, fetched on
2026-11-15. Local branch: `fix/windows-build-portability`. The initial review was
read-only. Denis subsequently requested localization, UI fixes and a local
commit. Remote publication remains exclusively his manual operation.

## What the maintainer accepted

PR #11 retains the three contribution commits in its history, followed by the
maintainer's adjustment `ab97588`. The accepted head and merge commit have the
same tree. The comparison is against contribution head `2bc4e5a`.

| Area | Accepted outcome |
| --- | --- |
| Portable helpers | Start/update helpers and their tests retained; SHA-256 verification, EXE backup and settings preservation remain. |
| User guides | Portable data, input/privacy, driving example and nine quick-start translations retained. |
| Installer | Retained as explicit optional packaging; the maintainer added unchecked-by-default personal-data cleanup and tests. |
| Ordinary build | Restored to standalone EXE delivery without an NSIS prerequisite. |
| Output copies and process helpers | Paired root-level app/setup publication and additional UAP auto-close helpers removed. |
| README and artwork | README shortened; new icon/image/SVG assets removed. No separate reason for each artwork removal is documented. |
| Detailed reports | Seven November 11 validation reports removed; the installer guide was rewritten around optional packaging. |

The explicit design decision is in [AGENTS.md](../../AGENTS.md): keep HallJoy a
standalone EXE, package setup separately and require opt-in for personal-data
deletion. This follow-up preserves that contract. It does not assume a motive for
each individually reverted line.

## Plan and acceptance criteria

1. **Make the existing Windows build portable across installed toolchains.**
   Find Visual Studio Clang through `vswhere` without overriding CXX/PATH;
   select the actual VC toolchain for MSBuild; use the discovered Python without
   requiring a separate `py.exe`; scale the simulator's profile fixture by DPI.
   Implemented. Seven compiler-discovery regressions, the native suite and the
   complete release/profile/layout/recovery checks passed in the
   [validation record](../validation/PR11_ACTIONS_REVIEW_2026-11-15.md).
2. **Give TK75 HE V2 users a precise verification path.**
   Add a [USB setup guide](../TK75_HE_V2_SETUP.md) distinguishing ANSI 3167 from
   ISO 3138, manual-layout limits, independent analog controls, neutral/reconnect
   behavior and measured tuning. Clarify the historical opening of the existing
   GamaKay investigation. This changes documentation, not support statuses.
   Functional hardware results remain separate from software PASS.
3. **Keep the contribution reviewable.**
   Keep build/test compatibility as the primary code change. The TK75 guide is an
   independently reviewable documentation addition. Do not restore paired EXE
   outputs, introduce firmware writes, invent ISO geometry or redesign the UI.
   Do not repeat passed full checks solely because the branch name or prose changed.

## Documentation placement

Use `docs/development/` for this active plan and build instructions,
`docs/validation/` for dated test evidence, and `docs/archive/` only when a record
has been superseded. Preserve historical model evidence and mark its chronology;
do not move active references wholesale or restore the deleted report collection.
Private local logs, settings, backups and tester notes stay under `.local/`.

Fresh checks for this documentation/branch step: `tools/project_context.py --check`
passed, all 12 local link targets in the four affected guides/plan resolved, and
`git diff HEAD --check` passed with the repository's CRLF convention accounted for.
At that branch/documentation step, the source/test implementation and local EXE
were unchanged from the completed validation run. A subsequent full BUILD.cmd
failure exposed a separate missing environment initialization in build.ps1; its
fix and verification are recorded in
[the full-build follow-up](../validation/FULL_BUILD_PYTHON_2026-11-15.md).
At the initial review stage the branch had no commits beyond its upstream base.

## Next evidence before broader changes

The useful next hardware contribution for TK75 HE V2 ISO is exact layout evidence
plus functional feedback. A normal EXE, **Gamepad Tester** and **Open log** are the
tester workflow. A regional model name, a working USB connection or an ANSI
relative's preset does not establish complete ISO geometry. Keep board 3138
Experimental until the existing admission/layout/support process is satisfied.

## Subsequently requested interface work

Denis expanded this task with these concrete interface requirements:

1. Add nine interface languages, default English, with a persistent dropdown
   above Factory reset and delayed explanations for selected settings. Preserve
   user names, device identities and profile isolation. Implemented and exercised
   through the production controls and persistence path; see the
   [language guide](INTERFACE_LANGUAGES.md).
2. Allow closing the informational keyboard/Discord banner after the first
   minute of each launch; reclaim its space and restore it on restart. Keep
   operational/limited-firmware warnings available.
3. Repair Profiles shortcut/Clear button spacing, selector corner fills and
   script-appropriate CJK system-font selection without redistributing fonts.

These additions do not change protocol admission, keyboard support status or
firmware. Denis authorized a local commit of the integrated work after checks;
all remote writes and publication remain his responsibility.
