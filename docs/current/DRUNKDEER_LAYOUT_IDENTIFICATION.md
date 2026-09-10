# DrunkDeer Antler model identification — 2026-09-09

Historical investigation notes. Implementation and delivery status now live in
`DRUNKDEER_LAYOUTS.md`; investigation-only statements below describe earlier steps.

Official source: https://drunkdeer-antler.com/js/index.CJWCGjvj.js
Downloaded file: `.local/research/layout-import-drunkdeer/index.CJWCGjvj.js`.
SHA256: F4D896DE0E8FC696C108F35C01B3864831ED437CB0E1A1B30BE4D75FA871DBD7.
The vendor links Antler from https://drunkdeer.com/pages/help-center.
Analysis is static; no physical requests, firmware writes or downloaded JS execution.

## Real identification, not VID/PID guessing

`OA().sendIdentityData()` returns a zero-filled 63-byte payload with bytes
0=A0, 1=02, 3=00. Connection handler `qB` queues that request after opening HID.
Response handler `AC` requires reportId equal to keyboard_report_id, command A0,
byte1=02 and byte2=00. It reads payload bytes 4,5,6 and passes them to `yC` as
Q,A,e. WebHID payload excludes the report-ID byte: native full-report offsets
will be one greater. This is a read-identity request, not KQ's simulated/demo
identity packet, calibration, settings write or firmware updater.

`yC` selects:

| Payload bytes 4..6 (hex) | Model |
|---|---|
| 0B 01 01 or 0B 04 01 | A75 ANSI |
| 0B 04 03 | A75 Pro |
| 0B 04 02 | A75 ISO |
| 0B 02 01 or 0F 01 01 | G65 |
| 0B 03 01 | G60 |
| 0B 04 05 | G75 ANSI |
| 0B 04 07 | G75 JP |

Other model/revision branches exist but do not by themselves authorize adding
all DrunkDeer devices. Check each against the owner's supported-only scope.

UK/FR/DE are NOT distinguished by this hardware identity. For A75 ISO the
driver reads browser localStorage `current_iso`, validates 751/752/753 and
defaults to UK (751). Its language/variant selection writes that setting.
Thus HallJoy can identify the physical ISO outline, but must not claim the
same reply reveals the printed regional legends.

## Integration constraints

The earlier assumption that shared VID/PID forces A75/Pro/ISO manual selection
was premature. Extend the discovery identity with this verified response and
use it for exact first-run matching. Do not open a competing reader while UAP
owns the same stream; use the existing owner's serialized transport and
bounded timeout. Unknown/truncated/conflicting replies must not default to A75.
No per-frame identity polling is needed. Preserve the existing one-shot and
manual-selection policy. Hardware behavior has not been newly tested here.

## Additional integration blocker: physical-to-analogue mapping

Static inspection found a concrete mismatch that a new visual layout alone
cannot correct. The official bundle's `getG60()` defines Esc at tracking value
21, Q at 43 and W at 44. Its B7 handler passes the combined tracking offset
directly to `window.updateKeyHeight`. The shipped Soup source uses a common
`DRUNKDEER_KEY` table: offset 21 (`row=1,col=0`) is KEY_BACKQUOTE, while
KEY_ESCAPE is offset 0. The materialized build source
`build/obj/UAP/native/Soup/soup/AnalogueKeyboard.cpp` has that same table.
The separate native DrunkDeer backend explicitly labels non-G65 maps
`generic_uap_unverified`; G65 has its own reviewed map and must be preserved.

This is source evidence of differing model maps, not a new physical test and
not proof that every DrunkDeer keyboard fails. Adding accurate geometry while
claiming correct per-key analogue identity for G60 would hide this issue.
Correcting the analogue decoder is broader than the current layout-only batch;
ask the owner before changing working runtime protocol paths. Existing release
remains unchanged; investigation continued after the owner's latest message.

## Reproducible extraction and new conflicting evidence

`tools/drunkdeer_layout_source.py` now extracts seven physical preview arrays
and seven complete factory layers using literal parsing only. Downloaded
JavaScript is never executed. `tools/audit_drunkdeer_layouts.py` saves the
machine-readable result to `docs/research/drunkdeer-source-audit.json` with the
source SHA256. Existing outputs are compared, never silently overwritten.

The factory `g` constructor declares seven arguments; some calls pass an
eighth ignored modifier hint, retained explicitly in the audit. Array position
is retained independently of `keyIndex`: G75 and G75JP entry 19 incorrectly
claims index 29. Using a dictionary keyed by that field could lose a key.

| Model | Preview keys | Concrete disagreement |
|---|---:|---|
| A75 ANSI | 82 | Preview End at 99; factory assignment at 100 |
| A75 Pro | 82 | Preview End at 99; factory assignment at 100 |
| A75 ISO UK geometry | 83 | Preview 75/85/99 have no factory HID assignment; factory has assignments at 55/100 absent from preview |
| G60 | 61 | Preview/factory offset sets agree; existing generic UAP maps Esc incorrectly |
| G65 | 68 | Preview/factory sets agree; vendor nav positions 35/56/77/98 differ from HallJoy native 36/57/78/99 |
| G75 ANSI | 84 | Sets agree; unused entry 19 has incorrect keyIndex=29 |
| G75 JIS | 86 | Sets agree; unused entry 19 has incorrect keyIndex=29 |

The G65 historical physical log establishes many keys but explicitly did NOT
exercise Delete/End/PgUp/PgDn. Thus the old document's phrase "full map" is not
proof of those four nav offsets, and the new bundle alone is not proof that
firmware 0012 uses the newer offsets. The owner subsequently explicitly chose
the official driver positions ("доверяем официальному драйверу и его позициям").
The native G65 map now uses 35/56/77/98, with old guessed cells cleared. All
other positions, including the 45 directly observed ones, remain unchanged.
Diagnostic map name is now `g65_antler_nav_v3` so old and new logs cannot be
mistaken for the same map. This is source-backed, not new hardware validation.
An additional test compares ALL 126 native cells with the parsed official
G65 factory array, including unmapped cells and Fn/Menu identities.

`tools/drunkdeer_layout_dom.cjs` extracts CSS rectangles with an isolated
headless DOM, blocked network and no vendor JS. It does not take screenshots
or access HID. `.local/research/layout-import-drunkdeer/dom-layouts.json`
contains all seven models and source hashes. These are research rectangles,
not yet approved HallJoy layouts: CSS flex shrinking and compound Enter asset
geometry still need normalization/overlap validation. ISO/JIS assets downloaded
alongside the bundle are data only, not application UI assets.

Validation: eight Python extraction/audit tests; standalone C++ identity
parser regression (known signatures, malformed and truncated replies, unknown
signatures). No physical-device validation is claimed. No decoder, catalog,
profile or running application was changed during the initial extraction step.
The later owner-approved G65 correction changes the native diagnostic source
and its regression/build markers only; the shipped UAP decoder and release
executable have NOT yet received this correction. Catalog and first-run
identification integration remain unfinished; do not report the brand shipped.

Discovery integration must run identity only after the existing device-ID
deduplication, before starting the device worker. `getAll()` is also called on
later discovery scans; placing a query there would repeatedly touch devices
already owned by running workers. Keep the stable device-ID seed unchanged.

Backup: `.local/backups/drunkdeer-models-20260909/`. Release SHA256 remains
`CFF7D1CB9962F91FA68508E6ED05824C3932D128D560DBB9769BD6F31F8B44EE`.
