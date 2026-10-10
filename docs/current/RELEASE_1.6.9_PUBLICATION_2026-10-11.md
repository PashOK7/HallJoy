# HallJoy 1.6.9 publication — 2026-10-11

Owner request: release FUN60 Ultra TMR (owner checked the layout visually) and
everything since 1.6.8. Patch notes say nothing about Keychron: the HJO1
firmware exists only on the owner's keyboard. Title:
v1.6.9: FUN60 Ultra TMR and several keys per stick direction.

## Source

- Fresh clone of `main` at 58ea73e (1.6.8 publication record), `core.autocrlf=false`.
  `main` changed after tag v1.6.8 only by that record, which the workspace already had.
- Transfer (`.local/release169-final-manifest.json`): 205 files written, 2260 identical,
  557 denied paths skipped (PUBLICATION_POLICY list, now including the FUN60 Ultra
  geometry excerpt `docs/research/rongyuan-stream/sources/fun60ultra-2352-geometry.js`),
  local-only files held (titan68 trace log, Pwnage correspondence). Ignored copies removed
  from the clone before staging (`git clean -fdX`).
- Line endings normalised to the style of `main` for 8 files (OWNER_CONTEXT.md and 7
  sources/tests had whole-file CRLF/LF churn); the workspace was aligned too.
- Private-data scan of the staged diff: local download paths removed from
  FUN60_ULTRA_2352_2026-10-10.md and OWNER_CONTEXT.md. No e-mails, serials or tokens added.
- `tools/research_reference_checks.py`: files under `.local/` are now private sources
  (they are never published). Before, the FUN60 Ultra evidence check, which re-reads the
  local vendor chunk and firmware when present, failed the public replay in a clean checkout.
- Release index `docs/releases/README.md` now lists 1.6.7, 1.6.8 and 1.6.9 (1.6.7/1.6.8 were missing).
- Every tracked file of the clone is byte-identical to the workspace the EXE was built from.
- Release commit: 036881d (author PashOK7, 151 files). No attribution trailers.
- `check_publication_inputs.py`: PASS.

## Validation

- Workspace: native backend checks with `HALLJOY_NO_TEST_CACHE=1` EXIT=0;
  `build_release.ps1` EXIT=0 (uncached, then final build after normalisation).
- Clean publication checkout: native backend checks (uncached) EXIT=0, private replays
  reported as not run where `.local` sources are absent.
- Research reference record re-run after the policy/notice changes: all original private
  audits passed.
- Package `HallJoy-1.6.9-Windows-x64`: EXE SHA256
  `e748cb7f875317b7c0bf8913022f0538cf995c958ceb480d2ab9bdfae005e409`, version 1.6.9.0.

## Published and verified

- Tag `v1.6.9` on 036881d; `main` pushed (58ea73e..036881d).
- https://github.com/PashOK7/HallJoy/releases/tag/v1.6.9 is latest, not a draft or prerelease.
- Assets: HallJoy.exe, LICENSE, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt. Server-side SHA-256
  of all four equals the package. LICENSE, SHA256SUMS.txt and THIRD_PARTY_NOTICES.md
  downloaded and byte-identical; HallJoy.exe downloaded with curl and byte-identical
  (`gh release download` aborted with "unexpected EOF" on this machine's network).

## Support status synchronization

Support changes since 1.6.8: MonsGeek FUN60 Ultra TMR -> Supported (Sheet row 573 green,
SUPPORTED_LAYOUTS/SUPPORT_NOTICE_CATALOG/SHEET_STRUCTURE PASS on 2026-10-11).
Still pending from 1.6.8: the AJAZZ AK820 MAX HE 0C45:80B1 red row and the 1.6.8
red/green reconciliation (see RELEASE_1.6.8_PUBLICATION_2026-10-08.md).
