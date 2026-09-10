# Release preparation review — 2026-09-10

Final audit and four-user feedback: [FINAL_RELEASE_AUDIT_2026-09-10.md](FINAL_RELEASE_AUDIT_2026-09-10.md).

## Latest follow-up: overlay-only input

Supersedes artifact hashes below. Independent main/overlay input subscriptions
fix missing keypad analog when only the overlay has a keypad; immutable union
publication also removes concurrent list mutation. See LAYOUT_EDITOR_OVERLAY_2026-09-09.md
for implementation, backups, failed attempts and final test evidence.
Static audits, production-linked profile suite and native Release build PASS.
Current EXE build/release/HallJoy.exe SHA256:
F6CF016FA3D8B15D80EB2AF83BCAE1D8E16F989FE142EBC92D96CA454232079B.
Current ZIP build/packages/HallJoy-1.5.0-Windows-x64-overlay-input-fix/HallJoy-1.5.0-Windows-x64.zip
SHA256: 6EF62BE7DB0ABBE861D4595E1E9A74163C989B8862D8C625BAEFECA4EF8B7026.
Package hash readback verified; packaged staging EXE installer self-test exit 0.
Earlier ZIPs preserved as superseded backups. No visual or new hardware testing,
signing, or GitHub publication claimed.

## Follow-up: overlay selection and pause preview

Input Overlay follow mode now belongs to Brand; Model and Variant are absent
while following. See LAYOUT_BRAND_MODEL.md. Global settings explains that Pause
releases the keyboard for its web configurator. The existing pause/resume control
is retained. A centered, mouse-only Resume card overlays the keyboard preview
only after a confirmed pause (not during initial startup), remains through resume,
and disappears on Active. Runtime-owner notifications drive state changes, not
polling. Resume sends a one-way command, so queued double clicks cannot re-pause.

The card uses existing fonts/theme drawing, a double-buffered small surface, and
a 2.4-second cosine glow at 33 ms intervals only while paused and visible. Hidden
or minimized windows stop its animation; paint/layout resumes it when visible.
Fonts/timer are owned by the child window and destroyed with it. No backend or
pause lifecycle changes. Backups: .local/backups/pause-preview-20260910/ and
.local/backups/overlay-follow-brand-20260910/.

Validation: .local/pause-preview-static.log PASS; production-linked simulator
profile/editor/overlay tests PASS, including real pause-card mouse/cancel/double
click events, keyboard nonactivation, startup/resume/fault/active/hidden states.
Evidence: HJProfileTest-6cb3a484eb3a43b68c2340365ff0d79d. Native Release x64 rebuild
PASS, only existing optional ViGEm PDB warning. No visual inspection was performed.
The full build evidence below refers to the earlier candidate, not this follow-up.

Current EXE: build/release/HallJoy.exe (1.5.0.0), SHA256
0A5EC36305A6732E67E1464727B26F6A98CBCDFA98A432A5BE0146E231C1BC84.
Current ZIP: build/packages/HallJoy-1.5.0-Windows-x64-preview-fixes/HallJoy-1.5.0-Windows-x64.zip,
SHA256 147595921BE9E556D3045F6C3A075DD010FF97F67C16F65B7F1921AE8B4D392E.
Package script verified staged/archive hashes; packaged staging EXE embedded
installer self-test exited 0. Earlier package below is preserved but superseded.

## Implementation and candidate 1.5.0

Owner approved layout consolidation and all local release preparation. No GitHub
publication or signing has been performed. Version is now 1.5.0 / 1.5.0.0.
See RELEASE_NOTES_v1.5.md at repository root and docs/SUPPORT_REPORT.md.

Implemented:

- 13 reviewed duplicate pairs become combined presets; see LAYOUT_DUPLICATION_REVIEW.md.
- Public README links server/template and explains automatic diagnostic reports.
  Older release notes are explicitly historical, not current installer guidance.
- package_release.ps1 packages exactly four approved distribution files, verifies
  input/staged/archive hashes, refuses overwrites, and writes an external ZIP checksum.
  GitHub artifact upload uses the same explicit four paths, not a directory wildcard.
- .gitignore excludes scratch diagnostics and common credentials; the exact pinned
  ViGEmBus installer is explicitly included because clean source builds require it.
- Version audit compares all central numeric/string representations, not a fixed
  historical version. Source inventory refreshed (530 records).

Final official BUILD.cmd-equivalent tools/build.ps1 PASS (exit 0):
`.local/release-1.5-final-build.log`. Full static/portable C++ suite, simulator
profile suite, pinned UAP rebuild, ABI runtime check with one connected device,
native build and embedded installer extraction/signature self-test all pass.
Only the existing optional ViGEm PDB LNK4099 warning; zero unexpected production
warnings. Additional final-source static run PASS: `.local/release-1.5-final-static.log`.
Official-run profile evidence: HJProfileTest-ed509698d2e646e18976a94d20df89d5.
18 layout pipeline tests PASS. Production-linked profile/editor/picker/first-run/
overlay tests and rejected-startup preservation PASS at Windows Temp
HJProfileTest-ecf5ef341b2344aa865c021139fe3e70.
Additional full profile suite seeded with a COPY of owner's existing Layouts PASS:
HJUpgradeLayoutTest-07447eda753648b393220a4b33aab6d9. Actual user data not exercised.
The original imported Q1 file has UniformGap=6, distinct from builtin default 8;
it deliberately stays separate rather than losing stored spacing preferences.

