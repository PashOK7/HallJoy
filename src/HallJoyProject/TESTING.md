# Testing and Performance Checks

## V11.0.2 safety hotfix checks

Run the source invariants and portable overlapped-I/O lifecycle test first:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_safety_hotfix_tests.ps1
```

The source checker covers HJ-P0-001 through HJ-P0-007. When `cl.exe` is available, the runner also builds and executes `tests\hid_io_operation_lifecycle_test.cpp` with `/W4 /WX`. These checks do not replace an MSVC application build, Application Verifier, reconnect soak, or physical-device validation.

HallJoy now has a small native console test target: `HallJoyTests`.

For day-to-day optimization work, use the benchmark runner first:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\bench.ps1
```

It builds `HallJoyTests`, runs perf probes several times, and writes ignored reports to `perf_results/`:
- `bench-*.json` for machine-readable comparisons;
- `bench-*.md` for quick review;
- `bench-*.raw.txt` for the original console output.

For `Release|x64`, the runner calls `tools\ensure_blend2d.ps1` before building. If `third_party\blend2d` is missing, the script clones Blend2D and builds a static `/MT` `blend2d.lib` for the native renderer benchmark.

Run all tests and performance probes:

```powershell
.\tools\run_tests.ps1
```

If PowerShell blocks local scripts on the machine, run the same script with a one-shot bypass:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_tests.ps1
```

Run only unit/smoke tests:

```powershell
.\tools\run_tests.ps1 -UnitOnly
```

Run only performance probes:

```powershell
.\tools\run_tests.ps1 -PerfOnly
```

Build a privacy-safe Release diagnostic executable for an end-user crash:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_diagnostic.ps1
```

The output is `x64\Diagnostic\HallJoyDiagnostic.exe`. After reproduction, ask
for `HallJoyDiagnostic.log` and `HallJoyDiagnosticCrash.txt`. See
`DIAGNOSTIC_SUPPORT.md`.

Current coverage:
- `CurveMath` unit checks for clamping, endpoints, identity behavior, and monotonic finite output;
- `CurveMath::EvalRationalYForX` microbenchmark with `PERF ... ns_per_op` output;
- `BackendCurve` cached per-key curve application benchmark;
- `BackendCurve` 255-HID full-tick benchmark, reported as ns per full 255-key scan;
- `KeySettings_GetUseUnique` fast snapshot lookup benchmark for the realtime path;
- HSV color-picker palette pixel generation benchmark for UI color controls;
- retained-scroll memory-copy benchmark that approximates the cost of copying a cached page slice during high-refresh scrolling;
- SparkLink route-row parsing benchmark for the 21-column row packet path used by Irok MG75 Max;
- overlay `/state` JSON generation benchmark with an 84-key stream layout;
- overlay sprite/label cache-key calculation plus cache lookup benchmark;
- SparkLink missed-HID diagnostic tick simulation across 255 HIDs;
- SparkLink route-row pacing scheduler simulation;
- synthetic backend gamepad report building plus report-change detection benchmark;
- synthetic input-latency simulation from SparkLink-style HE scan to XUSB report visibility, including p50/p95/p99/max simulated delay;
- short-tap capture simulation for SparkLink-style row pacing, reporting captured/missed synthetic taps without failing the suite;
- SparkLink timeout/reconnect policy state-machine benchmark.
- native Blend2D 1280x720 overlay render benchmark with an 84-key synthetic layout, used for browser-free render-path profiling.

Run the broader local profiling wrapper:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\profile.ps1
```

It runs `tools\bench.ps1`, records CPU/OS context, writes `perf_results/profile-*.md`, and can include runtime log analysis:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\profile.ps1 -LatencyLog .\x64\Debug\log.txt -OverlayPerfLog .\x64\Release\overlay_perf.log
```

Agent workflow:
- run `tools\bench.ps1` before hot-path rewrites when possible;
- apply one scoped optimization;
- run `tools\bench.ps1` again;
- compare `avg_ns_per_op`, `min_ns_per_op`, and `max_ns_per_op`;
- keep the change only if correctness still passes and the measured result supports it, or if the change improves stability/latency risk for a clearly documented reason.

Measured wins:
- `CurveMath::EvalRationalYForX`: local Release x64 average improved from about `322.73 ns/op` to `222.09 ns/op` after reducing work inside the binary-search loop.
- `BackendCurve` cache invalidation: changed from per-tick invalidation to event-driven invalidation on curve setting changes. The harness now includes `backend_curve.apply_smooth_cached` so future curve/cache rewrites have a direct realtime-path benchmark.
- UI scroll pass: manual visual validation is required. Check `Input Overlay`, `Mouse settings`, `Configuration`, and `Remap` for smoother wheel/drag scrolling, no ghost trails, no stale text, no clipped controls, and correct scrollbar thumb tracking.

