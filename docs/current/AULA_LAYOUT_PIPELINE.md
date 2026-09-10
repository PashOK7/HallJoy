# Aula: current layout preparation result

## Runtime integration completed (2026-09-09 follow-up)

Delivered `build/release/HallJoy.exe`, SHA256
`54D71CB4A981364DBFFE25363C3BA78A45CE84648668510DF66022A51A868653`.
Complete static + portable C++ suite PASS (`.local/aula-integration-full.log`),
13 pipeline tests PASS, repeated production UI/persistence suite PASS at
`HJProfileTest-d42e650bc30842f09c141ea3119a5113`, including rejected-startup
file preservation. No physical keyboard validation or visual assessment claimed.

Three built-ins now come from the common generator: WIN60 Standard ANSI (61),
WIN68 Standard ANSI (68), WIN60 MAX ANSI (61). See the generated registration
and identity tables under `src/HallJoyProject/HallJoy/generated/layout_pipeline`.
`py tools/layout_pipeline.py integrate` updates those tables and exports with
automatic backup and concurrent-change checks; `check Aula` verifies all bytes.
The historical preparation-only description below is superseded by this section.

W669 publishes a generated identity token only after the owning session repeats
the exact firmware-product check, completes travel/map proof and subscribes.
Marketing-name fallback sessions keep token zero. MAX publishes its token only
for ExactWin60HeMax proof and board `0A021902`; compatible siblings remain zero.
Disconnect/failure clears the token. Telemetry samples it on both sides of the
connected observation, discarding an identity that changed during the read.
No new HID commands, polling timers, locks in the input loop or protocol changes.
Native backend internal ABI is 3; all modules are compiled together.

The existing first-run policy now consumes this token for exactly one native
device, only after coherent discovery completion. Saved/manual selection and
independent overlay selection remain unchanged. Unknown tokens and multiple
devices do not cause selection. Production-linked tests exercise every identity
alias, zero/unknown identity, multi-device ambiguity and one-shot behavior.

MAX source: the [official driver directory](https://www.aulastar.com/web-drive/)
links MAX/Pro to https://magnet.aulastar.com/ . Locked JS `index-C7aUVaaC.js` and
CSS `index-lKmrC98y.css` are stored in `docs/research/aula-max-layout-sources`.
The driver maps layout ID 10 (board high byte 0A) to its 61 layout. Geometry is
calculated from its width/height/margin arrays and border-box flex CSS: content
width 873, wrapper height 58, top margin 1, padding 1. The cap interior excludes
wrapper padding and is uniformly scaled 42/56 with absolute half-up rounding.
The physical matrix is the firmware-proven native MAX map, including C++ implicit
zero initialization and physical Fn -> 0x409. This is not Standard geometry.
The adapter rejects changed hashes, dimensions, flex rules and unexpected wrapping.

KP-TE153 remains pending attribution/compound-shape review; HERO84 remains frozen.
No unrelated brand or additional protocol admission is introduced.

Backup: `.local/backups/aula-integration-20260909/`. Generator updates also create
scoped automatic backups. Simulator/native builds and static suite PASS. Private
desktop persistence/UI/first-run suite: `HJProfileTest-ef43ff20ec594d6eae258a73fbd5bbb1`.
The intermittent overlay test was hardened to clear inherited thread Ctrl/Shift
state before synthetic WM_CHAR; localized assertions now identify the failed
phase instead of returning an opaque false. No visual/hardware testing claimed.

2026-09-09. Use `py tools/layout_pipeline.py summary Aula` first.

## Prepared, not installed in EXE

- WIN 60 HE Standard ANSI: 61 keys, SI2825 products.
- WIN 68 HE Standard ANSI: 68 keys, SI2828 products.

Exact product aliases, URLs, hashes and factory-profile bindings are recorded in
`tools/layout_catalog.json`. Both official JSON files were downloaded from
`https://hed.aulacn.com/config/keys/` and their SHA256 values match the earlier
protocol audit in `docs/protocols/AULA_WIN60HE_STANDARD_PROTOCOL.md`. No new
protocol research, hardware test or HID transaction was needed.

Official driver directory/reference: https://www.aulastar.com/web-drive/ .
`device.aulacn.com` and `win.aulacn.com` failed TLS in this environment; no TLS
verification bypass was used. Standard's `hed.aulacn.com` supplied the locked
files. The current work does not claim a MAX source acquisition.

Coordinates come directly from official x/y/width/height, with only the global
origin removed and a uniform 42/35 scale; absolute edges use half-up rounding.
No row reflow or per-key tweaking. Every position/HID is checked against the
shipped `Win60FactoryMap`/`Win68FactoryMap`. Fn remains W669 0xFA, not UAP 0x409.
Labels use standard English. Both geometries are ANSI; no artificial language
variants. Source URLs/hashes and native map hash are retained in each review.

Verified stage: `.local/layout-pipeline/aula-reviewed-20260909/`, two INIs,
two detailed reports, three generated C++ registration/identity/geometry files
plus the final completion manifest. Reproduce in a NEW output folder.

## Remaining gates

- Runtime first-run currently consumes UAP model identities, not W669's verified
  firmware product. Extend that verified-session path and test it before wiring
  generated product identities; never select Standard by shared `2E3C:C365` alone.
- WIN60 MAX is already protocol-supported but uses RM6x21, not W669. Obtain its
  exact driver geometry/identity before adding a model; do not copy Standard.
- KP-TE153: supported protocol profile exists, but manufacturer classification
  and exact compound shape still need review. Do not silently call it Aula ANSI.
- HERO84 HE remains frozen/excluded. No new brand or experimental support enabled.

This task built the reusable preparation architecture and demonstrated it on
Aula. It did not install new built-ins, change user layouts, or claim hardware
validation. See `LAYOUT_PIPELINE.md` for commands and integration boundaries.
