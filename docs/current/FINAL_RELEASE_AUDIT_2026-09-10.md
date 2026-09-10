# Final candidate audit — 2026-09-10

Owner reports four recipients of the latest build have no complaints. No new
production code changes were made during this audit; the tested candidate stays
unchanged. This is not an assertion that all keyboards/scenarios were tested.

## GitHub baseline

Read-only GitHub API and shallow clone of PashOK7/HallJoy:

- Main/tag v1.4.1: d478300daf531e49c77275fccb91ad0faff9f565.
- Latest published release: v1.4.1, 2026-08-10T21:18:17Z.
- Published EXE SHA256: B21060D0FE5676A6301DDB2EEB0412DFBF5EDC4850BAD575B82045905FDE4243.
- Zero open issues/PRs reported by repository metadata at inspection time.
- Two latest workflows at that commit are cancelled, not successful. Earlier
  successful workflow targets 07fc13bf0fb6d5cf48f7484232836d5e3a8db622.
- No 1.5.0 release is published. Local working snapshot has no .git metadata;
  this audit did not create a source commit, push, tag or release.

Links: https://github.com/PashOK7/HallJoy/releases/tag/v1.4.1 and
https://github.com/PashOK7/HallJoy/actions/runs/31433211765.
Read-only comparison clone: .local/github-release-audit-20260910/.

## Regression review

Comparison ignores CRLF/LF-only changes. Addressed backend and scheduler,
W669 protocol, Hex80 backend/protocol, MAD68 Pro R backend/protocol and native
routing implementation are unchanged from published main. W669 backend changes
add verified layout identity, not a replacement input protocol. Existing normal
backend registrations remain; experimental/diagnostic routes remain gated.
UAP/protocol and profile/UI changes are wider than this unchanged subset; no
claim of exhaustive line-by-line proof or hardware coverage is made.

Dependency source commits and ViGEm client remain pinned; installer policy
intentionally changed from manual to embedded pinned ViGEmBus 1.22.0. Encoding,
dependencies, publication and frozen-route guards are included in static checks.
A bounded common-token/private-key pattern scan found no matches in src/tools/
docs/.github/README (not an exhaustive secret audit).

## Candidate and completed checks

- EXE build/release/HallJoy.exe, version 1.5.0.0, SHA256
  F6CF016FA3D8B15D80EB2AF83BCAE1D8E16F989FE142EBC92D96CA454232079B.
- Native MSBuild Release x64 succeeds and output has exactly that SHA256;
  final audit did not replace the release EXE.
- Embedded installer extraction/signature self-test exits 0; no installation run.
- 40 layout import/pipeline/brand tests pass via unittest discovery.
- Production-linked profile/UI tests pass, including configuration roundtrip,
  legacy pair migration, concurrent loads, failure preservation, layout editor,
  independent overlay subscription and pause preview. Log:
  .local/final-release-audit-profile.log; isolated evidence:
  HJProfileTest-5cc19ecff3f84f78b0e0b4199a845534. Backend init attempts: zero.
- ZIP build/packages/HallJoy-1.5.0-Windows-x64-overlay-input-fix/HallJoy-1.5.0-Windows-x64.zip
  SHA256: 6EF62BE7DB0ABBE861D4595E1E9A74163C989B8862D8C625BAEFECA4EF8B7026.
- Every ZIP entry independently read and hashed. Exactly four files: HallJoy.exe,
  dependency-lock.json, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt. EXE hash matches
  installed candidate. No settings, personal logs or simulator included.
- EXE is unsigned, as documented; no publisher-signature claim.

Full static/compiled regression runner PASS, exit 0:
`py tools/run_native_backend_checks.py --require-compiler`;
log `.local/final-release-audit-tests.log`. No release-blocking regression was
found in the inspected paths and completed checks. This is evidence for release,
not a guarantee of zero bugs or CI success on an as-yet unpublished source commit.
77 C++ test programs compiled and ran. MinGW emitted five non-blocking warnings:
two NOMINMAX redefinitions and three aggregate-member initializer warnings for
SP_DEVINFO_DATA (remaining aggregate members are zero-initialized). No compiler
errors. Do not describe this portable run as warning-free.
No screenshots, new physical-device protocol tests or hour-long idle runs.

## Publication gates

Transfer current intended source files to a Git worktree, review the publication
diff, commit them, and run GitHub CI on that exact revision before tagging v1.5.0.
Upload the candidate ZIP and its external checksum, with RELEASE_NOTES_v1.5.md.
Do not tag old GitHub main while attaching the new binary: sources would mismatch.
Prior packages in build/packages are preserved backups, not current candidates.
Unsigned distribution remains a disclosed choice, not a newly introduced blocker.
