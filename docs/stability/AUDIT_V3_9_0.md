# HallJoy v3.9.0 initial stability and performance audit

Audit baseline: July 30, 2026. This document records the imported v3.9.0 state,
not the final v1.4 result. Current dispositions are maintained in
`RISK_REGISTER.md` and the v1.4 release register.

## Outcome

The application had substantial protocol work, fail-closed native/UAP routing,
portable parser tests, and useful process isolation, but it was not yet safe to
describe as fully stable. The highest risks were lifecycle ownership,
untruthful/bounded shutdown, uncaught worker exceptions, synchronous driver work
inside realtime, IPC capability design, and persistence that could lose the
last valid file.

The audit registered 45 findings: 16 P1, 21 P2, and 8 P3. It recommended small
characterized packages rather than a broad rewrite. No finding was considered
fixed merely because a plausible code change existed.

## Evidence and limitations

The imported archive and source manifest were hashed before documentation
changes. Existing Python audits and portable C++ tests were run under the
available compilers. Protocol and project structure were inspected across
HallJoy, native backends, the isolated analogue host, UAP/Soup integration,
ViGEm, overlay HTTP, IPC, and writable storage.

The original environment did not provide a complete MSVC/Windows runtime,
physical MAD68/Hex80/Addressed matrix, driver-stall injection, Application
Verifier/PageHeap, or long production soak. Findings based on those boundaries
were assigned explicit later gates and never promoted through local static
evidence alone.

## P1 findings

| ID | Initial finding |
|---|---|
| `HJ-AUD-P1-001` | normal shutdown used `TerminateThread`, allowing resource ownership to outlive destroyed state |
| `HJ-AUD-P1-002` | three native stop paths claimed a bound but could still reach an unbounded join |
| `HJ-AUD-P1-003` | boolean backend stop callbacks could report completion without proving a generation ended |
| `HJ-AUD-P1-004` | important worker entries allowed C++ exceptions to escape across OS/C boundaries |
| `HJ-AUD-P1-005` | Addressed could close a HID handle from another thread while stack-backed overlapped I/O remained live |
| `HJ-AUD-P1-006` | analogue-host initialization could overlap an unreaped supervisor/bridge generation |
| `HJ-AUD-P1-007` | analogue-host partial-start rollback could unmap IPC below a live bridge thread |
| `HJ-AUD-P1-008` | manual-reset device-change wake logic allowed a reset/wait race and lost notification |
| `HJ-AUD-P1-009` | startup continued after required realtime or native phases failed |
| `HJ-AUD-P1-010` | realtime called ViGEm synchronously, so a driver stall could stop input and shutdown |
| `HJ-AUD-P1-011` | mouse IPC checked mapping creation after another API call had overwritten the disposition signal |
| `HJ-AUD-P1-012` | named analogue-host IPC objects were precreatable/spoofable within the same user session |
| `HJ-AUD-P1-013` | built-in dependency installation had TOCTOU and incomplete provenance verification |
| `HJ-AUD-P1-014` | an installer process/wait could freeze the UI indefinitely |
| `HJ-AUD-P1-015` | UAP C exports lacked exception containment and used manual locking |
| `HJ-AUD-P1-016` | UAP unload joined without a bound while holding the global devices mutex |

## P2 findings

| ID | Initial finding |
|---|---|
| `HJ-AUD-P2-001` | overlay accepted and served one client synchronously |
| `HJ-AUD-P2-002` | overlay HTTP parsing did not implement incremental message framing |
| `HJ-AUD-P2-003` | overlay numeric parsing allowed unsigned overflow and ambiguous keys |
| `HJ-AUD-P2-004` | overlay endpoint accepted arbitrary browser origins |
| `HJ-AUD-P2-005` | mouse IPC read peer interlocked fields through ordinary volatile loads |
| `HJ-AUD-P2-006` | UAP polling workers had no explicit success cadence or failure backoff |
| `HJ-AUD-P2-007` | UAP identity for identical devices depended on unstable enumeration order |
| `HJ-AUD-P2-008` | plugin `is_initialised()` returned true regardless of lifecycle |
| `HJ-AUD-P2-009` | `_device_info` did not reject a null destination buffer safely |
| `HJ-AUD-P2-010` | snapshot export held the global device mutex while copying every value |
| `HJ-AUD-P2-011` | settings replacement could destroy a good file with an incomplete temporary file |
| `HJ-AUD-P2-012` | overlay settings wrote directly and reported success without verifying persistence |
| `HJ-AUD-P2-013` | profile saves ignored stream failure after flush/close |
| `HJ-AUD-P2-014` | layout saves truncated and rewrote the destination non-atomically |
| `HJ-AUD-P2-015` | curve-preset writes ignored result and readback |
| `HJ-AUD-P2-016` | many save callers ignored returned errors |
| `HJ-AUD-P2-017` | mutable state defaulted to the executable directory |
| `HJ-AUD-P2-018` | profile/preset names lacked complete Unicode, reserved-name, and collision policy |
| `HJ-AUD-P2-019` | curve-cache generation used an insufficient memory-order contract |
| `HJ-AUD-P2-020` | global lifecycle registry lacked serialization and owner-thread enforcement |
| `HJ-AUD-P2-021` | native ownership at VID:PID granularity could steal a sibling interface |

