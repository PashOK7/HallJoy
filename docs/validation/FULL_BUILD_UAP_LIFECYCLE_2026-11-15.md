# Full build: scoped HallJoy shutdown for the physical-UAP gate

Date: 2026-11-15. Local changes on `fix/windows-build-portability`; base HEAD
`35ba83ac1f42fe849bd896721503a880a8cf4c61`. No new commit or remote write.

## Failure and cause

The full `BUILD.cmd` successfully built the plugin, then stopped at the
physical-UAP process-isolation guard. The six processes in the owner's log all
belonged to the existing `.local/portable/HallJoy.exe`. They included its app and
workers, not six separately identified interactive windows. The required
isolation was correct, but this stage did not use the existing authorized
project shutdown/restoration mechanism.

`CheckOnly` was already working. The owner separately confirmed that an earlier
combined transcript contained its successful output followed by a previous full
build's failure. This fix concerns the full build's UAP stage.

## Changes

- `tools/build_uap_isolation.ps1` defines a scoped callback. Before closing any
  process it verifies every enumerated owner's actual executable image path and
  Windows session. Only images inside this checkout and the current session may
  be closed automatically. Foreign paths/sessions and unverifiable identities
  stop the operation before any target is closed.
- The scope reuses `close_project_halljoy.ps1`, including its normal shutdown,
  worker cleanup and process-identity protections. It records original app paths
  and normal/minimized startup style. It pins restorable EXE hashes before
  shutdown and refuses to launch an image changed during the check.
- `tools/build.ps1` wraps only its existing physical-UAP guard and ABI checker.
  Both the builder's global process guard and the leaf Python guard remain in
  place. A new owner racing with shutdown still blocks ABI initialization.
- A `finally` path restores previously running apps after success, ABI error or
  partial shutdown failure. Orphan workers and inactive apps do not cause new
  interactive launches. Restoration attempts continue for the other targets if
  one fails; original check and restoration failures are both reported.
- Dependency compilation runs with the app open. Original apps resume before
  profile tests and application compilation. Candidate replacement continues to
  use the existing exact-target publication helper.
- Added `test_build_uap_isolation.ps1`, wired it into Windows CI, and extended the
  lifecycle static audit. Updated the build guide. `CheckOnly` returns before the
  isolation helper is loaded.

## Fresh verification

| Check | Result |
| --- | --- |
| `tools/test_build_uap_isolation.ps1` | **PASS**, 16 assertions using real, separately named fixture processes. Covers multiple paths/workers, callback exclusivity, ABI failure, partial shutdown failure, foreign-path refusal before any closure, enumeration failure, changed-image refusal, orphan cleanup and no-owner behavior. |
| `tools/test_full_build_environment.ps1` | **PASS**, 10 assertions; CheckOnly still exits without compilation, downloads or process shutdown. |
| Lifecycle and provider-V2 architecture audits | **PASS**; independent global/leaf physical-UAP guards retained. |
| Full `tools/build.ps1 -PythonPath <uv Python 3.14.9>` | Exit **0**, started with the owner's six HallJoy processes running. Native suite, dependency/plugin build, real leaf ABI check, profile/layout/recovery tests, MSVC Rebuild and linked-image/license checks all passed. |
| Exact delivered EXE `--halljoy-require-full-catalog` | Exit **0**. |
| Original personal app readback | `WasRunning=true`, `Count=6`, `WindowStyle=Normal`; new process IDs after restart. Its EXE SHA-256 stayed unchanged. |
| `git -c core.whitespace=cr-at-eol diff HEAD --check` | Exit **0**. |

The full log records, in order:

```text
Temporarily closing HallJoy for the physical-UAP check: ...\.local\portable\HallJoy.exe
PRIVATE_UAP_ABI_RUNTIME=PASS abi=1 ... dual_view_equivalent=1 negotiated_capacity=1
Restored HallJoy after the physical-UAP check: ...\.local\portable\HallJoy.exe
```

The ABI checker reported zero plugin devices in this run. This demonstrates the
ABI/process lifecycle, not an analog hardware-input test. It does not change
keyboard support status. Unexpected production warnings: **0**; the allowed
third-party `LNK4099` for the missing ViGEmClient debug PDB remains.

Local evidence:

- `build/evidence/uap-build-isolation-tests-2026-11-15.log`
- `build/evidence/uap-build-checkonly-2026-11-15.log`
- `build/evidence/uap-build-full-2026-11-15.log`
- `build/evidence/uap-build-before-2026-11-15.json`
- `build/evidence/uap-build-after-2026-11-15.json`
- `build/evidence/uap-build-artifact-2026-11-15.json`

Delivered ordinary EXE: `build/bin/Release/x64/HallJoy.exe`, file version
`1.6.15.0`, **11,553,792 bytes** (`Get-Item`), SHA-256 (`Get-FileHash`):
`c1da243f9dac863138990eb3a747ace9ee60cf228e4ec09c6b72e8aa4b1fe845`.
The personal portable EXE was restored without replacement; its hash remains
`523f07bb84b0561b5f25bf8cd517c4aff9802b483b11ccba5f372af6a0924180`.

Original-file checkpoint:
`.local/backups/20261115_144956_uap_build_isolation/`, with relative paths,
verified SHA-256 manifest and individual restore instructions.

Elevated UAC handling, another real Windows session and visual window placement
were not exercised in this run. The existing close helper owns elevation; the
new scope fails closed for external/unverifiable owners. Hard termination of the
builder or operating system cannot guarantee execution of a `finally` block.
