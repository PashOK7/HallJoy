# HallJoy — RM-01 runner side-effect registry

Recorded from static source inspection on 2026-09-06. Status: **IMPLEMENTED
PARTIAL**.
The user has since explicitly permitted controlled HallJoy runtime.  This does
not make every output-capable runner generally safe: each still needs its named
RM gate and declared device boundary.

## Classification contract

- **File-only candidate**: a reviewed command branch returns before
  `Backend_Init` and window creation. It may still create its explicitly owned
  temporary files and initialise ordinary diagnostic/runtime files before
  `App_Run`; it is not a general zero-side-effect promise.
- **Process without gamepad/HID by reviewed branch**: a command exits before
  `App_Run`, but starts the EXE and its pre-`App_Run` setup.
- **Output-capable**: starts the normal app/simulator or a ViGEm path. Treat it
  as capable of virtual-controller, HID, window, or injected-input effects.
- **Real-device/HID**: an exact device/UAP path is required; it is not file-only
  even if the tested branch does not intentionally publish a gamepad report.

Every script writes only its named temporary/evidence root or a copied test EXE
when cleanup is implemented; this was inspected statically, not exercised.

| Runner | Static class | Process / external boundary | Current permission |
|---|---|---|---|
| `run_analog_simulator.ps1` | Output-capable | builds/starts `HallJoyV14Simulator.exe` with `--halljoy-simulate-analog=script`; may start overlay/Python | Do not run |
| `run_drunkdeer_abnormal_exit_log_test.ps1` | Output-capable / diagnostic | starts copied DrunkDeer diagnostic normally | Do not run |
| `run_drunkdeer_diagnostic_smoke.ps1` | Output-capable / diagnostic | starts copied DrunkDeer diagnostic normally | Do not run |
| `run_drunkdeer_log_growth_test.ps1` | Process without gamepad/HID by reviewed branch | `--halljoy-test-drunkdeer-log-growth` returns in `main.cpp` before `App_Run` | Not run; file/log effects remain |
| `run_factory_reset_test.ps1` | Output-capable | invokes AnalogSimulator paths | Do not run |
| `run_gravastar_v75_diagnostic_smoke.ps1` | Real-device/HID and output-capable | starts copied V75 diagnostic normally | Do not run |
| `run_input_pipeline_profile.ps1` | Output-capable | starts a hash-verified portable copy of the ordinary EXE plus browser/overlay profiling; requires a real supported UAP input route and ViGEm publication, while native SparkLink telemetry is optional; live `%LOCALAPPDATA%\\HallJoy` is manifest-verified unchanged | User has permitted HallJoy runtime; execute only as the RM-33 controlled profile |
| `run_irok_nd75_smoke.ps1` | Real-device/HID and output-capable | starts copied diagnostic normally | Do not run |
| `run_keyboard_shutdown_matrix.ps1` | Output-capable | drives analog-simulator fault scenarios | Do not run |
| `run_long_soak.ps1` | Output-capable | hash-verified portable copy of the ordinary EXE, optional overlay and scoped sleep prevention; live `%LOCALAPPDATA%\\HallJoy` is state-verified | User has permitted HallJoy runtime; execute only as the RM-33/RM-34 controlled soak |
| `run_production_smoke.ps1` | Output-capable | hash-verified portable copy of the requested ordinary EXE, copied profile state and optional overlay; live `%LOCALAPPDATA%\HallJoy` gets before/after manifest comparison | User has permitted HallJoy runtime; executed once as controlled RM-34 overlay evidence |
| `run_profile_transaction_tests.ps1` | File-only candidate | simulator profile transactions and rejected startup-only in a GUID TEMP root | Not run; eligible only after fail-closed guard verification |
| `run_provider_v2_qualification_smoke.ps1` | Output-capable | delegates to `run_production_smoke.ps1` | Do not run |
| `run_release_qualification.ps1` | Output-capable | repeated lifecycle of a hash-verified portable copy of the ordinary EXE; live `%LOCALAPPDATA%\\HallJoy` is state-verified | User has permitted HallJoy runtime; execute only as controlled RM-34 qualification |
| `run_storage_migration_test.ps1` | Output-capable | delegates to analog simulator, even with isolated synthetic input | Do not run |
| `run_uap_provider_v2_dual_capture_smoke.ps1` | Real-device/HID | exact EXE UAP dual capture before `App_Run` | Do not run |
| `run_ui_scroll_stress.ps1` | Output-capable | ordinary release EXE plus UI message injection | Do not run |
| `run_vigem_output_real_child_test.ps1` | Output-capable | exact simulator real ViGEmBus child and X360 PnP check | Do not run |
| `run_vigem_output_self_host_test.ps1` | Output-capable | simulator self-host/runtime-stress ViGEm output | Do not run |

## Verified source control flow

`App_Run` handles profile transactions immediately after `AppPaths_Initialize`,
before factory-reset work, profile/window setup, and `Backend_Init`. The rejected
startup-only flag returns after profile loading but before window creation and
`Backend_Init`. `Backend_Init` itself immediately stops/reconfigures output and
native workers, so it is the mandatory fail-closed boundary for both modes.

`wWinMain` is not a zero-side-effect route before `App_Run`: it initialises
stability/debug logging and calls `EmbeddedAnalogStack_Prepare`. The source
review found no normal HID/backend start before `App_Run`, but this needs an
explicit test guard rather than an inference from order alone.

## Implementation decision pending

| Option | Assessment |
|---|---|
| A: document the intended early return only | Rejected: a later refactor can move backend startup above the return silently. |
| B: command-scoped fail-closed guard at `Backend_Init`, passed by the file-only runner | Chosen and implemented: local, production-inert absent the test flag, and makes a moved backend call fail before output/HID lifecycle. |
| C: separate executable/test project | Deferred: stronger isolation but duplicates production startup and does not by itself prove the actual branch remains early. |

The file-only runner now passes `--halljoy-test-forbid-backend-init`; `Backend_Init`
records `backend/test.forbidden_backend_init`, increments a simulator-only
process-local oracle, and returns false before output/HID lifecycle. Both tested
app branches require the oracle to remain zero; the production-linked profile
test records `backend_init_attempts=0`. This was statically checked only; no EXE
was built or run.

The already injected `VigemApiV1` fake owns its own target-allocation/update
call records. `SendInput` has no implementation in this repository. A global
`CreateTarget`/HID-open counter would be incomplete and misleading because HID
opens are deliberately spread over independently owned protocol modules.
Future fake-transport harnesses must instead expose zero-valued counters at
each relevant injected seam, and must not be reclassified from output-capable
based solely on their name or a synthetic WASD replacement.
