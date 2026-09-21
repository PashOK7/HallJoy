# Hidden Configuration controls, Wooting grouping and build shutdown — 2026-09-19

> Superseded shutdown behavior: see [build/replacement lifecycle](BUILD_REPLACEMENT_LIFECYCLE_2026-09-19.md). Broad build/test shutdown hooks below were removed at the owner's request; only final exact-target replacement may close the app.

## Root cause and scope

After layout/selection synchronization, Configuration showed a second native
mode combo over its retained, parent-painted face. Ksp_SyncUI unconditionally
sent WM_SETREDRAW FALSE/TRUE to hidden backing controls. DefWindowProc handles
TRUE by adding WS_VISIBLE; this changes visibility, not just redraw permission.
The mode combo remained at an old native coordinate while the retained face
used current content coordinates. No duplicate control creation was required.

Removed unnecessary redraw suppression from synchronous settings updates.
Existing invalidation coalesces normally; hidden controls remain hidden. The
same defect in graph info-label updates is removed. Audited all WM_SETREDRAW
sites: Remap batching now pairs suppression/restoration only for windows with
WS_VISIBLE initially set. Removed the unused overlay redraw helper containing
the same unconditional pattern. No per-frame hide workaround was added.

Production-linked regression creates the real Configuration page without showing
it, then changes layouts, selected keys, settings text and page size repeatedly.
It checks each backing child's own WS_VISIBLE (not IsWindowVisible, which could
hide the defect beneath a hidden parent), and preserves control identities.

## Wooting grouping

60HE, 60HE+ and ordinary 60HE v2 have identical key data for each region: ANSI
61 keys, ISO 62 keys. Add v2 as a third reviewed alias of the existing combined
preset. Display: 60HE / 60HE+ / 60HE v2. Keep its existing internal combined name
for profile compatibility; old v2 names resolve through the alias. Edited legacy
layout files still take precedence and remain independent. Split stays separate.
The production alias tests verify third-model geometry, display and overlay lookup.

## Automatic shutdown before build/tests

Owner explicitly requested automatic closure of older HallJoy instances.
Shared tools/close_project_halljoy.ps1 is invoked by tools/build.ps1, the vcxproj
PrepareForBuild target (except design-time builds), and the profile runner before
launch, including -SkipBuild. It requests normal window closure, allows a shared
five-second grace period, then re-enumerates and terminates remaining checkout
HallJoy processes. Fresh path/start-time verification guards PID reuse. Only the
current Windows session and executables inside this checkout are in scope.
Failures stop the operation; unrelated installed/downloaded copies are untouched.

## Evidence

- Full native static audit suite PASS after UI/alias changes.
- Production profile tests PASS, including hidden controls and merged aliases.
- Startup recovery PASS: 16 scenarios plus repeated startup.
- Shutdown helper fixture PASS: matching checkout process stopped, identically
  named process outside checkout untouched, repeated invocation safe.
- Release x64 build PASS through the new MSBuild shutdown target.
- Exact release EXE Shark/Mini60/NA87 and embedded ViGEm tests exit 0; no log or
  extra file created in their isolated directory.
- No visual run by the agent. Owner verifies appearance. No GitHub publication.

Backup: .local/backups/hidden-controls-wooting-merge-before/.
Delivery: build/bin/Release/x64/HallJoy.exe (ordinary optional-logging build).
EXE SHA256: 9a81d0af92416ed4666e9d45409035a8b5ea06f1c9476a966c249ba19fb06cd3