## P3 findings

| ID | Initial finding |
|---|---|
| `HJ-AUD-P3-001` | `TESTING.md` described commands/test flow that no longer existed |
| `HJ-AUD-P3-002` | internal README/build guidance mixed several obsolete generations |
| `HJ-AUD-P3-003` | an Addressed audit checked an architecture that production no longer used |
| `HJ-AUD-P3-004` | validation prose overstated what lifecycle tests had actually proven |
| `HJ-AUD-P3-005` | a Win32 configuration referenced the x64 ViGEm library |
| `HJ-AUD-P3-006` | production used Warning Level 3, hiding useful compiler evidence |
| `HJ-AUD-P3-007` | public build instructions omitted actual toolchain dependencies |
| `HJ-AUD-P3-008` | the Sun build dependency followed mutable branch state |

## Architectural diagnosis

### Lifecycle state was distributed

Running booleans, thread handles, stop events, and backend-specific cleanup
encoded one implicit state machine in several globals. The audit recommended a
generation-aware owner with explicit `Starting`, `Running`, `StopRequested`,
`Joined`, `Faulted`, and `Poisoned` states plus a structured stop result.

### Realtime and driver-facing work were coupled

The same path consumed input, applied curves/remapping, and called ViGEm. A slow
driver therefore threatened both latency and shutdown. The recommended model
was a latest-value mailbox feeding a dedicated bounded output owner, preserving
report equivalence and neutral recovery.

### Large modules mixed unrelated ownership

UI, persistence, protocol, worker, and diagnostic behavior crossed large
translation units and include fragments. Modularization was recommended only
after characterization tests, one ownership boundary at a time, without mixing
it with protocol changes.

### Persistence needed one transaction API

Settings, profiles, layouts, curves, overlay, and bindings used inconsistent
write patterns. The target contract was a same-directory unique temporary file,
checked write/flush/close, schema and semantic readback, atomic replacement,
explicit failure stage, and preservation of the known-good destination.

### Process isolation was valuable but incomplete by itself

Keeping UAP in a child process protected the UI from plugin failure. It did not
make IPC, parent generation ownership, plugin unload, or HID lifetime safe. The
audit recommended preserving isolation while making both sides' lifecycles
truthful and capability-bound.

## Missing validation at baseline

- Windows lifecycle fault injection for every start/stop/rollback phase;
- repeated startup/shutdown, hotplug, sleep/resume, and stuck-driver scenarios;
- long production soak with handles, CPU, memory, rate, and immutable user
  state;
- Application Verifier/PageHeap or equivalent ownership diagnostics;
- transactional persistence failure at every stage;
- fragmented, pipelined, malformed, oversized, slow, concurrent, hostile-origin
  overlay traffic;
- device-specific rate, multi-key fairness, release, reconnect, and shutdown for
  each supported protocol family.

## Recommended order

1. establish an immutable baseline and machine-readable lifecycle contracts;
2. add exception containment without changing normal algorithms;
3. make stop/start ownership truthful and remove forced termination;
4. isolate driver and child-process generations;
5. centralize transactional persistence and writable paths;
6. harden IPC and overlay framing/capabilities;
7. pace UAP and stabilize identity/snapshot ownership;
8. remove privileged runtime installation and pin build inputs;
9. align build/documentation and run proportional long-duration qualification.

This ordering became `STABILITY_MASTER_PLAN.md`.

## Positive baseline properties

The audit retained several strengths: protocol parsing was substantially
separated and testable; native routes generally failed closed before claiming a
device; digital input was not fabricated into analogue depth; process isolation
limited third-party crashes; multi-device aggregation and curve/remap semantics
were explicit; and the project already contained enough portable tests to make
small, evidence-driven migration practical.

## Final baseline assessment

The v3.9.0 code was functional but had serious edge-case ownership and evidence
gaps. The correct response was neither cosmetic patching nor a single wholesale
rewrite. It was the staged program documented here, with status moving only when
the required portable, Windows, failure, persistence, and physical gates passed.
