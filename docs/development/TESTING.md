# HallJoy testing

Current workflow: 2026-10-31. Run from HallJoy-main. Read
[active context](../current/OWNER_CONTEXT.md) and [build guide](BUILD_README.txt).
Scripts define current gates; old release-readiness records describe their candidates.

## Ordinary source change

Run each once, with the exact-input test cache enabled:

```powershell
python tools/run_native_backend_checks.py --require-compiler
powershell -NoProfile -ExecutionPolicy Bypass -File ./tools/build_release.ps1
```

Never `HALLJOY_NO_TEST_CACHE=1`; do not repeat PASS without a relevant change.
`research_reference_checks.py --record` only follows a changed pinned source.
Full dependency rebuilds use BUILD.cmd, which already runs the unified gate;
do not duplicate the full and incremental workflows merely to repeat checks.

The native runner validates static contracts and portable C++ behavior. The release
wrapper builds a complete candidate, runs linked-image, legal-resource,
production-profile/layout and startup recovery checks before installation. It
enforces the independent full native catalog and Supported layouts. The publisher
closes/restores only the exact installed app when necessary:
[build lifecycle](../current/BUILD_LIFECYCLE_2026-09-27.md).
For source-only investigations, --static-only is available; it does not replace
compilation of changed C++. Keep diagnostics isolated from real user profiles.

## Documentation and workflow checks

`python tools/project_context.py --check` checks active local links, required
entry points and bounded startup context without launching HallJoy.
`--topic` selects the source map; `--search` locates historical decisions;
`--hash-exe` reads the actual delivery hash. Local release state is not a live
GitHub assertion. Workflow regressions:

```powershell
python -m unittest discover -s tools/tests -p test_project_context.py
python src/HallJoyProject/tests/s20_build_docs_static_audit.py
```

These checks do not establish app behavior or skip mandatory source checks.

## Targeted regression evidence

- [Input lifecycle](../current/INPUT_LIFECYCLE_REVIEW_2026-09-20.md): join producers
  before shared-state reset on Pause / failed Resume.
- [Profiles](../current/PROFILE_PERSISTENCE_REVIEW_2026-09-20.md): isolated save,
  migration, concurrency, interrupted files and startup recovery.
- [Automatic layout](../current/AUTOMATIC_LAYOUT_REVIEW_2026-09-20.md): coherent
  evidence, ambiguity, hotplug and manual isolation; inspect newer model fixes.
- [Diagnostics](SUPPORT_DIAGNOSTICS_CONTRACT.md): telemetry coverage, writer policy
  and scoped capture/trace exceptions.
- [Support sync](SUPPORT_STATUS_SYNC.md): live Sheet, full yellow reconciliation,
  Supported coverage, structure and base/effective colors.

Narrow checks include tools/generate_attackshark_family.py --check and the existing
protocol tests. Choose checks for a concrete failure hypothesis. No speculative
long soak is required.

## Hardware and visual limits

Software PASS is not USB timing, every firmware revision, sensor resolution or
gameplay proof. The owner evaluates visual behavior; the agent checks code and
automated behavior, not visual runs. Tester feedback and emulation are separate
evidence. Record model/transport/firmware limits in
[SUPPORTED_HARDWARE](../SUPPORTED_HARDWARE.md). Physical testing per model is not an
automatic Supported gate when protocol and implementation are established. Do not
invent testing or promote unresolved paths based on a suite alone.
Do not infer a blanket ATTACK SHARK freeze from a superseded September guide;
read exact current model records and owner decisions.
Continuous logging is optional; required incidents still log when OFF. Limited/
unstable banners do not themselves force logs. Tester flow: one ordinary EXE,
HallJoy.log / Open log.

## CI, publication and history

.github/workflows/native-backend-checks.yml runs automatic checks on Windows
for push/PR. Its production build job is manual workflow_dispatch. Release tooling
runs one local suite/build; CI supplies clean-checkout checks. Local builds are not
publication authorization. Check publication inputs in the cleaned Git checkout,
not the extracted workspace.
[Historical guide](../archive/TESTING_BEFORE_2026-09-20.md) retains older qualification
and soak evidence. Those dates/targets are not additional release gates.
