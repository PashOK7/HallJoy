# Build replacement lifecycle — 2026-09-19

Owner correction: do not close HallJoy at the start of work or when building
unrelated tests. Keep it running through compile/validation, then replace and
restore it only if the user had it running. This supersedes the broad shutdown
hooks introduced in HIDDEN_CONTROLS_AND_WOOTING_MERGE_2026-09-19.md.

## Implementation

- Removed the global PrepareForBuild shutdown target from HallJoy.vcxproj and
  the eager calls in tools/build.ps1 and run_profile_transaction_tests.ps1.
- Ordinary incremental entrypoint: tools/build_release.ps1. Both it and the full
  tools/build.ps1 build into build/obj/ReleaseCandidate/x64. These internal files
  are not a second delivery. Final ordinary path: build/bin/Release/x64/HallJoy.exe.
- Linked-image checks run on the candidate while the old window stays open.
  A compiler/check failure never reaches replacement.
- tools/publish_halljoy_build.ps1 compares hashes before inspecting processes.
  Equal bytes leave the app untouched. A pending copy is hashed before shutdown.
- Closing requires an explicit exact TargetPath, restricted to this checkout and
  Windows session. Other copies and test builds are not closed. Graceful close
  gets a shared five-second deadline; remaining exact-image helpers are then
  terminated with fresh path/start-time checks.
- Atomic File.Replace keeps a previous-image backup under build/obj. The finally
  path restores the window only if an interactive instance was previously running.
  On replacement failure the old file remains and its app is restored. Orphaned
  child-role processes alone do not cause a new interactive launch.
- File-only simulator profile/startup tests with an explicit data root and the
  forbid-backend-init flag use a separate per-user mutex suffix derived from the
  canonical root. Ordinary builds retain their original single-instance guard.
  This is not an isolation exemption for interactive or hardware tests.

## Validation

- Complete production-linked profile/catalog/recovery suite passed while the
  owner's ordinary HallJoy PID 10112 (started 20:03:54) stayed running unchanged.
- Replacement fixture tests PASS: equal image, missing candidate, exact-path
  isolation, successful replacement/restart, locked-file failure preserving and
  restoring the previous app, and inactive app remaining inactive.
- Full native static suite PASS, including the new lifecycle audit and existing
  instance guard checks. Build-script parsing PASS.
- Staged Release build and four candidate self-tests PASS. Actual final
  replacement restored the ordinary HallJoy window as PID 27756 at 20:11:34.
  Its EXE bytes match the validated candidate. Repeating publication detected
  equal bytes and left that process untouched.
- No visual test or GitHub publication. The full dependency-rebuild path was
  updated and statically checked, not rerun; the incremental delivery path was
  executed end to end.

Backup: .local/backups/build-replacement-lifecycle-before/.
Evidence: .local/build-lifecycle-profiles.txt, build-lifecycle-replacement-tests.txt,
build-lifecycle-static-final.txt and build-lifecycle-final-release.txt.
Final EXE SHA256: 80b689ed5ff695d55ec1981e3b984ae4f035627b248743a127f2b6d54f8feeda
