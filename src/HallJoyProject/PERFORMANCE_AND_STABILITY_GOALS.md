# Performance and Stability Goals

HallJoy should feel like a small, sharp native utility even as the feature set grows.

The long-term performance target is "classic 90s/2000s optimization": reduce latency and CPU cost through better design, tighter hot paths, fewer allocations, fewer kernel transitions, better caching, and simpler data flow. Useful features should not be removed just to make benchmarks look better; the goal is to keep the full experience and make the implementation smarter.

Optimization priorities:
- keep the realtime input path deterministic and allocation-free where practical;
- avoid repeated WinAPI/kernel object creation in high-frequency loops;
- keep logging, diagnostics, UI painting, and OBS overlay work away from the critical input path;
- prefer cached, precomputed, fixed-size data structures for code that runs every millisecond;
- measure first, then rewrite the most expensive code in small safe steps;
- preserve visual quality and input correctness while reducing work.

Stability is equally important. HallJoy should aim for effectively endless runtime stability: no crashes, no hangs, no broken input state, no stuck virtual buttons, no corrupted settings, and graceful recovery from device disconnects, bad HID packets, missing dependencies, OBS polling spikes, old games, and unusual Windows input behavior.

Every optimization should be judged by both speed and robustness. A faster implementation that is fragile is not an improvement.

- End-user crash reproduction has a separate privacy-safe `HallJoyDiagnostic.exe`
  build. It keeps Release code generation, synchronously records HallJoy
  subsystem breadcrumbs, and writes an unhandled-exception code plus
  module-relative offset without collecting a minidump or PC inventory. See
  `DIAGNOSTIC_SUPPORT.md`.

## Current Hot-Path Work