## Earlier local artifacts (superseded by follow-up above)

- EXE: build/release/HallJoy.exe, version 1.5.0.0.
- EXE SHA256: 27A1FE2F552CD9627C1803C0CDC4863B54FB9FF0BAC2C7250C39467167D7191E.
- ZIP: build/packages/HallJoy-1.5.0-Windows-x64/HallJoy-1.5.0-Windows-x64.zip.
- ZIP SHA256: 6D4D02E24E1349FA79E0600D165AE4A98669ED1243B6589566D8768D99F978AA.
- External ZIP checksum: SHA256SUMS-release.txt alongside ZIP.
- ZIP entries/hash readback verified: HallJoy.exe, dependency-lock.json,
  THIRD_PARTY_NOTICES.md and SHA256SUMS.txt, under HallJoy/.
- ZIP extracted into a fresh Windows Temp directory; packaged EXE embedded-installer
  self-test PASS with matching EXE hash. Evidence directory:
  HJReleaseZipTest-ca0b2d04a9aa4212bd313c2df55c3747. No installer launched.
- Packaging smoke/no-overwrite negative test PASS; existing archive unchanged.
  Its older-version fixture remains under .local/package-smoke-20260910, not release.

Owner handoff: evaluate visuals/normal gaming use, decide whether to release
unsigned or supply a signing certificate, and authorize/perform GitHub publication
with version tag v1.5.0 and matching source commit. ZIP/checksum and release text
are prepared; no remote release, commit or GitHub CI execution is claimed.

Failures retained honestly: first build caught an obsolete 1.4.1 version oracle;
next profile run caught an obsolete K2 ANSI picker expectation. Seeded-layout run
caught the new alias test assuming no pre-existing overrides; corrected the test
to restore exact entries and never remove an unrelated tail item. Fresh and seeded
suites then both pass; these findings did not justify weakening preservation rules.

Backup: .local/backups/release-preparation-20260910/ contains sources/tools/docs,
old EXE and user-data. All 101 copied user-data files were hash-verified. Old EXE:
ECA48DA77D128574C4FB4E03C1F6B047D85FE0FAECC19FE4E24493189F2920D3.

Publication boundaries:

- No .git metadata exists in this working snapshot; no remote commit/tag, tracked
  file set, GitHub CI result, or source-history secret scan can be certified here.
- Filename-only scanning of the nonlocal source tree found no matches for common
  GitHub/AWS credential or private-key header patterns. This is not a proof that
  all historical documents or arbitrary credential formats contain no private data.
- Downloaded vendor research assets are not in the binary ZIP. Their local hashes
  support offline source checks; no new redistribution permission is asserted.
- Historical hardware-specific residuals in RM36_RISK_RECONCILIATION remain scoped
  qualifications, not newly reproduced failures or a demand for every keyboard.
- EXE is unsigned; SHA-256 is integrity, not publisher authentication. Owner still
  chooses signing/publication policy. No signing key is available in this task.
- Owner evaluates appearance and decides publication; agent performed no visual runs.

The original preparation review below is historical.

Read-only inspection, not a new release qualification or authorization to publish.
Current EXE matches SHA256SUMS: ECA48DA77D128574C4FB4E03C1F6B047D85FE0FAECC19FE4E24493189F2920D3.
Last batch build/test evidence: FINAL_LAYOUT_BATCH.md. No new tests run here.

Concrete remaining preparation:

- Choose new version; version.h still declares 1.4.1 / 1.4.1.0.
- Update release notes: current v1.4.1 notes predate recent QOL/editor/layout work
  and contradict README about the embedded ViGEm installer versus manual download.
- Update public support guidance to the server and structured diagnostic reports;
  README still routes troubleshooting to personal Discord pash.ok.
- Document last-batch limits (manual selection, four deferred Razer layouts,
  Air75 screenshot macro omitted); do not equate layout count with new protocol support.
- Run the final official build/gates on the frozen candidate, including dependency
  packaging and runtime gates, and reconcile their evidence with historical audit
  statuses. Do not treat historical open issues as newly reproduced bugs or old
  test passes as qualification of a different executable.
- Prepare a clean allowlisted distribution, not a ZIP of the live release folder:
  that folder currently also contains overlay_perf.log and an extracted runtime
  HallJoyUniversalAnalogHost.dll beyond the documented four distribution files.
  Do not delete the live DLL as part of packaging.
- Review the source publication set for private diagnostics, cached vendor assets,
  redistribution/provenance and secrets; .gitignore is not proof of tracked-file safety.
- Current executable Authenticode status is NotSigned. No signing credential or
  owner release decision checked. Do not claim authenticated publisher identity.

Duplicate-layout consolidation is optional and not a release blocker; see
LAYOUT_DUPLICATION_REVIEW.md. No further brands or broad refactoring are needed
merely to prepare this release. User evaluates visuals; no visual runs performed.
