> Scope correction: the owner meant ALL yellow models. The subsequent [full-catalog batch](ALL_YELLOW_LAYOUTS_2026-09-24.md) adds41 more models and records all251 entries. The RongYuan deferral below was superseded for the ready subset found in another archived official client.

# Easy layout batch — 2026-09-24

Owner narrowed the layout task: only straightforward, evidence-backed batches;
ambiguous or difficult models may remain without a model-specific visual preset.
This supersedes the earlier full-catalog completion priority. No guessed geometry.

Implemented five ANSI presets: IROK NA87 Pro (87 keys), ND63 (64), Mercury68
and Mercury68 Pro (68 each), IYX MU68 Pro (68). Exact session identities include
ND63/MU68 Pro Cyan/TTC variants. Geometry comes from the pinned official IROK
client already used for protocol research: row arrays, model key membership,
class inheritance, square dimensions and per-key overrides. No matrix-position
inference. Generator tools/build_jingtai_layouts.py verifies source SHA, model
membership, key counts, unique HID values and overlap; normal catalog integration
provides exact automatic selection and the existing identical-layout grouping.

During this work a real startup bug was found in the previous JingTai expansion:
InstallMap required native_layout::Publish even when no visual identity token
existed. Publish(0) rejects the map, preventing analog initialization. Corrected
to the existing RongYuan behavior: publish remaps only when a visual token exists;
manual visual selection does not prevent factory-position analog operation.
The production-linked simulator now exercises InstallMap + analog publication
for every exact JingTai identity and a deliberately missing visual preset,
in addition to the previous MG75 Pro remap/release regression. Previous portable
map tests did not exercise this admission failure; do not cite them as proof
that the old five-model binary could initialize successfully.

Deferred: broad RongYuan geometry batch. The saved technical JS archive has
registry keyLayout references but the separately imported Figma keyboard
components are absent. Do not substitute a shape using only key count. Other
unresolved visual models retain manual selection; support status is unchanged.

Backup: .local/backups/layout-easy-20260924-180050.zip plus automatic
layout-integrate-rsetikwu generated-output backup. No publication, hardware test,
visual run, firmware write or Sheet status change.

Validation complete: source/layout generation and full static native audit PASS;
production-linked model publication, layout catalog and18 startup recovery
scenarios PASS. Catalog149 source variants /117 visible groups; all144 previous
presets retain identical keys. Mercury68/Pro share one visible selector.
The first catalog run caught the missing duplicate merge; corrected. The next
run failed an unrelated overlay text-edit event check; unchanged-binary retry
passed all checks. This intermittent failure is recorded, not concealed as an
uninterrupted pass. Ordinary Release build and mandatory gates PASS.

Evidence: .local/layout-easy-static-20260924.log,
.local/layout-easy-profile-retry-20260924.log,
.local/layout-easy-release-20260924.log.
EXE build/bin/Release/x64/HallJoy.exe SHA256 f2de1ab1f376a0cb1c45db66d782f7abee458415fbf36055007cf0b3b39020bd.