- UI architecture decision: scrollable pages should migrate from many Win32 child controls to custom single-HWND retained/immediate surfaces. See `CUSTOM_UI_ARCHITECTURE.md`.
- SparkLink HID polling is the first priority because Irok MG75 Max uses this path during real gameplay.
- Repeated WinAPI event creation in overlapped HID read/write has been removed; the SparkLink/Sayo HID I/O helpers now reuse a per-thread event.
- Per-transaction SparkLink heap allocations have been removed from the normal 64/65-byte HID report path; read/write packets reuse thread-local fixed buffers, with a fallback allocation only for unexpectedly large reports.
- `Backend_Tick` now snapshots device connection/fallback/keycode-mode flags into the per-tick HID cache, so repeated key reads do not reload the same atomics and settings before cache hits.
- `HallJoyTests` plus `tools/bench.ps1` is the native agent-facing test/perf harness. It covers `CurveMath` correctness and emits repeated benchmark reports under `perf_results/`.
- `tools/profile.ps1` is the broader local profiling wrapper: it runs the benchmark suite, records CPU/OS context, and can fold gameplay/OBS runtime logs into one `profile-*.md` report.
- `tools/analyze_latency_log.ps1` summarizes runtime `[rt.stats]` and `[overlay.perf]` logs after manual gameplay/OBS sessions, so latency work can use measured jitter/slow-tick data instead of impressions only.
- `tools/overlay_runtime_capture.ps1` is the focused OBS/browser overlay capture helper. It clears the old overlay perf log, waits through a manual stress pass, and writes a p95/p99/max runtime report without mixing it into unrelated benchmark work.
- `tools/overlay_synthetic_probe.ps1` is the autonomous OBS-free render stress probe. It starts HallJoy's overlay server, opens a headless browser with synthetic HE key depths, and records the same `[overlay.perf]` metrics so rendering/cache regressions can be found without a manual OBS session.
- `tools/run_input_pipeline_profile.ps1` is the production end-to-end load gate. It separates the entire HallJoy process tree from the entire browser tree, attributes persistent worker TIDs plus residual UI/short-lived workers, measures physical SparkLink transaction time and server/browser overlay telemetry, and rejects user-state changes or process survivors.
- `tools/ensure_blend2d.ps1` restores/builds the native Blend2D dependency used by `HallJoyTests`. This gives the agent a browser-free/offline native render benchmark without opening Edge, OBS, or a web view.
- First measured optimization using the harness: `CurveMath::EvalRationalYForX` was reduced from about `322.73 ns/op` to `222.09 ns/op` on the local Release x64 benchmark by avoiding full `Vec2` evaluation inside the binary-search loop.
- `BackendCurve` cache invalidation is now event-driven instead of per tick. Curve caches survive normal realtime ticks and are invalidated only when global or per-key curve settings change. The harness now tracks `backend_curve.apply_smooth_cached`, currently about `166.68 ns/op` locally.
- The harness now also tracks full 255-HID curve tick cost, fast `KeySettings_GetUseUnique` snapshot lookup, HSV color-picker palette generation cost, retained-scroll copy bandwidth, SparkLink route-row parsing, overlay `/state` JSON generation, overlay sprite/label cache-key lookup, SparkLink route pacing, missed-HID diagnostic scanning, backend report building, and SparkLink timeout policy state transitions. These are the first guardrails for backend, settings, color UI, high-refresh scrolling, SparkLink polling, and OBS overlay regressions.
- Input-latency work now has its own synthetic guardrail: `input_latency.sparklink_to_xusb_1khz_6row` models a SparkLink-style 1 kHz realtime loop with six paced matrix rows, raw HE ramp-up, curve application, bindings, and XUSB report building. It reports internal CPU cost per tick plus p50/p95/p99/max simulated delay from raw 50% press depth to gamepad-report visibility.
- `input_latency.sparklink_short_tap_capture_6row` intentionally models a very short HE press window against the same six-row pacing. It does not fail the suite; it reports capture/miss ratio so row-pacing changes can be judged against fast-tap reliability, not only average latency.
- HallJoy exposes product-facing analog telemetry from real devices, not only simulation. The `Gamepad Tester` tab shows source-specific analog rate and resolution data: SparkLink route/matrix Hz and raw full-scale range, SayoDevice depth-response Hz and raw depth levels, and Wooting SDK polling/normalization information.
- SparkLink has experimental live tuning controls in `Configuration`: `HE poll mode` (`Safe`, `Fast yield`, `Max burst`) and `Rows` (`Auto` or a 1..8 cap). These are measurement tools first; any setting that increases matrix Hz must also be checked for packet failures, missed HID presses, and firmware pressure.
- Current local baseline from `perf_results/profile-20260524-165950.md`: SparkLink 21-column route-row parse is about `240 ns` per row, route pacing scheduler is about `2.4 ns` per tick, SparkLink timeout policy state update is about `1.9 ns` per tick, missed-HID diagnostic scan is about `0.67 us` per 255-HID tick, overlay `/state` generation for an 84-key layout is about `10.8 us`, overlay sprite/label cache-key lookup is about `0.52 us` per key, synthetic backend report building is about `171 ns` per pad, full 255-HID curve tick is about `66.6 us`, retained-scroll 900x620 copy is about `92.0 us`, and fast key-settings lookup is about `2.2 ns`.
- Current native renderer baseline from `perf_results/profile-20260524-174000.md`: Blend2D 1280x720 stress render with 84 keys, bloom/glass/fill/borders, and simple label strokes is about `6.33 ms/frame` on this CPU. This is not a Chromium/OBS-equivalent workload, but it is the first reproducible baseline for a future native overlay renderer.
- Current input-latency simulation baseline from `perf_results/bench-20260525-001246.md`: simulated SparkLink-to-XUSB CPU work is about `3.13 us` per 1 kHz realtime tick. The modeled row-scan delay from raw 50% press depth to XUSB visibility is p50 about `3.2 ms`, p95/p99/max about `5.2 ms` with six paced matrix rows.
- Current production input-to-overlay baseline from `build/evidence/input-pipeline-profile/20260802-113109`: on 12 logical processors with the existing 1 ms setting, the full HallJoy tree is 0.809% machine CPU and the full unthrottled headless Chrome tree is 5.451%; Spark is 0.656%, realtime 0.017%, and UI/short-lived HTTP workers 0.119%. The exact final-artifact 8 ms comparison is 0.721% HallJoy and 3.747% Chrome. These are physical Irok and repeatable upper-pressure browser measurements, not exact OBS or unavailable-keyboard claims.
- First short-tap risk check: with a deliberately tiny synthetic press window, six-row pacing captures about `63%` of modeled taps. This is not a normal gameplay number, but it is a useful warning metric: future SparkLink pacing work should improve this without increasing firmware pressure or input jitter.
- UI scroll pass: `Input Overlay` is being migrated to a single-HWND custom surface. The current custom pass stores the whole page in a retained offscreen bitmap and scrolls by copying the visible slice, so wheel/scrollbar movement does not rerender every label, slider, and color picker frame.

## Optimization Backlog

- Audit `Backend_Tick` for any work that scales with all 255 HID slots every millisecond; debug-only scans should stay gated and cheap.
- Expand input-latency simulation from the current SparkLink row-scan model into separate cases: best-case same-row press, worst-case just-missed-row press, many simultaneous presses, button-only bindings, analog axis bindings, and multi-device mixed SparkLink/Sayo/SDK paths.
- Use real SparkLink telemetry from Irok MG75 Max to validate the simulator: compare live route Hz/full-matrix Hz/max row age against the modeled six-row pacing, then tune request scheduling from measured data.
- Keep overlay/OBS rendering and logging away from the physical keyboard input path.
- Review SparkLink request pacing and timeout behavior under device disconnect/reconnect so stability does not depend on perfect HID responses.
- Profile settings/UI paint paths separately from input paths; UI smoothness matters, but it must not compete with keyboard event processing.
- Move more hot logic into small testable functions so the perf harness can measure backend tick pieces, SparkLink packet parsing, overlay state JSON generation, and canvas/sprite-setting serialization separately.
- Overlay runtime perf logs now include browser-side sprite and label rasterization time on their own cache misses, and the analyzer reports p95/p99/max so short visual spikes are visible. Add the next layer of isolated work around actual SparkLink USB/HID transaction latency and backend report building against production bindings before rewriting those paths. Based on the first SparkLink baselines, USB/HID transaction latency and timeout behavior are more likely to matter than row parsing or scheduler CPU cost.
