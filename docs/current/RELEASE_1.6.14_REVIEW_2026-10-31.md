# HallJoy 1.6.14: release review — 2026-10-31

The owner explicitly requested gathering all changes, patch notes in the previous
release style, updated keyboard support in README, and publication on GitHub.
Target: PashOK7/HallJoy, normal stable/latest release 1.6.14. No extra approval
is needed. Use tools/release.py and the existing four standalone release assets.

## Baseline and scope

Live GitHub review: latest v1.6.13, published 2026-10-26; main matches the local
post-CI-fix baseline 2e06cf5c78ebdd82b41ee895e59cc97fb2fcc82c. Existing release
asset IDs/digests/download counts are saved locally for comparison; historical
assets and tags must remain untouched. Repository push permission was confirmed.

Comparison with that clean baseline found 85 changed/new publication candidates
before release-specific notes and review. Ignored backups/installers/research
payloads remain local. User-visible changes:

- AULA AG75 / Ultra: exact SparkLink identity, user confirmation, 81-key preset,
  manual layout selection, no unlisted-keyboard notice.
- YUNZII RT75 Pro: confirmed USB analog, built-in approximate 81-key layout for
  boards 2865/3755, corrected confirmed-device notice handling. Other RT75/RT80
  revisions are not silently covered by this status.
- Redragon K552-M: four exact firmware products and three built-in regional
  layouts, then user confirmation of ISO analog/gamepad operation. K686 HE:
  existing regional layouts, latest user confirmation, testing notice removed.
- Mix 87 III: other firmware versions admitted through existing data checks;
  1.22 retains its fingerprint path. Version is logged; not every firmware was
  physically tested.
- ATTACK SHARK metadata retry, no successful-result rescan, waiting heartbeat,
  bounded ordinary discovery/open/identity/travel evidence; startup issue flags
  logged in their actual data domain. R82 remains experimental pending feedback.
- Context/documentation entry tools and discovery regression improve maintenance.

README has all four promotions under Supported and retains exact unrelated
experimental entries. New notes follow the previous Now supported / Fixes style.
Unresolved K552-M UI/game observations are recorded separately and are not listed
as fixes. Everglide AE68 Max and Apex Pro TKL 2023 research did not add support.

## Fresh support reconciliation

Live Main metadata: sheetId 0, 1300 rows/26 columns. Full A1:C1300 read checked
all 86 Supported models against the production catalog and all 281 yellow models
against the generated runtime notices: PASS. The affected Supported rows were
also read with validation/base/effective styles: AULA AG75 C132, Mix 87 III C548,
Redragon K686 HE C671, K552-M C675, YUNZII RT75 Pro C814. All are Supported with
green B:C base/effective fills and preserved strict dropdowns. No Sheet edits
were necessary in this release task. X82 HE remains Supported, R82 HE yellow.
The compiled catalog already contains all required regional presets.

## Preparation and verification plan

Verified original-file backup and manifest:
`.local/backups/release-1.6.14-20261031/` (includes pre-release EXE/PDB/MAP,
version/index/notes, release script/state and affected sources/docs). Original
private path spelling is preserved there; two Downloads paths in public tester
evidence were shortened to the attachment filename before publication.

Local evidence directory `.local/research/release-1.6.14/` holds the source
inventory, fresh Sheet snapshots and historical asset metadata. Native suite,
ordinary release build, packaging, private-data/index checks, and server-side
asset hashes run through the standard release command. Do not rerun a passed
suite without an affected change. No hardware or visual tests are claimed.

Publication result and exact artifact hash will be recorded by tools/release.py
in RELEASE_1.6.14_PUBLICATION_2026-10-31.md; hosted Windows CI is verified after
the release commit is pushed. Local execution does not replace clean-checkout CI.
