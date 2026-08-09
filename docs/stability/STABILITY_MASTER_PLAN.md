# HallJoy stabilization master plan

Baseline: v3.9.0 imported on July 30, 2026

## Purpose

Correct stability, lifecycle, persistence, security, and performance defects
without changing established protocol behavior or hiding incomplete validation.
Each package is intentionally small, independently reversible, and accepted
only through the gates appropriate to its risk.

## Process rules

1. Preserve an immutable baseline and exact source/package hashes.
2. Reproduce or prove the root cause before changing production behavior.
3. Do not combine unrelated protocol, lifecycle, UI, and optimization work.
4. Characterize the normal path before refactoring it.
5. Treat timeout, partial startup, failed persistence, and missing hardware
   evidence truthfully; none is success.
6. Retain worker/resource ownership until terminal completion is proven.
7. Keep hardware-unvalidated status explicit.
8. Store command output, hashes, manifests, and acceptance boundaries with each
   package.
9. Roll back when invariants, hot-path identity, or mandatory gates fail.
10. Remove temporary instrumentation after its contract is accepted.

## Universal regression gates

### Gate L: portable local validation

- all Python static audits;
- all portable C++20 tests under the available GCC and Clang toolchains;
- strict warnings plus applicable ASan/UBSan;
- project/XML/script syntax checks;
- protocol/hot-path comparison for code declared unchanged;
- clean self-excluding package manifest.

### Gate W: Windows build and shared runtime

- clean MSVC x64 Debug/Release build as applicable;
- no unexpected warning;
- normal HallJoy/ViGEm startup and input;
- repeated startup/shutdown with no hang, crash, or surviving process;
- overlay and available-device smoke;
- user-state hash comparison.

### Gate F: lifecycle and fault injection

- injected startup, worker, cancellation, timeout, and rollback failures;
- bounded completion and truthful result state;
- neutral publication on disconnect/fault;
- no replacement of unreaped generations;
- process containment when completion cannot be proven.

### Gate H: physical hardware

- exact interface and protocol proof;
- input, release, multi-key, reconnect, and device-specific shutdown;
- no claim conflict with UAP or another native route;
- measured rate/latency where performance is part of the claim;
- evidence from the exact tested executable.

### Gate P: persistence and security

- write/readback/replace transaction under injected failure;
- source-preserving migration and rollback;
- normalization, collision, reserved-name, and path-boundary cases;
- IPC identity/capability and origin/cookie negative tests;
- dependency pinning and absence of automatic privileged installation.

## Work-package sequence

### S00: plan and immutable baseline

Record archive/source hashes, import the audit, create the risk/decision/work
records, and prove zero production-code change.

### S01: lifecycle contracts and test scaffolding

Introduce generation-aware state/result types and platform-neutral worker
primitives without connecting them to existing runtime paths.

### S02: exception boundaries

Add one allocation-free C++ barrier in ownership-complexity order: core workers,
simple native workers, SparkLink C++/SEH, Addressed nested reader, then remaining
workers and UAP C ABI. Preserve normal algorithms exactly where claimed.

### S03: truthful stop contract and lifecycle registry

Replace boolean stop success with generation-scoped results; serialize registry
ownership, enforce thread affinity, and poison incomplete generations.

### S04: cooperative realtime shutdown

Replace forced termination with explicit stop wake, bounded wait, neutral
publication, and retained ownership on timeout.

### S05 and S05A: logger and overlay shutdown

Close producer/accept gates, wake workers cooperatively, bound join, and keep
file/WSA/socket resources owner-reaped.

### S06: Addressed overlapped-I/O ownership

Make the reader the sole owner of active HID, buffer, event, and `OVERLAPPED`;
cancel and reap before close; retain the complete session on timeout.

### S07: Hex80 and MAD68 lifecycle migration

Give each worker a waitable generation and bounded join. Hex80 owns its session
handle. MAD68 additionally forbids post-stop A8 and preserves final A9 recovery.

