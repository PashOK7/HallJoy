# Configured XUSB shadow design

Date: 2026-08-22

Package: R2-B2c through R2-B2f. Current status: the shared explicit-state
mapping builder and neutral-frame/XUSB boundary are qualified. `Backend_Tick`
owns one same-generation parent dense+V2 capture and runs live Provider V2 as a
read-only configured shadow. Production publication still uses only the dense
compatibility result; physical zero-mismatch evidence and route promotion are
pending.

## Purpose

The next route gate must compare what the game would receive, not merely raw key
values. XUSB is the complete Xbox 360 controller state: buttons, both triggers
and four stick axes. The comparison must use the user's actual bindings, curves,
Snappy Joystick/Last Key Priority settings and mouse-to-stick contribution while
never submitting the shadow report.

## Compared implementations

1. Call the old `BuildReportForPad` twice with different providers. Rejected:
   that function mutated global SOCD/Last Key Priority and mouse filter state, so
   the second call could change or mischaracterize production output.
2. Compare only raw or curved per-key values. Rejected: equal source values do
   not prove bindings, thresholds, conflict resolution, mouse merge or output
   conversion.
3. Copy the report builder for the V2 path. Rejected: two implementations can
   drift and a passing comparison would not qualify the eventual production
   component.
4. Extract one production builder with immutable configuration/input arguments
   and explicit caller-owned state. Selected.

## Required boundaries

- one per-tick binding/settings snapshot is shared by qualified and shadow paths;
- curves are read from the same production curve generation;
- mouse movement is acquired once and supplied as a value to both paths;
- qualified and shadow SOCD/Last Key Priority states are separate;
- provider acquisition, settings reads, allocation, waits and I/O occur outside
  the builder;
- comparison names all seven XUSB fields and the full button mask;
- shadow output is never passed to the ViGEm output runtime;
- an unavailable, truncated or incoherent V2 generation disables comparison; it
  can never neutralize or replace the qualified report.

## Foundation checkpoint

`configured_xusb_builder` now owns complete configured standard-controller frame
construction. Its historical module name remains temporarily to avoid mixing a
cosmetic rename into this proof package. The former qualified `Backend_Tick`
route captures its existing bindings and inputs, calls this component once per
pad, then `xusb_output_adapter` performs the only conversion to the unchanged
legacy `XUSB_REPORT` publication. The builder has no access to global settings,
bindings, mouse acquisition, providers or ViGEm.

Portable tests cover every report field, extended Fn binding, threshold edges,
mouse merge, stateful Last Key Priority analog retrigger, paired independent
states and localized input divergence. The complete native suite, MSVC Release
x64 build, exact UAP child/parent capture and ordinary startup/shutdown pass.

This checkpoint intentionally does not claim live V2 XUSB equivalence. The
parent transaction is now complete; the next change must derive the V2 input
map from that exact capture, merge the same native ownership into both maps,
apply one curve/configuration snapshot, run a separate shadow state and retain
bounded field-level mismatch evidence. Only the qualified report may be
published.

## Neutral output boundary checkpoint

The mapping result is now `VirtualControllerFrameV1`: semantic standard-gamepad
buttons, two unsigned triggers and four signed stick axes. It is an internal
value type, not a wire ABI. `xusb_output_adapter` is the only component that
knows Xbox button masks and proves exact conversion of all current controls.
Production still publishes XUSB only; no DS4 target or runtime behavior was
added.

Three scopes were compared. Keeping XUSB as the canonical mapping type would
make every future output inherit Xbox-specific constants. Introducing a fully
general graph for touch, motion and arbitrary axes now would expand this
correctness package without a proven consumer. The selected boundary models
the complete controls HallJoy already emits and allows a future DS4 adapter to
consume them. Output-specific touch/motion extensions require a later versioned
frame instead of silently changing V1.

The production-linked test now proves both the neutral mapping and the exact
XUSB adapter, including all seven XUSB fields and full button mask. Full native
checks, MSVC Release x64, exact production-image UAP dual capture and ordinary
startup/shutdown pass. This is an architectural checkpoint only: it does not
activate Provider V2 shadowing, change the provider route or promote an EXE.

## Parent tick-capture checkpoint

`AnalogHostClient_CaptureTickSnapshot` now copies aggregate dense values,
per-device dense values and optional Provider V2 under one parent-side sequence
transaction. This closes the previous possibility that separate per-key UAP
reads inside one controller report came from adjacent host publications.

The qualified path consumes the captured dense compatibility values only. The
same captured object retains a validated V2 view for the next shadow package,
but no V2 sample is passed to configured mapping yet. An unavailable parent
capture falls back to the legacy analogue read; an unavailable or malformed V2
view disables only shadow eligibility. No digital correlation or ViGEm shadow
submission exists.

## Live read-only Provider V2 shadow checkpoint

The exact parent snapshot is now projected into a versioned raw input map.
Ownership is stored separately from values, so an owned zero remains a real
release. Only USB keyboard-page identities and explicit supported UAP extended
identities are bindable; consumer and semantic namespaces remain unaliased.
Multiple V2 devices merge by maximum value. The same cached native read is
merged into qualified and shadow paths: ordinary HID values use the existing
maximum policy, while native ownership remains authoritative for extended Fn
and Menu identities.

Both routes share one captured binding/settings value, one mouse sample and one
curve definition/generation. They call the same production mapping builder with
separate SOCD/Last Key Priority state. A missing or invalid V2 capture, a
qualified digital fallback or a curve-generation mutation makes the tick
ineligible and exactly resynchronizes shadow state from qualified state. No
digital event triggers, learns or supplies an analogue value.

Eligible reports are compared as `VirtualControllerFrameV1`: the complete
button mask, both triggers and all four stick axes. Bounded in-memory telemetry
retains eligible/matched/mismatched counts, skip reasons, seven field counters
and the most recent mismatch mask/pad/sample generation. It emits no continuous
production log. Only `frames.qualified` reaches `ToLegacyXusbReport`; shadow
state cannot enter the XUSB/ViGEm boundary.

Portable production-linked tests, the complete static/portable suite, official
MSVC Release x64, exact real-child UAP capture and a ten-second production smoke
pass. This proves implementation and local qualification, not a physical
zero-mismatch duration. Provider V2 remains unselected until a bounded evidence
path observes the counters on representative UAP hardware and a separate
promotion decision passes.
