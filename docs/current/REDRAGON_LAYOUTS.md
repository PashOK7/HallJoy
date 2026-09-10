# Redragon layout batch, 2026-09-09

2026-09-10: owner accepted both layouts. Overall count: `LAYOUT_PROGRESS.md`.

Two exact supported W669 layouts, English legends:

| Preset | Verified firmware product | Published keys |
|---|---|---:|
| Redragon K673WB-RGB-M ANSI | 7272USHEXYXK673JCARGB | 80 |
| Redragon K673RGB-M ISO | 7272UKHEXYXBJCARGB | 81 |

Source: https://www.illumipc.com/ (manufacturer-linked web configurator).
`tools/layout_catalog.json` pins the exact JSON and CSS URLs/hashes. JSON hashes
are identical to the August 10 protocol research. Cached JS is static evidence
only, SHA256 E09EB699DC3B96D4876274D8DC49D25D6EA3CB5FA2A4BE539A7E0DEBE478DF22.
No vendor JS executed. No HID commands or firmware changes.

`layout_redragon_w669.py` checks every matrix position against the shipped native
factory map and each product against its exact classifier. Geometry uses absolute
edge rounding at 42/35 scale. ISO Enter is not a bounding rectangle: the driver's
`devKeyPanelUK` CSS has a 48%-height top and 80%-width right-aligned stem. The
one-pixel overlap between pseudo-elements is an internal decorative seam; the
outer contour is represented by the existing compound-key format.

Fn (matrix121) and encoder (matrix15) have HID zero in the official source and
are not published by HallJoy's existing backend. They are explicitly recorded as
omitted in review JSON, not assigned invented analog usages. Thus the presets
contain 80/81 functional keys, not every decorative control in the web driver.

## BR is intentionally pending, not unsupported

The official BR JSON (cached with its original E2ED9429... hash) and shipped map
both assign position100 to International1/IntlRo (0x87), with no right Shift.
This is not sufficient evidence to label or substitute it as right Shift, nor
to reuse the UK identity. BR analog support remains fully enabled and untouched;
only its new exact layout/autoselection is pending a resolution of this discrepancy.
See `docs/research/REDRAGON_K673_SOFTWARE_STATIC_ANALYSIS_2026-08-10.md`.

## Integration / checks

Manifest adapters now use an explicit trusted adapter map. Add new runtime brands
after existing ones in catalog order to preserve built-in indices. Generated
registry, geometry and identities are shared with Aula. Existing verified-session
W669 publication automatically supplies these tokens; no per-model UI code.
First-run restrictions and saved/manual choice policy remain unchanged.

Commands: `py tools/layout_pipeline.py check Redragon`, shared Python unit tests,
native static/portable checks, production profile/UI/first-run tests. Geometry
tests cover Enter contour, exact counts, navigation/backslash matrix differences,
no fabricated Fn, mismatched profile rejection, distinct tokens and rejected BR.
Backup: `.local/backups/redragon-layouts-20260909/` plus generator backups.

Validation completed: 15 Python tests, all native static/portable C++ tests
(`.local/redragon-checks.log`, exit 0), simulator and native MSVC builds.
Production profile/UI/first-run suite PASS, including rejected-startup file
preservation: `%TEMP%/HJProfileTest-a1edd86a25ac46d7a10abc37fc431207`.
Existing generated Aula preset order and geometry match the backup prefix exactly.
The only linker warning is the pre-existing optional ViGEm PDB LNK4099.
No screenshots or hardware validation are claimed.

Installed `build/release/HallJoy.exe` SHA256:
`149ECF8CD33324F1900C5F6EEF346D15B0AD59EFAC53FC862B9055B82B037CC6`.
Previous executable is preserved in the task backup, SHA256
`54D71CB4A981364DBFFE25363C3BA78A45CE84648668510DF66022A51A868653`.
