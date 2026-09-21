# Complete layout catalog audit — 2026-09-19

Owner requested remaining duplicate review, persistence/automatic-selection checks
and README alignment. Clarification: never merge layouts across brands.

## Inventory and changes

Exported actual compiled g_builtinPresets and resolved display names from the
isolated simulator profile suite, including hand-written presets absent from the
JSON import manifest. Audited 95 source variants; existing aliases yielded 80
visible variants. Three additional exact within-brand merges yield 77:

- Razer Huntsman V2 Analog / Huntsman V3 Pro ANSI: 104 keys.
- MADLIONS MAD68HE / MAD68R ANSI: 68 keys.
- IPI QBZ75 / Aurora 75 / Aurora75 PRO ANSI: 82 keys.

Each new merge matches usage IDs, coordinates, dimensions, compound contours and
legends. Source definitions, protocol identities and device matching stay separate.
No cross-brand merge was made. Labels alone or a shared form factor do not qualify.
Existing non-identical regional variants remain separate. User-edited old layouts
continue to take precedence over shared defaults; no user files were rewritten.

## Regression coverage

Production inventory test loads all 95 legacy source names through the actual INI
loader, saves canonical names and reloads them. It also checks every merged source
for untouched-legacy recognition and edited-legacy precedence. Existing automatic
selection suite covers all Keychron identities, native layout tokens, multiple
and missing devices, manual fallback, and all IPI model remaps. A historical test
expected native source names literally; it now compares the resolved preset index,
so it verifies the same physical selection after an alias merge.

New read-only tools/audit_layout_catalog.py validates the compiled inventory:
no unequal geometry in a merged group, no cross-brand group, and no remaining
exact within-brand duplicates. The profile runner invokes it automatically.
Four negative/control tests confirm that equal cross-brand geometry stays separate,
and reject cross-brand merges, unequal geometry and unmerged within-brand copies.

## Documentation and delivery

README now links docs/KEYBOARD_LAYOUTS.md, an exact model/region/key-count table
from the compiled inventory. Wooting names are explicit and v2/Split/UwU layout
additions are labeled 1.5.4 development-only. Supported devices without dedicated
presets (O3C, KP-TE153, experimental HERO84 HE) are distinguished from missing
protocol support. No new hardware-validation claims were added.

Evidence: docs/research/layout-catalog-audit-20260919.json.
Backup: .local/backups/layout-catalog-audit-before/.
Full static suite PASS; production profile/catalog tests PASS; recovery PASS
(16 scenarios plus repeated startup); four audit tests PASS; ordinary Release x64
build PASS. Exact EXE Shark/Mini60/NA87/embedded-ViGEm checks exit 0 and created no
extra files in the isolated test directory. No visual testing or GitHub publication.

Delivery: build/bin/Release/x64/HallJoy.exe, optional logging unchanged.
SHA256: b627151ddb11c8bbb1ecaa7d0f22905594624ff2cff95c29e122f214f86b7128

## Display separator follow-up

All model-group display names now normalize spaced ` + ` separators to ` / `,
including source-defined IPI and UwU groups outside the merge table. Attached
model suffixes such as 60HE+ remain unchanged. Internal preset names, matching
identities and saved profile keys are unchanged. Production profile/catalog tests
PASS; exported display names contain no spaced-plus separators. Public layout
table updated.
