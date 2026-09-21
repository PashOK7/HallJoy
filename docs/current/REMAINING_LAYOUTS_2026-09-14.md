# Remaining low-complexity layouts — 2026-09-14

> 2026-09-19: [K673 BR resolved](REDRAGON_K673_BR_LAYOUT_2026-09-19.md): exact manufacturer ABNT2 image confirms a wide /? key, not Right Shift. ABNT2 preset and verified-product selection added; native analog unchanged. Earlier BR deferral is superseded.

> 2026-09-14: [ATK Hex80 native fixes](ATK_HEX80_NATIVE_FIXES_2026-09-14.md).
> Corrected19 matrix slots;87 factory keys including Fn now publish analog.
> Supports32/128-byte payloads, per-key freshness and correlated chunk replies.
> Historical82-key table/report-size assumptions below are superseded.

> 2026-09-14 follow-up: [IPI layouts](IPI_LAYOUTS_2026-09-14.md): four merged ANSI presets for
> eight models. Official demo defaults resolve the previously missing labels;
> the separate Addressed fallback discrepancy remains documented at its table.
> O3C and Plus revisions remain excluded. No new analog routes enabled.

Owner requested all remaining straightforward additions, excluding Sayo O3C.
This supersedes the earlier release-only pause, but does not authorize new analog
protocol work. Deliver at the existing EXE path, closing HallJoy if necessary;
no additional delivery folder. UI/logs/code comments stay English.

## Added: ATK Hex80 ANSI

Official ATK HUB binds PID 1177 to demo/hex80.json, with current selectors
1176/1177/1250 mapped to hex80. The source contains 88 physical controls. The
layout represents 87 keys; the vendor Mute control has no supported analog HID
and is omitted with its geometry gap preserved. Fn is displayed as 0x409,
consistent with other layouts, but the native Hex80 route does not publish Fn.
Manual selection under ATK; no PID-based regional autoselection was introduced.

Sources are under docs/research/remaining-layout-sources-20260914:
- Official https://hub.atk.pro/ serves CDN build 3.2.25.
- https://bpcdn.atkgear.com/hub-v3/production/3.2.25/demo/hex80.json
- https://bpcdn.atkgear.com/hub-v3/production/3.2.25/static/index-M8vylvoC.js

The renderer uses 50px base height, 4px gaps, maximum row width and per-row width
fitting, including margins and wide keys. Extraction applies that actual formula,
then normalizes absolute edges to a 42px key height. All six rows, margins and
key widths are preserved. The script verifies model binding, source hashes and
renderer expressions. No vendor JavaScript is executed. The compressed source
cache is only research input, never a dependency of HallJoy.exe.

The driver's separate keymaps/hex80.json file has stale five-row metadata and
incorrect repeated default actions. Its RGB matrix also represents LED positions
rather than physical keycaps (including split spacebar LEDs). Neither is used to
infer key bindings. The official demo provides physical default key actions.

The existing native backend has 82 mapped slots and does not cover every key
represented by the official factory layout. Its slot table also differs from the
current demo's matrix positions. This addition is physical layout support only;
no new whole-keyboard analog claim or hardware test is made, and the protocol
mapping is not silently rewritten from demo data. Future protocol review should
check that discrepancy against hardware/firmware before changing the slot table.

## Reviewed but excluded from this bounded batch

- Sayo O3C: explicitly excluded by owner; dynamic physical-key/binding association
  requires separate design.
- IPI QBZ75/family: public vendor device catalog and UUID-specific physical geometry
  found through https://qbz.ipigame.cn/ and its public configuration API. Existing
  canonical backend data lacks several key assignments and is not a complete
  factory-label map. Geometry IDs alone do not justify inventing fixed bindings.
  QBZ65/Aurora/Flash variants are not extrapolated from QBZ75.
- Razer Huntsman V2 Analog/Mini Analog/V3 Pro/Tenkeyless: previously documented
  exact-region source gaps remain outside this simple batch; no newer 8K geometry
  substitution. See FINAL_LAYOUT_BATCH.md.
- Redragon K673 BR: previous International1/right-Shift discrepancy is unresolved.
- KP-TE153: previous model attribution/compound geometry review remains pending.
- Frozen ND75/NA87 Pro/HERO84 routes remain unchanged.

No existing user settings/layout files were edited. Owner assesses appearance;
agent uses offline contour/HID tests and production-linked regression tests.

## Reproduction

python tools/prepare_atk_hex80_layout.py
python tools/layout_pipeline.py check ATK
python tools/run_native_backend_checks.py --static-only

The reviewed report and manifest are source-locked. ATK is appended after MADLIONS
so existing preset order remains intact. Existing generated identity data remains
unchanged. Backup: .local/backups/remaining-layouts-20260914/ and the normal
pipeline backup layout-integrate-h5palcdn. Final evidence is recorded below.

## Final validation and delivery

All source/generator/static checks PASS. Production-linked profile, persistence,
layout editor and picker regression suite PASS; backend init attempts zero.
Old generated geometry and preset registration are unchanged prefixes; identity
header remains byte-identical. Native Release/x64 build PASS (existing ViGEm
missing-PDB warning only). Verified ATK Hex80 ANSI string in delivered EXE.
No visual/hardware run was made. Existing Hex80 published HID set does not include:
Home, Del, End, Win, Fn, Ctrl. Slot-position differences remain separately unverified.

EXE: build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe
SHA256: c0e806c34c67d651cc4cc75a632494ffbb56ea7f85bd011825e5872123a6661d
Evidence: build/evidence/remaining-layouts-20260914/profiles/profile-test-result.txt
Logs: .local/remaining-layouts-static.log and .local/remaining-layouts-release.log.
No additional delivery folder was created. User layout/configuration files were
not modified. Other candidates remain deferred for the explicit reasons above.
