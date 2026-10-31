# HallJoy 1.6.13 publication — 2026-10-26

Title: v1.6.13: Weikav EG75 HE, EPOMAKER HE65 V2, ATTACK SHARK X82 HE, MCHOSE Ace 60 Pro. Published with `tools/release.py`.

- Release commit 22444c0, tag `v1.6.13`, https://github.com/PashOK7/HallJoy/releases/tag/v1.6.13 (latest).
- Transfer: 127 files written, 1 normalised to main's line endings;
  denied, local and local-only paths skipped. Private-data scan and check_publication_inputs: PASS.
- Native suite and build_release.ps1: PASS. Workspace = clone for every tracked file.
- EXE SHA-256 `338545f052820342e9b558c2903696dfa244ee966032be74fd0e47a1500548af`, version 1.6.13.0. Server-side SHA-256 of HallJoy.exe, LICENSE, SHA256SUMS.txt, THIRD_PARTY_NOTICES.md equals the package.
- Clean-checkout tests: GitHub CI (Windows) on 22444c0.

## Post-release CI fix (reference manifest only)

- Hosted CI failed on the release commit 22444c0 and on tag `v1.6.13`: `research_reference_checks.py`
  raised "Reference changed" for `docs/research/rongyuan-stream/ca88c7a8.js`.
- Cause: this vendor configurator chunk (pinned on 2026-10-24 for EPOMAKER HE65 V2 board 3417) is
  excluded from publication by `.gitignore` (`/docs/**/*.js`) but was missing from
  `PUBLICATION_POLICY.json` `denied_paths`, so the manifest recorded it as a public file that CI
  must find. It was the only public-marked reference absent from the published tree.
- Fix: added to `denied_paths`, `research_reference_checks.py --record` (only change: that entry
  `private: true`). In the publication clone, without the file: `check_rongyuan_stream_profiles.py`
  PUBLIC_REFERENCE=PASS, check_publication_inputs PASS.
- Commit 2e06cf5 on main; hosted CI success (run 40371437001). The released EXE, assets and the
  `v1.6.13` tag are unchanged (same practice as the 1.6.6 follow-up). `.local/release-state.json`
  main = 2e06cf5.