Current local baseline:
- `perf_results/profile-20260524-165950.md` includes SparkLink pacing/timeout, missed-HID diagnostic, overlay-state, overlay sprite-key, retained-scroll, synthetic backend report baselines, and p95/p99 runtime overlay log analysis.
- `perf_results/profile-20260524-174000.md` adds the first browser-free native renderer baseline: `overlay.blend2d_native_frame_84_keys` averages about `6.33 ms/frame` for a 1280x720/84-key stress scene on this CPU.
- `perf_results/bench-20260525-001246.md` adds the first input-latency simulation baseline: `input_latency.sparklink_to_xusb_1khz_6row` costs about `3.13 us` of CPU work per 1 kHz tick, with modeled SparkLink row-scan delay p50 about `3.2 ms` and p95/p99/max about `5.2 ms`.
- `input_latency.sparklink_short_tap_capture_6row` is a warning metric for deliberately tiny press windows; the first local check captured about `63%` of modeled short taps with six-row pacing.
- SparkLink 21-column route-row parse is currently about `240 ns` per row, route pacing is about `2.4 ns` per scheduler tick, and timeout policy state update is about `1.9 ns` per tick, so raw row parsing/scheduling are not the first optimization targets unless future code regresses them.
- Missed-HID diagnostic scanning is currently about `0.67 us` per 255-HID tick when enabled in the simulation; keep the feature gated and avoid logging from the normal path.
- Overlay `/state` generation for an 84-key layout is currently about `10.8 us`; overlay sprite/label cache-key lookup is about `0.52 us` per key; synthetic backend report building is about `171 ns` per pad.
- Input-latency simulation prints both CPU cost per realtime tick and simulated press-to-report delay. Treat the delay percentiles as a model of SparkLink row pacing, not as a full end-to-end hardware/ViGEm/game/monitor measurement.
- Full 255-HID curve tick is currently about `66.6 us`; retained-scroll 900x620 slice copy is about `92.0 us`; fast key-settings lookup is about `2.2 ns`.
- Runtime overlay analysis now reports p95/p99/max for server build, browser fetch, browser render, browser layout, and sprite/label cache misses. Use the p99/max numbers to find visible spikes; averages alone are not enough for 240 Hz visual tuning.
- Overlay runtime perf logs now include browser-reported sprite and label rasterization time on cache misses: `sprite_misses`, `label_misses`, `sprite_build_us_avg`, `label_build_us_avg`, `sprite_build_us_total`, and `label_build_us_total`.

Runtime latency logs:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\analyze_latency_log.ps1 -Path .\x64\Debug\log.txt
```

Use this after a manual gameplay/OBS run to summarize `[rt.stats]` tick jitter and `[overlay.perf]` samples. Release builds do not write the normal debug `log.txt`; overlay perf can still be analyzed from `overlay_perf.log` if present next to `HallJoy.exe`. For overlay work, clear the old `x64\Release\overlay_perf.log`, run HallJoy with the OBS Browser Source open for at least 30-60 seconds, press many keys at once, then run the analyzer or `tools\profile.ps1`.

Real SparkLink HE telemetry:
- open the `Configuration` tab with a SparkLink keyboard connected;
- read the live `SparkLink` lines under the debug toggles: route Hz, matrix Hz, avg/max route interval, last route age, max row age, ok/fail counts;
- use `HE poll mode` to compare `Safe`, `Fast yield`, and `Max burst`. Watch for higher route/matrix Hz, lower max row age, and still-zero fail count;
- use `Rows` only as an experiment. `Auto` polls all discovered rows; setting fewer rows can improve measured matrix Hz but keys on skipped rows will not get live analog depth;
- open the `Gamepad Tester` tab to see the product-facing analog telemetry block. SparkLink shows route Hz, full-matrix estimate, route transaction time, mapped key count, and observed raw full-scale range. SayoDevice shows depth response Hz and raw depth resolution. Wooting SDK shows HallJoy's SDK polling target and normalized HallJoy output resolution;
- use `route_tx_avg_us` / `route_tx_max_us` from the live SparkLink UI to separate keyboard/protocol request-response time from HallJoy pacing overhead. For a six-row keyboard, true 1000 Hz per-row matrix updates would require roughly `6000 route_hz`, or under `166 us` per route transaction before any scheduling overhead;
- compare these live numbers with `input_latency.sparklink_to_xusb_1khz_6row` before changing SparkLink pacing.

For a focused overlay capture:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\overlay_runtime_capture.ps1 -DurationSec 60
```

The capture script clears the previous Release overlay log, waits while the OBS/browser overlay is running, then writes `perf_results/overlay-runtime-*.md` with p95/p99/max timing for server build, browser fetch/render/layout, and sprite/label cache misses. Add `-StartHallJoy` and `-OpenBrowser` when you want the script to launch the Release exe and open the overlay URL itself.

For an autonomous OBS-free overlay render stress test:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\overlay_synthetic_probe.ps1 -DurationSec 30 -ActiveKeys 32
```

This starts the Release HallJoy exe with `--overlay-server --minimized`, opens the overlay in a headless Edge/Chrome page with `?synthetic=1`, emulates many HE key depths inside the overlay page, warms up the sprite/label caches, then writes `perf_results/overlay-synthetic-*.md`. The script uses a temporary browser profile with extensions, sync, notifications, and background networking disabled, then terminates processes tied to that temporary profile. It isolates browser/canvas render cost from OBS and physical keyboard input.

Do not use the browser synthetic probe as the default agent test path. It can still wake Edge background components on some Windows setups. Prefer `tools\bench.ps1` / `tools\profile.ps1` and the native Blend2D benchmark unless browser-specific canvas behavior is the exact target.

Performance budgets are optional. For example:

```powershell
$env:HALLJOY_PERF_BUDGET_NS_CURVE = "250"
.\tools\run_tests.ps1 -PerfOnly
```

The first goal is not to pretend that one microbenchmark proves the whole app is fast. The goal is to build a habit: every time a hot path is isolated into a pure function or testable module, add a correctness test and a small benchmark for it.
