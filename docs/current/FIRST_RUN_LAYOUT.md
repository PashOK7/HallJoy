# First-run exact layout selection — 2026-09-09

Catalog update: 36 exact Keychron variants, generated in
`keychron_layout_identities.h`; see `KEYCHRON_COMPOUND_LAYOUTS.md`. The original
two-device description below is historical; the one-shot policy is unchanged.
ISO/JIS and HE 8K now have independent identities and matrix checks, never ANSI
fallbacks. Previously saved internal K4/Q1 names still resolve unchanged.

Owner approved automatic selection only on the first launch, after discovery,
for exactly one supported analogue keyboard with a reviewed exact layout.
Existing saved/manual choices must never be replaced. Overlay follows the main
layout unless explicitly configured otherwise.

## Implementation

- `app.cpp` arms a UI-thread one-shot only in the existing missing-settings
  first-run branch. Initial default settings are saved before device startup;
  the in-memory one-shot deliberately survives that initial write. Failed profile
  loads still follow the existing fail-closed startup path.
- `KeyboardLayout_LoadFromIni` and manual `SetPresetIndex` cancel the one-shot.
  No new persistence flag or migration is necessary: next launch loads the
  existing configuration, even if the first discovery found no exact match.
- The existing UI tick supplies already-published telemetry. There are no new
  timers, USB enumerations, HID commands or repeated saves. Inactive/minimised
  windows defer evaluation until the existing UI tick runs visibly.
- Wait for runtime admission, ready plugin host, a published fresh snapshot,
  and agreement of host/backend source counts. Adjacent-tick count mismatch is
  pending, not a definitive empty discovery. Consume the decision exactly once
  on coherent evidence, including zero, multiple or unsupported devices.
- Catalogue: Keychron `3434:0E40` (K4 HE ANSI, matrix 6x19) and `3434:0B10`
  (Q1 HE ANSI, matrix 6x15), connected UAP device, interface `FF60:0061`, no
  ambiguous duplicate-safe identity flag. No brand substring guessing; ISO/JIS
  and matching PIDs on unrelated interfaces are not admitted.
- The two reviewed imported geometries are compiled into `imported_layouts.h`
  and appended to built-in presets. Existing default index/order and old K4 HE
  remain intact. Existing user files with the same names remain authoritative;
  no migration overwrites them. This supersedes the earlier export-only phase.
- Successful selection activates the normal layout/overlay snapshot, queues the
  existing main-page rebuild and coalesced settings save. Hotplug after the
  one-shot completes does not switch the layout. Saves keep their existing
  transactional failure handling.

Rollback checkpoint: `.local/backups/first-run-layout-20260909/`.
No user settings or existing K4 geometry are reset to exercise first-run logic.

## Validation

Production-linked simulator test is called by `KeyboardSubpages_TestLayoutEditor`
on the isolated test data root, with real preset activation and snapshot code.
Covers pending startup, mixed telemetry generations, exact K4/Q1 selection,
manual cancellation, saved-config cancellation, no devices, multiple devices,
ISO rejection, wrong interface, one-shot consumption and later hotplug.
Python import tests also compare every compiled geometry entry with the reviewed
exports (100 K4 keys and 81 Q1 keys). No screenshot/visual testing.

Release and simulator builds PASS; 16 importer/compiled-geometry tests PASS;
all static audits PASS (inventory 494). Full profile/UI transaction suite PASS,
evidence `C:/Users/PC/AppData/Local/Temp/HJProfileTest-0a759295981d41d79b76006ab9502022`.
Delivered release SHA256:
`025DED7FF766D4A26073FE8C81722385B3BA70221F7C3BEB59234FE1D75BB7A5`.
Existing optional ViGEm PDB warning unchanged. No physical first-run test with
user settings deletion was performed. Restarted ordinary HallJoy with existing
settings, which intentionally do not trigger auto-selection.

## Retiring the old K4 preset (owner approved)

Owner now prefers the imported geometry and explicitly requested removal of the
old K4. Removed g_keychronK4HeKeys and its built-in entry. Legacy files named
`Keychron K4 HE.ini` no longer re-enter the catalogue; their contents are not
merged into or allowed to overwrite the replacement. Saved main and overlay
names resolve to `Keychron K4 HE ANSI - Imported`, using normal save persistence.
The two other tall-key migrations are unchanged. Unknown names still fall back
normally. The owner's old file was moved, not erased, to
`.local/backups/retire-k4-20260909/Keychron K4 HE.ini`; hash verified.

Both builds and all static audits PASS. Profile/UI tests PASS, including legacy
overlay-name resolution and absence of the retired catalogue entry. Evidence:
`C:/Users/PC/AppData/Local/Temp/HJProfileTest-2493837db1a54b75a14e37bf53fe96b2`.
Delivered SHA256: `B77E7E3D09A9FA131F025C2E9FA44D571F5F4D897684E99068057F20659A2756`.
Earlier statements about keeping the old K4 built-in are superseded here.
