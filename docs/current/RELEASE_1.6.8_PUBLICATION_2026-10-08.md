# HallJoy 1.6.8 publication — 2026-10-08

Owner request: release everything accumulated since 1.6.7, with the AK820
Sonix revision `0C45:80B1` switched off and set red. Title:
v1.6.8: Ying75, Ace 68 Air III, Logitech RAPID and game profiles v2.

## Source

- Fresh clone of `main` at a510933 (1.6.7 patch notes), `core.autocrlf=false`.
- Transfer from the workspace (`.local/release168-final-manifest.json`): 330
  files copied, 2224 identical, 556 denied paths skipped (exactly the
  `docs/legal/PUBLICATION_POLICY.json` list), local folders skipped. Nothing
  was removed from the clone.
- Removed before staging:
  - `tools/__pycache__` (76 `.pyc`, generated);
  - `docs/current/PWNAGE_SUPPORT_CORRESPONDENCE_2026-09-22.md` (correspondence,
    local-only; not in the denied list, so removed explicitly).
- Private-data scan of the staged diff: one local user path in an AK820
  document was replaced before the release commit. No e-mails, serials or
  tokens found.
- Release commit: 08c381b (author PashOK7, 170 files). No attribution
  trailers (owner memory rule).
- `check_publication_inputs.py`: PASS.

## AK820 Sonix 0C45:80B1

- `Ak820Admitted=false` in `aula_mini60_native_model.h`: the device is not
  admitted by any build. The implementation remains in the tree for a future
  owner decision.
- Notice group `AjazzAk820Sonix` removed; `support_notice_catalog.py --write`
  regenerated the header (287 yellow models).
- Red documentation: [AJAZZ_AK820MAX_SONIX_80B1_2026-10-08.md](AJAZZ_AK820MAX_SONIX_80B1_2026-10-08.md),
  "Decision: red".

## Validation

- Workspace `build_release.ps1` EXIT=0 with `HALLJOY_NO_TEST_CACHE=1`.
- `research_reference_checks.py --record` after the notices change: all
  original private audits passed.
- Native backend checks, workspace: EXIT=0.
- Native backend checks, clean publication checkout: EXIT=0.
- Package `HallJoy-1.6.8-Windows-x64`: EXE SHA256
  `6ee672777f1b74d5df982d439a5187b16b1cd3f4dba76dd1f922b9ba1ba76d58`,
  version 1.6.8.0.

## Published and verified

- Tag `v1.6.8` on 08c381b; `main` pushed (a510933..08c381b).
- https://github.com/PashOK7/HallJoy/releases/tag/v1.6.8 is latest and not a
  draft or prerelease.
- Assets: HallJoy.exe, LICENSE, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt. All
  four downloaded from GitHub match the package byte for byte.

## Pending: keyboard Sheet not synchronized (no Google Sheets connector in this session)

Required by `docs/development/SUPPORT_STATUS_SYNC.md` and not yet done:

- New row AJAZZ "AK820 MAX HE (wired, RGB, 0C45:80B1)": red, "Not supported".
- Re-read and reconcile every yellow row against
  `docs/development/keyboard_support_notices.json` (`--sheet` snapshot).
- Confirm the 1.6.8 status changes are in the Sheet: WLMOUSE Ying75 Supported,
  MCHOSE Ace 68 Air III Supported, Logitech PRO X2 RAPID and PRO X TKL RAPID
  yellow, Redragon K686 HE (3 models) yellow, FUN60 Pro yellow.
- Visual check of the game profiles v2 and tab transitions by the owner is not
  recorded in any document.
