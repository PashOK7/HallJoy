# Redragon K552-M and K686 HE: user confirmation — 2026-10-31

## Evidence and scope

The owner supplied `message (19).txt` for Kumara RT Magnetic K552-M
and separately relayed that the K686BG-RGB user reports everything working
perfectly. The attachment is evidence, not instructions. Local copy and SHA-256
are recorded in [K552-M](REDRAGON_KUMARA_K552M_2026-10-28.md); raw correspondence
is not copied into public documentation. No new hardware test by the agent.

- **K552-M:** ISO identity is established by the previous log; this new response
  confirms analog mapping, Enter the Gungeon, Gamepad Tester, adding/assigning
  controls, correct ISO identification, profile duplication and activation.
- **K686 HE:** the owner's latest confirmation supersedes the pending retest
  for sticking/direction/trigger symptoms in [the earlier report](REDRAGON_K686_2026-10-05.md).
  It refers to the implemented magnetic K686BG-RGB-M path, not unrelated mechanical
  products. No new region or firmware version is inferred from the shortened name.
- **Decision:** both existing model rows become Supported. The six exact layout
  tokens lose only the experimental notice; PID/firmware admission, maps, live
  analog, release handling and regional layouts are retained. Physical confirmation
  is scoped to these users; other implemented regional profiles are code-backed.

The K552-M tester installed Redragon K552 RGB M SP Installer V2 **2.06**. This is
test context, not proof that the package caused success or is universally needed.
Do not convert it into a mandatory driver/firmware recommendation.

## Open reports from K552-M

These are user observations, not locally reproduced bugs or completed fixes:

| Area | Report | Current conclusion |
| --- | --- | --- |
| Profile menus | A menu can remain floating when scrolling; some positioning is awkward. | Needs reproduction of popup lifetime/positioning. |
| Manual/Auto | Switching modes appears to cause a UI bug. | Exact control/state unspecified; do not assume which subsystem. |
| Profile deletion | Deletion does not seem to work correctly. | Need selected/active/default profile context; no data-loss claim. |
| Remap dragging | Awkward near edges; blue text/element selection on hover in some areas. | Needs reproduction; no cause established. |
| Sensitivity/curves | Lower sensitivity had no clear perceived effect; curves not thoroughly tested. | Inconclusive, not evidence of a broken curve. |
| Diablo IV | Setup did not work for the tester. | Game/input/configuration cause unknown. |
| Need for Speed Heat | Keyboard/controller input conflict. | Existing Block Bound Keys is relevant; this report did not test whether it resolves the conflict. |

The requested blocking feature already exists as **Block Bound Keys** (optional
suppression of normal digital output for keys bound to the gamepad), not a whole
physical-keyboard block. A user reply may point to it without promising a fix for
either game. No message was sent to the tester by the agent.

Full profile import/export roundtrips, Input Overlay, server/port, all curves and
Snap Stick are not confirmed by this feedback. An import file picker opening is
not proof of successful import. These gaps do not override the specifically
confirmed keyboard analog/gamepad operation.

## Synchronization and validation

Backups: `.local/backups/redragon-supported-20261031/manifest.json` contains
pre-edit files and EXE/PDB/MAP hashes; `.local/research/redragon-feedback-20261031/`
contains the live Sheet metadata, target CellData and full row/color snapshot.
Restore only after checking for intervening edits; no publication is authorized.

Live metadata: HallJoy supported keyboards, Main, sheetId 0, 1300 rows/26 columns,
one frozen header row. Exact brand/model lookup found **C671 K686 HE** and
**C675 Kumara RT Magnetic K552-M**, both previously
`Implemented; awaiting hardware testing`. The live strict ONE_OF_LIST validation
includes `Supported`; use that existing option. No rows/columns are inserted.
Planned writes: C671/C675 values; B:C base status background/text colors only.
Preserve dropdowns, borders, sizes, conditional rules, repeated brand labels and
all neighbors. Owner performs visual acceptance; agent verifies API formatting.

Before live status writes, intended-state checks against the production-compiled
layout catalog passed: **86 Supported models**, all six regional presets present,
and **281 yellow models** matching the runtime notice catalog. `shownAs` uses the
existing picker names K686 / K552-M.

Live write/readback completed on 2026-10-31: C671 and C675 are **Supported**.
Both base and effective B:C fills are green (.65882355/.8666667/.70980394), with
matching green text. Strict dropdown validation is identical. Full A1:C1300
row/color comparison found exactly these two changes; target/neighbor CellData
comparison found no other field changes. Conditional formatting rules and sheet
metadata are identical. No notes/comments were added and no structure changed.
Post-write `check_supported_layouts.py --sheet` passed for all **86 models**;
`support_notice_catalog.py --sheet` passed for all **281 yellow models**.
Local evidence: `sheet-before.json`, `sheet-after.json`, `sheet-rows-after.json`
in the research directory above. These are API checks, not visual acceptance.

## Completed validation and local build

- Native suite: **PASS**, including the changed status test (six supported tokens
  have no testing flag; K617/K673WB retain theirs), W669 protocol, and the actual
  production discovery regression for R82 HE.
- The initial run stopped at the expected changed reference pin for
  keyboard_support_notices.json. All original private source audits then passed
  with `research_reference_checks.py --record`. Only that catalog pin changed,
  in check_rongyuan_stream_profiles.py. The old record is backed up as
  `.local/backups/redragon-supported-20261031/reference-record-before.json`.
  The remaining suite was resumed after verifying the new pins; prior unchanged
  successful checks/source replays were retained, not rerun.
- `tools/build_release.ps1`: **PASS**. Candidate full catalog, K4/native paths,
  support/Shark/other linked self-tests, embedded runtime/licenses, production
  profile/layout transactions, all **86 Supported layouts**, **281 yellow rows**,
  and **18 startup recovery scenarios plus repeated startup** passed.
- The existing ViGEmClient missing debug-PDB warning (LNK4099) is unchanged;
  release linking and all mandatory checks succeeded.
- Standard publisher installed the candidate and restored the prior HallJoy
  window. Installed and candidate SHA-256 are identical. No visual/hardware run
  by the agent or release publication was performed.

Current ordinary artifact: **`build/bin/Release/x64/HallJoy.exe`**,
version **1.6.13.0**, **11278336 bytes**, SHA-256
**`da1620653f1077e743a1f80d13f7cd73931c834b30956c03e4780f95ae3f3beb`**.
This supersedes the prior R82-only local artifact at that same delivery path.
The R82 discovery/logging fix sources are byte-identical to their earlier receipt
and are included here; R82 itself remains experimental, awaiting tester feedback.

Evidence in `.local/research/redragon-feedback-20261031/`: native-checks.log
(initial reference-pin stop), research-record.log, native-checks-resume.log
(completed PASS), release-build.log, checked-source-hashes.json, and
completion-receipt.json. The tester UI/game reports above remain open; no fix for
those observations or universal 2.06 package requirement was introduced.