### S08: SparkLink and Sayo lifecycle migration

Remove normal forced termination, preserve device-specific wait/wake behavior,
and add neutral release plus retained-generation containment.

### S09: analogue-host generation safety

Bind supervisor, bridge, IPC, child process, and job object to one generation;
reap partial startup in reverse order; reject overlap and uncontained restart.

### S10: UAP ABI and unload safety

Contain C++ at every C export, replace manual locks with RAII, report real
initialization state, validate null buffers, and perform bounded worker unload
outside the global registry lock.

### S11: startup transaction and wake correctness

Commit required phases explicitly, roll back in reverse order, use a durable
monotonic input sequence rather than a resettable wake event, and publish curve
generations with release/acquire ordering.

### S12: ViGEm output isolation

Move driver calls out of realtime into a dedicated latest-state owner with
multi-pad equivalence, neutral recovery, bounded shutdown, and containment of a
stalled driver.

### S13: transactional persistence

Centralize same-directory temporary writes, flush, schema/readback validation,
atomic replacement, failure-stage reporting, and known-good preservation for
settings, bindings, overlay, layouts, curves, and profiles.

### S14: writable storage and name policy

Default to LocalAppData, require an explicit writable portable marker, migrate
legacy state with backup/replay protection, normalize Unicode and reserved
names, and guarantee direct-child paths.

### S15: IPC hardening

Fix mouse mapping-creation detection and interlocked peer reads. Replace named
analogue-host objects with an explicit inherited-handle capability list plus
owner identity and generation token.

### S16: overlay correctness and concurrency

Implement bounded incremental HTTP framing, numeric validation, loopback origin
and session-cookie checks, fixed concurrent-client capacity, and bounded active
client shutdown.

### S17: UAP polling, identity, snapshots, and exact ownership

Add deadline pacing/backoff, stable path-based device identity, bounded owner
pins for snapshots, and exact HID-interface claims before Soup opens a path.

### S18: dependency and supply-chain hardening

Remove runtime download/elevation/wait. Pin manual ViGEm guidance and every
build dependency/action to immutable versions and hashes.

### S19: modularization with behavior preservation

Split monoliths only behind characterization tests and token/behavior evidence;
do not combine this structural work with protocol changes.

### S20: build, local CI-equivalent gates, and documentation

Make shipped commands authoritative, require all static/portable checks, support
x64 only, enable W4 with an explicit narrow warning baseline, verify dependency
hashes and release contents, and align public/internal documentation.

### S21: performance and long-run qualification

Run persistent lifecycle cycles, attended production soak, available-device
stress, process/handle/state invariants, and targeted follow-up after any narrow
root-cause correction. Performance claims require correct output and safe
shutdown, not rate alone.

## Dependencies between packages

- S02 depends on S01's fixed error contract.
- S04-S10 depend on S03's truthful registry results.
- S11 must precede output and reconnect qualification.
- S13 precedes storage migration in S14.
- S15-S17 require lifecycle containment before increasing concurrency/rate.
- S18 and S20 precede final qualification.
- S21 runs only on the exact post-build release artifact.

## Package contents

Every archive records its baseline package/hash, bounded source diff, decision
and risk updates, commands and output, platform/hardware limitations, exact
artifact hash, rollback point, and next permitted package. Generated evidence
must exclude itself from source-integrity comparison.

## Stop and rollback conditions

Stop the package when a supposedly unchanged protocol/hot path differs, a gate
fails nondeterministically without root cause, a timeout is reported as success,
an unreaped generation is destroyed/replaced, neutral release is not proven,
state migration risks source loss, hardware evidence contradicts the model, a
new warning/error appears, or the diff expands beyond the declared boundary.

Rollback to the last accepted archive, preserve failing evidence, narrow the
cause, and create a separate package. Never hide a failed gate by weakening its
acceptance criteria.
