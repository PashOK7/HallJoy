# ATTACK SHARK X65 Pro: blocking and controller path review

## Scope and evidence

The tester reported Forza switching input prompts and both inputs apparently
stopping when Block bound keys was enabled. No log or exact executable hash of
that incident has been received. Local hardware is Keychron, not X65 Pro.
This review does not establish the incident's root cause or claim a Forza test.

Reviewed current path:
- attackshark_pro_diagnostic.cpp: addressed 0xe5 page reads, validated exact
  revision, coherent shared pages, native publication and analog-only Fn routing.
- native_analog_backend_registry -> backend ReadNativeCached/ReadRaw01Cached ->
  ReadFilteredPair (real curves/deadzones) -> BuildReportFramesForPad (real
  bindings and SOCD state) -> scheduler -> isolated ViGEm output process.
- app.cpp: native analog is independent of Windows keyboard notifications.
  The hook notifies the backend before suppressing Windows delivery. The block
  setting affects the hook, not USB reads or native depth ownership. Shortcut,
  reserved-key, own-window, pause and physical press ownership rules remain.
- vigem_output_process_host.cpp: appliedPublicationSequence advances only after
  transport.Apply succeeds. This confirms transport acceptance, not game use.
- keyboard_render.cpp: the green digital indicator uses Windows digital state;
  it is distinct from the native analog fill. Its absence is not proof of lost
  analog. Actual delivery of blocked events to a game's raw-input path remains
  Windows/game dependent and is not proved by local policy tests.

The previous digital-depth fallback has been removed, not re-enabled here.
No calibration commands, alternative probes or firmware changes were introduced.

## Existing freshness behavior worth distinguishing

All native slots currently require a fresh Fn page as well as their own page.
A missing/stale Fn page therefore neutralizes otherwise fresh keys. Pages expire
at 150 ms and connection status has a separate 500 ms threshold. This is existing
fail-neutral behavior; it was not changed speculatively. Native update/expiry
counts, Fn-unavailable frames and source-positive frames now distinguish it
from a filter, binding, controller or hook problem. These timeouts are freshness
limits, not a time limit on the user's diagnostic session.

## Focused diagnostic build

Build property: HallJoyInputPathDiagnostic=true. Marker:
input-path-20260919-r1. It does not enable the old broad Shark diagnostic or
StabilityTrace. Normal playable UI, physical analog and virtual gamepad remain.
The title and a disabled checked Automatic diagnostic logging control identify
automatic logging. The user's saved Enable logging preference is not modified;
ordinary builds retain optional logging.

One HallJoy.log in the existing data directory, accessible with Open HallJoy
folder (normally %LOCALAPPDATA%\HallJoy; portable mode uses the EXE directory).
No timed diagnostic stop. The existing 4 MiB bounded writer is retained. On
rotation, recent history is retained; cumulative counters preserve session
activity totals, but not the entire old timeline.

Once per second the background writer records cumulative counts separately for
the block setting OFF and ON: native frames/positive frames/Fn unavailable,
configured keyboard reads/raw-positive/filtered-positive, built/active controller
frames, accepted/rejected output publications and passed/blocked bound hook
events. Native health snapshots recur every five seconds. Output status includes
ready/enabled/pad count/generation/applied and published sequences/errors/restarts.
Foreground is only own/external/none; engine state is recorded separately.

Counters represent aggregate stage observations, not one-to-one transactions.
Configured reads can repeat a key bound to several fields/pads; hook events count
both edges/repeats. Mouse bindings can also make a controller frame active.
Asynchronous snapshots are not atomic across threads, and output sequences must
be interpreted within a stable generation. Deduplication of unchanged held
reports is normal; an unchanged applied sequence alone is not an output failure.
No text, key identities, individual depths, report contents, foreground titles,
process names, device serials or paths are added by this diagnostic. Activity
counts and timing are still diagnostic information, not a zero-information log.
Hook/native/realtime instrumentation uses fixed counters; file I/O stays on the
background writer. Production counters compile out. No input emulation added.

## Validation

- Exact delivered EXE: Shark, MINI60, NA87 native and embedded installer tests PASS.
- Added Shark test calls the actual backend native cache, real bindings, curves
  and production configured report calculation, with two simultaneous different
  depths and stale release. Block OFF/ON results match with zero digital events.
  It does not start real USB enumeration or ViGEm. Existing Shark tests cover
  all six revisions, Fn release orders, torn pages and worker failure/cancel.
- Shared hook policy: all 64 input combinations, held-press ownership, repeats,
  setting/focus changes, reserved keys and shortcut cases PASS. This is a policy
  test, not a claim to reproduce OS raw-input delivery in Forza.
- ViGEm channel and telemetry tests PASS; isolated fake transport tests PASS for
  four pads, initialization/update/neutralization/removal failures. Real transport
  and the game were not exercised on X65 hardware.
- Real Windows support-log writer tests PASS for forced diagnostic logging,
  separate counter banks, ordinary disabled/no-file behavior, failure and recovery.
- Static audits PASS; MSVC Release build PASS (existing ViGEm PDB warning only).
- No visual run, no GitHub publication. Backup: .local/backups/input-path-before-review.
- Evidence: .local/input-path-tests.txt, input-path-writer-final.txt,
  input-path-static-delivery.txt, input-path-build-final.txt,
  input-path-verification-final.json.

Delivery: build/bin/Release/x64/HallJoy.exe, version 1.5.4.0.
SHA256: 12f73a17ef39df6ae4e627d0a605dce8338f6d9780e92d0ba89540041ba6bf62
Size: 9307648 bytes.

## Tester steps

Close the old HallJoy and use this EXE with the same bindings. In Forza, try the
bound keys at different depths, including simultaneous holds, with Block bound
keys OFF and then ON. Release all keys before switching modes: existing physical
press ownership deliberately lasts until release. If the issue occurs, close
HallJoy and send HallJoy.log from Open HallJoy folder. Logging is automatic;
there is no required duration. Also report whether analog fill and actual
in-game control stopped, rather than judging only the green digital circles or
input prompts. Do not require another settings profile or digital fallback.
