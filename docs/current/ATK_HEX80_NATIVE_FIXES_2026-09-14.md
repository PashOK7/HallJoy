# ATK Hex80 native fixes — 2026-09-14

> Follow-up: [Automatic layout](AUTOMATIC_LAYOUT_2026-09-14.md) now recognizes
> verified Hex80 sessions as ANSI. Factory-map-only input remains unchanged;
> the manual-selection-only statement below is superseded.

Owner selected ATK Hex80 as the next task after IPI. Lack of local hardware is
not a continuation blocker; user reports will supply physical-device feedback.
This task fixes the existing Hex80 route and does not expand it to unrelated
ATK models. No visual run, device configuration or firmware flashing occurred.

## Root causes and fixes

The official Bityuan Hex80 definition `aUt` has six rows and17 columns.
`getCalibrationStatus` reads consecutive four-slot buffers and chunks them by
`this.col`. Its display joins readings through each key's `position` pair.
`actualPosition` is a compact rendered row index, not the firmware matrix slot.
These actual read/display paths establish how to interpret the factory demo;
geometry alone was not used to infer the protocol address.

The old handwritten table disagreed in19 slots. It omitted Home, Delete, End,
Right Win, Right Ctrl and Fn; it also assigned a context-menu channel where the
source has Fn and shifted PrintScreen/ScrollLock/Pause, Enter and arrow keys.
The corrected table publishes all87 represented factory keys, including Fn0x409.
The separate vendor Mute action remains unrepresented; its physical gap remains.
The two trailing slots102/103 remain padding for the last four-slot read, matching
the driver's ceil(6*17/4) loop. Physical geometry was not altered.

The old transport rejected reports smaller than129 output bytes. The current
manufacturer's WFt command class is32 bytes with baseOffset1 and is sent directly
through sendReport. Admission now accepts at least33 HID bytes including report
ID0; output is padded to the HID descriptor size. Both32-byte and legacy128-byte
payload reports are supported. Meaningful bytes can never be silently truncated.
All newly accepted interfaces still require valid travel-info and matrix replies.
The route is limited to known Hex80 PIDs1176/1177/1250: a common Bityuan reply
prefix alone cannot establish another ATK model's geometry.

Publication arrays now include extended keycodes. Each key has its own timestamp:
missing that key's packets for more than500ms produces neutral analog even when
other chunks keep arriving. Native ownership is retained while connected, so a
stale analog does not turn into a full-depth digital fallback press. Telemetry
counts only fresh active keys. Reset clears timestamps along with values.

Session reads correlate offset/count before consuming a response, allowing late
chunks to be skipped while waiting for the outstanding chunk. Parsing stages the
entire response before publishing records; a malformed last record cannot expose
a valid-looking prefix. Existing bounded recovery and proven calibration-exit
policy are retained; no new configuration writes were introduced.

## Reproduction and tests

- `python tools/check_atk_hex80_native_map.py`: source SHA256 locks, controller
  dimensions, real read/display indexing, full104-slot table and exact87-key
  equality with the physical layout report PASS. Registered in the unified runner.
- `hex80_protocol_test.cpp`: corrected control positions, every chunk and report
  ID form, Fn, stale/future/zero timestamps, 33/129-byte output, rejected meaningful
  truncation, late chunk correlation, wrong lengths and malformed-last-record
  atomicity PASS.
- Protocol parser fuzz smoke:250000 iterations PASS.
- Full static suite PASS; existing IPI source checks remain included.
- Simulator profile/control transaction test PASS on a private desktop, including
  editing, selection and persistence; backend init attempts0. It preceded the
  final transport-only edits, which do not run in that test mode.
- Release build PASS; existing third-party ViGEm missing-PDB warning only.

Evidence: `.local/atk-hex80-unit-complete.txt`, `.local/atk-hex80-static-complete.txt`,
`.local/atk-hex80-release-build.txt`, `.local/atk-hex80-profile-test/profile-test-result.txt`.
Backups: `.local/backups/atk-hex80-fixes-20260914/`; layout report integration
`.local/backups/layout-integrate-3w0574c2/` changed only the reviewed-report export.

## Scope and delivery

This is factory base-map support. Firmware remaps, macros and alternate layers
are not queried or synchronized by this existing Hex80 route. Layout selection
remains manual; no regional identification was guessed. No hardware result is
claimed. Current evidence comes from the cached official ATK HUB build3.2.25,
not from newly executed firmware or an extrapolation to all ATK keyboards.

Official sources and their original URLs/hashes remain in
`docs/research/remaining-layout-sources-20260914/` and the reviewed ATK report.
The old `docs/hex80-reference/MATRIX.md` table is historical and superseded here.

Delivered at the existing path, without a new tester directory or ZIP:
`build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe`

SHA256: `5d7f02baabd47d59080f158e432a101d65ce935114e1638e5fa16787741c9609`.
