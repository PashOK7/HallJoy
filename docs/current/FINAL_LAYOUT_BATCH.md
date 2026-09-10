# Final pre-release layout batch — 2026-09-10

Owner explicitly requested Razer, NuPhy and Wooting together, then a stop to
layout expansion for this release. No other brands or new analog routes added.

## Prepared and integrated: 18 additional physical variants

| Brand | Models | Variants |
|---|---|---|
| Razer | Huntsman V3 Pro Mini | ANSI, ISO, JIS (61/62/65 keys) |
| NuPhy | Air60 HE, Air75 HE | ANSI (61/83 published-key positions) |
| Wooting | One, Two, Two HE, 60HE, 60HE+ | ANSI, ISO |
| Wooting | 80HE | ANSI, ISO, JIS |

Total manufacturer catalog becomes 68, plus two technical presets. This is not
all supported models/variants of those brands. Existing analog support is unchanged.

## Explicit limits

- Razer Huntsman V2 Analog, Mini Analog, V3 Pro and V3 Pro Tenkeyless: the
  reviewed current official Synapse Web catalog exposes no product pages for
  their PIDs 614/642/678/679. PID688 is supported and has exact regional SVGs.
  Do not reuse the newer 8KHz model's geometry merely because its name is similar.
  The four remaining Razer layouts are deferred, not analog support disabled.
- NuPhy Air75 HE's screenshot-macro key is omitted: its vendor action has no
  published HID counterpart in the pinned UAP decoder. No fictitious PrintScreen
  analog channel is assigned. All remaining 83 keys, including Fn, are represented.
- Wooting v4 covers the listed keyboard families. 60HE v2, split-space variants
  and UwU are not included in this bounded batch.
- All 18 presets are manually selectable. No VID/PID-to-ANSI fallback is added:
  existing session telemetry does not prove regional layout for these presets.
  Existing exact first-run matching for previous brands remains unchanged.

## Sources and generation

- NuPhy: `https://drive.nuphy.io/static/js/main.23dc78ef.js`, module97075 exports
  Ud/Air60HE and sg/Air75HE, arrays y/C. Manufacturer unit coordinates/widths;
  normalize to HallJoy's 46px pitch and 4px key gap, round absolute edges.
- Razer: `https://synapse.razer.com/products/688/`, build2608200716.
  `main.2776d388.js`, regional buttonList chunks1823/1844 and inline JP45578;
  regional US.3007e3a0.svg, UK.b62b4afd.svg, JPN.7e1f2cc0.svg. Extract only
  `.selection` contours; cubic extrema via svgpathtools, English HID labels.
  For JIS Enter recover the inner corner before its concave rounding; using
  the tangent after rounding would intersect the neighbouring key.
- Wooting: `https://v4.wootility.io/assets/index-154c0c6c.js`. Readable official
  GenericDeviceLayout/NewGenericDeviceLayout/DeviceLayout80HE, selected factory
  first layers, matrix bounds and layout-specific overrides. OEM Fn/profile keys
  use the existing pinned UAP 0x403/404/405/408/409 mappings. ISO/JIS extension
  follows the driver's KeyShape SVG (x=-10..50, y=0..80; 0.25u extension).

Raw cached source: `docs/research/final-layout-sources/`.
Reviewed normalized reports: `docs/research/final-layout-reports/`.
Every report records original source hashes; catalog separately locks report bytes.
`layout_reviewed_report.py` validates both layers without network or dependencies.
`layout_pipeline.py integrate` emits shared geometry/presets and preserves old order.
These reports do not introduce session-identity entries or alter backend polling.

Re-extraction is optional, static (no downloaded JS execution or browser/HID):

```powershell
py -m pip install --target .local/layout-python-deps json5==0.12.1 svgpathtools==1.7.1
py tools/prepare_final_layout_batch.py --output-dir .local/new-exclusive-review
```

Compare reviewed report bytes before deliberately updating catalog hashes. This
is a source review, not an implicit generation in normal builds. Dependencies
are not shipped with HallJoy. Script refuses an existing stage and validates all
reports and unique IDs before writing. Partial diagnostic stages are local only.

## Validation and backup

18 pipeline unit tests pass, including compound contour overlap, all new key
counts, unique Plus model IDs, rejected metadata/hash drift and no guessed
autoselection. Existing source-locked adapters remain checked. No screenshots;
owner assesses appearance. No physical hardware test is claimed.
Backup: `.local/backups/final-layout-batch-20260910/` (old tools, source, EXE),
plus `.local/backups/layout-integrate-uuswdnoq/` generated-output backup.

All native static/portable C++ tests PASS, exit0: `.local/final-layout-checks.log`.
Simulator and release MSVC builds PASS; pre-existing ViGEm PDB warning only.
Production profile/UI/first-run suite and rejected-startup preservation PASS:
`%TEMP%/HJProfileTest-1a0c5952425f4cc185b6f9754a70c40a`.
Re-extraction reproduces all 18 report contents (text newline normalization);
old generated geometry/preset order matches backup prefix exactly.

Installed `build/release/HallJoy.exe` SHA256:
`ECA48DA77D128574C4FB4E03C1F6B047D85FE0FAECC19FE4E24493189F2920D3`.
Backup EXE SHA256:
`149ECF8CD33324F1900C5F6EEF346D15B0AD59EFAC53FC862B9055B82B037CC6`.
Stop here for owner visual assessment/release preparation; do not expand brands.
