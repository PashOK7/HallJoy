# Background work review — 2026-09-20

Owner explicitly prioritizes avoiding regressions. This review makes one narrow
UI optimization; it does not change analog polling, USB requests, freshness,
reconnect detection, timers, Pause sequencing or logging policy.

## Measurement and limits

Read-only samples of the six currently running HallJoy-named processes were
collected four times at5-second intervals. Their roles/window states could not
be reliably identified from available process paths, so these are observations
of current activity, not controlled idle/minimized/Pause benchmarks. Two processes
consumed approximately1.66% and4.25% of one CPU core respectively; the other four
had zero measured CPU delta. Thread counts stayed unchanged; handles stayed
unchanged except one decrement; largest private-byte increase was36,864 bytes.
Fifteen seconds does not establish absence of long-term leaks. No app/window was
manipulated for the measurement and no causal before/after CPU saving is claimed.
Raw samples: .local/idle-review-20260920-samples.json.

## Confirmed redundant work and change

KeyboardUI_OnTimerTick already captures BackendAnalogTelemetry for automatic
layout and device status. On Configuration/Gamepad Tester it additionally called
HashAnalogTelemetry every100 ms, which collected the whole telemetry again.
That repeats native backend traversal, IPC snapshot reads and inventory-cache
querying, and may observe a different generation within the same UI tick.

HashAnalogTelemetry now takes the existing tick snapshot by const reference.
Hash contents and notification frequency remain unchanged. This eliminates one
redundant collection per diagnostic refresh (up to10 per second on those tabs),
not all telemetry collection. No cross-tick caching or delayed analog sampling
was introduced. No special performance unit test was added for this small change;
existing production-linked behavior coverage was rerun.

## Reviewed behavior retained

- Hidden/minimized main-window rendering exits before dirty-key invalidation and
  graph/tester refresh; automatic selection still receives current telemetry.
- Main timer still services hooks, recovery and overlay watchdog. Stopping it
  solely because the window is minimized could break necessary behavior.
- Native identity metadata enumeration retains its one-second fallback and
  topology invalidation. Frozen-model inventory remains topology-driven.
  Longer caching needs separate missed-notification/late-metadata evidence.
- Support logging retains bounded background history and mandatory incidents.
- Pause still joins realtime before neutral publication and shuts down native
  providers; this pass does not modify that independently tested transaction.

## Verification and delivery

Full production-linked simulator profile/layout suite PASS, including automatic
layout contention/reconnection and hidden-control checks, plus18 startup recovery
scenarios. Static coherence, Pause and output producer-freshness audits PASS.
Ordinary Release build and four linked-image checks PASS; existing ViGEm PDB
warning only. No visual assessment, controlled three-state hardware benchmark,
long-duration leak test or GitHub publication was performed.

Evidence: .local/idle-review-20260920-tests.txt and
.local/idle-review-20260920-build.txt.
Backup: .local/backups/ui-telemetry-reuse-20260920.zip.
Artifact: build/bin/Release/x64/HallJoy.exe
SHA256:f89789a0b5e5e38a28e66e1d0d505f7e99a56595b3f03157d2941b9c6c566288
