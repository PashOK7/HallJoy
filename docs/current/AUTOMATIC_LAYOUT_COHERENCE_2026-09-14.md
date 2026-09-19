# Automatic layout: coherent telemetry and preview stability — 2026-09-14

The owner reported a periodic jump in the idle keyboard preview. The saved
HallJoy.log contains30 automatic-selection transitions, including K4 -> missing
-> K4 pulses at19:21:12,19:22:44,19:24:45 and later. During these pulses the SDK
source count stays1 while plugin telemetry count becomes0 for roughly200–300ms.
The UAP child is not restarted at these times. Intervals are irregular; this is
not a one-minute layout-refresh timer. Startup device-refresh restarts are a
separate event already visible in the log.

## Root causes

AnalogHostClient_GetTelemetry retried a shared-memory seqlock five times. If all
attempts collided with the writer it returned success with deviceCount0 and
potentially other fields from an incomplete attempt. Automatic layout treated
that provisional empty list as proof of disconnection, restored the saved manual
preset and selected K4 again on the next coherent read. This rebuilt the preview
twice. The older first-run selector checked source-count coherence, but the new
automatic selector had omitted that condition.

A second issue sent WM_APP_KEYBOARD_LAYOUT_CHANGED for status-only changes.
The main page unconditionally destroyed/rebuilt keyboard controls, cancelled
key dragging and recomputed page dimensions even when geometry was unchanged.

## Corrections

- Stage the entire device-telemetry group privately. Accept it only after equal
  even sequence reads and agreement of dense and descriptive device counts.
- Keep the last coherent UI metadata snapshot under a lock, tagged with launch
  nonce, host PID and restart counter. A failed capture may reuse that snapshot
  only for the same healthy owner, within the existing1000ms freshness bound.
  Lower publication generations cannot replace a newer accepted snapshot.
- Host failure/replacement and expired cache do not yield a valid cached list.
  Explicit deviceSnapshotValid distinguishes unavailable evidence from a genuine
  captured empty list. A coherent empty list is applied immediately; no unplug
  debounce or cosmetic delay was added.
- Cached metadata cannot declare a new provider-plane transaction coherent.
  Live analog samples, input neutralization and gamepad output are not cached by
  this helper and retain their independent data-plane/liveness policies.
- The automatic selector ignores unavailable/inconsistent observations while the
  host is healthy. It changes layouts on coherent device evidence. Thus seqlock
  contention cannot manufacture a device removal or trigger a geometry rebuild.
- A status-only notification refreshes Global settings without rebuilding the
  keyboard or cancelling drag. The UI compares immutable layout snapshot identity
  when deciding whether a real layout-change notification is needed.

## Validation

The production-linked simulator profile/control test passes10,000 repetitions
of the observed count mismatch, invalid-snapshot and adjacent-generation cases.
All retain exactly the same immutable preview snapshot and selected preset.
A coherent empty inventory still restores the manual preset immediately.
Evidence: .local/automatic-layout-coherence-profile-final/profile-test-result.txt.

The actual AnalogHostClient_GetTelemetry implementation is additionally exercised
with an isolated SharedState fixture:1000 reads while its write sequence is odd,
a stable but inconsistent pair of counts, a coherent empty list, host error and
host replacement. The production reader preserves valid metadata and never
accepts a partial list. This test starts no backend or device worker and opens no
keyboard.

The portable cache test injects100,000 failed captures, coherent removal,
reconnection, host/session replacement, expiry, generation rollback and concurrent
publication. Static checks verify staging, validity propagation and the status-only
branch before expensive rebuilding. The unified run is recorded in
.local/automatic-layout-coherence-checks.txt; all static and portable tests PASS.

Simulator and diagnostic Release builds use the existing directories. The owner's
running HallJoy was closed normally for replacement; no visual run was performed.
The saved original log and source/EXE backup are in
.local/backups/automatic-layout-coherence-20260914/.

Delivered EXE: build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe.
SHA256: 77cb1dee0ce050d7ea8d05950ff25dd06d83e4578397530366e1a54d8d0d6eac
