# HallJoy v1.4 private UAP runtime

## Runtime contract

HallJoy embeds the exact ABI1 Universal Analog Plugin used by its isolated
analog-host child. It does not load or require a system-wide Wooting Analog SDK
and does not consume a global `C:\Program Files\WootingAnalogPlugins` install.

At startup HallJoy:

1. compares the portable DLL beside the executable with the embedded resource;
2. atomically writes and flushes the resource when the portable location is
   writable;
3. falls back without elevation to
   `%LOCALAPPDATA%\HallJoy\Runtime\v1.4.0\HallJoyUniversalAnalogHost.dll` when
   the executable directory is protected;
4. compares the final file byte-for-byte with the resource;
5. passes the verified absolute path explicitly to the isolated child process.

A stale, truncated, or modified runtime copy is replaced atomically. Temporary
files include the process and thread IDs, and are moved into place only after a
complete write and successful `FlushFileBuffers`.

## Dependencies and diagnostics

ViGEmBus remains a system driver dependency for virtual Xbox controllers.
HallJoy may offer to download its trusted installer when the driver is missing.

Private UAP failures never offer a system Wooting SDK or global UAP installer,
because those installations are outside the runtime path HallJoy actually
loads. Diagnostics identify the embedded runtime condition and the selected
path. Native protocol backends may continue independently when their hardware
passes capability validation.

## Parent generation containment

The main process owns the analog-host snapshot bridge, supervisor, IPC mapping,
events and child job as one lifecycle generation. It releases those resources
only after both parent workers have joined. Partial startup follows the same
rule: if the supervisor cannot start, the bridge is stopped and joined before
IPC rollback.

Final shutdown uses bounded graceful and child-job containment phases. Failure
to join a parent worker retains reachable resources, blocks reinitialization
and propagates failure through backend/application shutdown so static state is
not destroyed beneath a live worker.

V14-07B adds separate C++ and SEH boundaries to the snapshot bridge,
supervisor and isolated child entry. Parent faults publish a neutral error
state, signal the bridge/child stop events and permanently block restart. Child
faults publish the same neutral shared state before process exit. The
supervisor requires job assignment and never replaces a child until the old
process HANDLE is signaled; an unconfirmed exit retains that HANDLE and blocks
overlapping generations.

V14-07C protects the private plugin itself. Throwing C exports enter a common
catch-all barrier, publish a fixed transport fault and poison restart. Soup
mutexes use scope-bound guards, state queries reflect the active generation,
and pointer/length exports reject invalid input before access. Plugin unload
uses one deadline, snapshots device worker pointers under the devices mutex,
then cancels and joins them after releasing that mutex. The child host prefers
the optional `halljoy_unload_bounded` export; an incomplete join deliberately
ends the disposable child without unloading its DLL or destroying CRT state
beneath a live worker.

## Verification

The development verification build accepts
`--halljoy-test-uap-exe-write-denied`. The switch is compiled only when
`HALLJOY_STABILITY_TRACE` is enabled and forces the protected-directory fallback
without requiring administrator rights.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\run_analog_simulator.ps1 `
  -ForceUserUapRuntime
```

The gate requires an exact-resource-match trace, successful child/backend and
ViGEm initialization, the full simulated common-pipeline scenario, graceful
shutdown, and no remaining process. It is runtime/dependency evidence, not
analog-keyboard hardware evidence.

The official build also runs `tools/check_private_uap_abi.py` against the newly
built ABI1 DLL. It verifies the ABI version, truthful state before/after
initialization and unload, null-buffer behavior, bounded unload and idempotent
cleanup before the DLL is embedded into HallJoy.
