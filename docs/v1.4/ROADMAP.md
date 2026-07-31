# HallJoy v1.4 roadmap

## Release objective

v1.4 combines the mature user interface and self-contained dependency lessons
from v1.3 with the native protocol, isolation, scheduling, and stability work
from the imported advanced archive. The release must not require a system-wide
Wooting Analog SDK installation.

The word "final" is not used before every release gate in this roadmap passes.

## Package rules

- One package addresses one coherent risk area.
- Protocol bytes, mappings, timing, lifecycle, and UI changes are not mixed
  without characterization tests.
- Every package has a local rollback commit.
- Every package updates the authoritative v1.4 documentation.
- Static audits supplement behavioral tests; they do not replace them.
- No push, merge to `main`, tag, or GitHub Release occurs without explicit
  approval.

## Ordered packages

| Package | Scope | Status | Required completion gate |
|---|---|---|---|
| `V14-00` | Preserve v1.3 SDK work; import and verify the advanced archive | Verified | Provenance commits, clean tree, baseline build and test evidence |
| `V14-01` | Product identity, version resources, current README and documentation ownership | Planned | No active product surface reports 3.9.0; historical evidence remains intact |
| `V14-02` | Development-only deterministic analog simulator and scenario runner | Planned | Full common pipeline, ramps, SOCD, hotplug, disconnect and fault scenarios without production enablement |
| `V14-03` | Self-contained private UAP runtime and truthful dependency diagnostics | Planned | Works from writable and protected install locations; never recommends system SDK |
| `V14-04` | Reproducible dependencies, warning baseline, local/CI-equivalent build scripts | Planned | Pinned inputs, x64 Release CI, portable tests, documented warning policy |
| `V14-05` | Truthful lifecycle registry and cooperative worker shutdown | Planned | Failure-injected start/stop/restart tests; no unsafe ordinary `TerminateThread` path |
| `V14-06` | Addressed, MAD68, Hex80, SparkLink and Sayo lifecycle migration | Planned | Per-backend tests plus unchanged protocol and mapping characterization |
| `V14-07` | Analog host and UAP ABI generation, exception, unload and restart safety | Planned | Partial-start, crash, hang, unload and bounded restart tests |
| `V14-08` | Startup transaction, wake correctness and ViGEm output isolation | Planned | Reverse-order rollback, no lost wake, stalled-driver and report-equivalence tests |
| `V14-09` | Transactional persistence and writable state migration | Planned | Fault-injected atomic-save tests and safe `%LOCALAPPDATA%` migration |
| `V14-10` | IPC and overlay security/correctness | Planned | ACL, spoofing, framing, overflow, origin, concurrency and shutdown tests |
| `V14-11` | UAP pacing, identity, modularization and measured performance | Planned | CPU/USB/latency comparison with no unsupported sampling regression |
| `V14-12` | Release qualification and hardware matrix | Planned | Clean package, 8-24h soak, reconnect cycles and required device-owner gates |

## Completed package: V14-00

Completed:

- v1.3 self-contained SDK work preserved on
  `checkpoint/v1.3-self-contained-sdk` at `b3fefce`.
- Advanced archive imported byte-for-byte on `v1.4-integration` at `f5e8c18`.
- Original ZIP SHA-256 recorded in the worklog.
- Imported x64 Release build reproduced successfully before integration.
- establish the authoritative v1.4 documentation set;
- x64 Release build reproduced from `v1.4-integration`;
- all supplied static audits passed;
- all portable C++20 tests passed with Clang 19.1.5;
- ViGEmBus 1.22.0 installed from the verified winget/GitHub release;
- UI launch, ViGEm initialization, main shutdown, and child shutdown passed.

The trace verdict is `WARN`, not `FAIL`, because this workstation has no
SparkLink device. It proves generic startup/shutdown and ViGEm lifecycle only.
GCC evidence is inherited from the byte-identical archive baseline and will be
rerun in CI before publication.

## Next packages

`V14-01` establishes the v1.4 product identity. `V14-02` then adds a
development-only deterministic analog simulator so the common analog pipeline
can be exercised on this membrane-keyboard workstation. Simulation never
closes a protocol-specific hardware gate.

## Release definition

v1.4 can be tagged only when:

1. all P1 risks in the v1.4 register are `Verified` or explicitly excluded with
   a release-blocking decision;
2. all automated Windows and portable CI gates pass from a clean checkout;
3. the application starts without a system Wooting SDK;
4. dependency failures produce truthful, actionable diagnostics;
5. settings and profiles survive injected write failures;
6. the required device matrix and long-run qualification are complete;
7. the release archive, executable, sources, notices, and evidence have recorded
   SHA-256 values.
