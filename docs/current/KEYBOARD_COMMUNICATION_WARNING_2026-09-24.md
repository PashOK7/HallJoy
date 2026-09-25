# General keyboard communication warning — 2026-09-24

Owner confirms the R68 tester had the web configurator open and reports that
things are fine. Requested a general anomaly warning for all keyboards, not
process/browser detection or an R68-specific banner. This is an anomaly
inference, never proof of another application's access.

Implemented common UI-side observer using existing BackendAnalogTelemetry.
Each native protocol is tracked independently; aggregate SDK and UAP routes
are included. No extra HID requests, processes, browser inspection, key history
or forced logging. Fixed-size state; observer runs on existing UI timer even
when window hidden. Startup/runtime admission closed resets observations.

Triggers: previously working source still present but disconnected for1500ms;
two drops within30seconds; explicit backend anomaly. Native protocol failures
can publish ReportCommunicationAnomaly(source) without affecting support status.
ATTACK SHARK periodic identity failure reports this event (all family models).
UAP restart/invalid-snapshot increases after a working connection also signal it.
Counters failedUpdates/zero values/input silence are deliberately NOT generic
fault predicates: they can mean probes or normal idle time in different protocols.

Warning clears after15seconds continuously connected without a new observed
anomaly; detached sources expire after30seconds following a drop. An isolated
unplug alone does not warn. Per-protocol telemetry cannot distinguish all
individual devices within one backend, and successful-looking corrupt replies
or contention before any established source may remain undetected unless that
backend emits explicit evidence. This is a shared baseline, not universal
proof of all possible protocol faults. No periodic identity check was disabled
and no retry behavior changed by this task.

Existing advisory card prioritizes communication warning over testing/missing
source text. Full-width body, no Discord QR/buttons in this state. Text suggests
configuration-app/browser-tab closure as a possibility, explicitly preserves
software required for analog input (e.g. Synapse), and suggests checking USB
if the fault persists. Ordinary log records warning transitions only.

Validation: extended keyboard_support_status_test compiled and executed PASS
(startup, idle, unplug, repeated drops, sustained present/disconnected, recovery,
explicit events, atomic status isolation/reset). Ordinary Release build and six
linked executable gates PASS. No agent visual run per owner instruction.
Final EXE SHA256: 3c945da5ba7a39c033d36fbe4587bbed180b307e123a35c90fadfa558748077c
Build: build/bin/Release/x64/HallJoy.exe. No GitHub publication.
Support classifications unchanged; no Sheet status write or hardware-test claim.
