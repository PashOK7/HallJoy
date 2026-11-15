# Self-contained ViGEmClient debug information — 2026-11-15

## Cause and scope

The previous pinned `ViGEmClient.lib` referenced a compiler PDB that was not
distributed alongside it. Linking HallJoy with debug information therefore emitted
LNK4099. Microsoft's [LNK4099 documentation](https://learn.microsoft.com/en-us/cpp/error-messages/tool-errors/linker-tools-warning-lnk4099?view=msvc-170)
describes `/Z7` as a way to embed debug information in the library's objects.

The dependency was rebuilt from the same immutable upstream commit
`b66d02d57e32cc8595369c53418b843e958649b4`, using the same
`Release_LIB|x64`, v143 / MSVC `14.44.35207` configuration, with
`DebugInformationFormat=OldStyle` (`/Z7`). The upstream checkout remains clean.
`tools/vigem_client_embedded_debug.props` supplies that metadata through
`ForceImportBeforeCppTargets`; no upstream source or project was patched.
HallJoy's debug information remains enabled. No linker warning is suppressed.
The official build now rejects every production compiler/linker warning, with
no ViGEm exception.

The MIT license and existing notices are retained. Source and license hashes
were checked against the original provenance after converting the clean upstream
LF files to the historical checkout's CRLF representation. This is a newline
difference only. See the original
[dependency review](../legal/DEPENDENCY_PROVENANCE_2026-09-27.md).

## Reviewed dependency

| Item | Value |
| --- | --- |
| Original library bytes | 1,246,058 |
| Original SHA-256 | `2D99C44ED8F615EB7A481F8C13781B25D883F07C006CEECB9C694369C969364E` |
| Updated library bytes | 1,606,198 |
| Updated SHA-256 | `01F376457D4C786C9BCBC7049E9FB5991A84E50B5E9330201BA745D95F2C6D2D` |
| Public API | Same 30 `vigem_*` symbols, checked with `dumpbin /linkermember:1` |

The lock and component provenance record the updated bytes together. Embedded
debug information can contain local source paths, so byte-for-byte reproduction
from another directory is not asserted. The build helper still produces a
candidate only; it does not replace the locked library automatically.

## Verification

A local minimal program calls `vigem_alloc()` and `vigem_free()` without
connecting to ViGEmBus or opening HID. It is linked with debug information and
linker warnings treated as errors (`/DEBUG /WX`):

- Original library: exit 1, LNK4099 followed by LNK1218.
- Updated library: link exit 0, no warnings; program exit 0.
- The compiler command log confirms `/Z7` rather than `/Zi`.

Local evidence is retained under `build/evidence/`:
`vigem-debug-before-2026-11-15.log`, `vigem-debug-after-2026-11-15.log`,
`vigem-source-build-2026-11-15.log`, and both `vigem-*-symbols-2026-11-15.log`
files. The source builder remains available as `tools/build_vigem_client.ps1`.

Automated dependency auditing also checks the actual library size/hash against
the lock and provenance, and the debug property sheet against its recorded hash.
This is software verification; no new hardware compatibility result is claimed.

## Integrated verification

- `tools/run_native_backend_checks.py --require-compiler`: exit **0**, normal
  cache enabled; all static and portable C++ tests passed.
- `tools/build_release.ps1`: exit **0** on this invocation. The candidate's full
  catalog/self-tests, embedded legal resources, production-linked profile/layout
  tests and **18 recovery scenarios plus repeated startup** passed. The ordinary
  delivery image matches the verified candidate.
- Source-library, native-suite and Release logs contain **0 warning diagnostics**.
  The Release log includes both HallJoy and simulator builds.
- The actual warning-gate block from `tools/build.ps1` was exercised with clean,
  LNK4099 and compiler C4101 output: clean output passed; both warnings were rejected.
- `git diff HEAD --check` with `core.whitespace=cr-at-eol`: passed.

Integrated logs: `build/evidence/vigem-debug-native-2026-11-15.log` and
`build/evidence/vigem-debug-release-2026-11-15.log`. Artifact hashes and diagnostic
counts are in `build/evidence/vigem-debug-artifacts-2026-11-15.json`.

Verified ordinary image: `build/bin/Release/x64/HallJoy.exe`, **11,567,616 bytes**,
SHA-256 `364C57CBE2BF36C3C4B5724A59C20B65B027DD2E78F9A55521689FAF67EE57F4`.
The complete `BUILD.cmd` dependency/physical-UAP workflow was not repeated; the
ordinary release workflow and focused `/DEBUG /WX` reproduction were run here.

Before replacement, the original library and affected tracked build/audit inputs
were backed up under `.local/backups/20261115_160558_vigem_embedded_debug/` with a
verified SHA-256 manifest and restore instructions. No commit or remote operation
was performed for this follow-up.
