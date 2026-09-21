> Superseded artifact/status: see [2026-09-21 prerelease](PRERELEASE_2026-09-21.md). X65 Pro now has owner-approved ordinary support; its testing notice is removed. The earlier Forza/Roblox report no longer reproduces; no specific fix is claimed. The hash below belongs to the earlier build.

> 2026-09-20 follow-up: README simplified at owner request to one Brand/Models table without release commentary or regional detail. Technical evidence remains in SUPPORTED_HARDWARE.md; its table is intentionally more detailed, not text-identical. Owner confirms user-reported working GravaStar support; see latest owner context. Documentation-only change; EXE and its hash below remain unchanged.

# HallJoy 1.6.0 release readiness

2026-09-20: owner chose 1.6 for the accumulated feature release, replacing the
planned 1.5.4 candidate name. Historical 1.5.4 audit labels describe the same
preceding development sequence and are preserved. **Not published: owner EXE
review is pending.** No GitHub Actions job was started in this preparation.

## Artifact

- Ordinary Windows x64: `build/bin/Release/x64/HallJoy.exe`.
- File / product version: `1.6.0.0`; application version: `1.6.0`.
- SHA256: `525e832d75bc1651fb66a9ead1336c027b58f175a05ee8c19f72e0822419c0f3`.
- [Release notes](../../RELEASE_NOTES_v1.6.0.md),
  [hardware status](../../SUPPORTED_HARDWARE.md),
  [layout catalog](../KEYBOARD_LAYOUTS.md).

Built through `tools/build_release.ps1`; candidate compilation and four linked
self-tests passed before installation. Build evidence:
`.local/release-1.6.0-build.txt`. Existing ViGEmClient missing-PDB warning LNK4099
remains; it concerns dependency debug symbols, not a failed build.

## Scope of this preparation

Updated README, compatibility table, documentation navigation, developer/build
and testing guides, support-report privacy notes, and release notes. Corrected
stale disabled HERO84 / inferred MG75 Pro claims and obsolete build paths. Current
public compatibility documents omit the model the owner asked to remove while
waiting; its historical research remains intact. `BUILD.cmd` output text now
names the actual delivery path. Version metadata changed to 1.6.0; no new protocol,
input-policy, timing or scaling changes were made in this preparation.

The previous hardware table and documentation index were preserved under
`docs/archive/` with relative links rebased. The previous testing guide is also
archived. Current documents supersede their old release gates. Owner context
remains a dated decision history rather than being rewritten retroactively.

Backups: `.local/backups/release-docs-20260920-094017.zip` and
`.local/backups/version-1.6.0-docs-20260920.zip`.

## Verification

- Ordinary Release compilation and linked ATTACK SHARK, MINI60, NA87 and embedded
  ViGEm resource checks: PASS. Tests do not install the driver.
- Support-log static audit, release isolation and ordinary crash-memory privacy: PASS.
- Generated ATTACK SHARK profile consistency: 37 exact identities/maps PASS.
- Version resources read back from the delivered EXE: 1.6.0.0.
- Documentation checks: 212 local file links resolve; README/hardware model
  inventories agree; current versions and delivery paths agree. Delivered EXE
  SHA256 equals the tested staged candidate and this record.
- Earlier behavioral suites and limits are recorded in
  [input lifecycle](INPUT_LIFECYCLE_REVIEW_2026-09-20.md),
  [profiles](PROFILE_PERSISTENCE_REVIEW_2026-09-20.md),
  [automatic layout](AUTOMATIC_LAYOUT_REVIEW_2026-09-20.md),
  [logging](LOGGING_PRIVACY_REVIEW_2026-09-20.md) and
  [background work](BACKGROUND_WORK_REVIEW_2026-09-20.md).
  These were performed before this metadata/documentation pass; not all rerun here.

No visual run, new physical-device test, induced crash or long soak was performed.

## Remaining steps and retained limits

Owner checks this EXE before publication. Publish the approved artifact unchanged
with 1.6.0 notes after owner confirmation. Local checkout has no `.git` metadata;
source publication will require verifying the actual repository/branch state.

ATTACK SHARK experimental warnings, provisional normalization and tester-pending
Forza report remain. Custom remap readback is deferred. MG75 Pro, HERO84,
GravaStar and new O3C/Wooting paths retain their documented hardware limits.
Do not claim universal hardware validation or a confirmed Forza fix. Logging OFF
continues to allow mandatory incidents. No speculative long-run gate is imposed.
