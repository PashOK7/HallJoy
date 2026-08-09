# Architecture decision record

## ADR-001: local phased work without GitHub

Accepted. Stabilization was performed in a local working copy and exchanged as
complete stage archives. Remote repositories and CI workflows were not evidence
for the local package.

## ADR-002: no monolithic rewrite

Accepted. Existing protocol parsers and routing had useful characterization
tests. Changes were split into small packages, preserving public behavior;
architectural separation followed tests and touched one module at a time.

## ADR-003: timeout means `Poisoned`, not `Stopped`

Accepted. When completion is unconfirmed, a worker's context may be neither
reused nor destroyed as if normally stopped. Restart remains forbidden until
the generation completes or the process is contained.

## ADR-004: cooperative shutdown, not forced termination

Accepted. `TerminateThread` is forbidden on normal paths. A worker owns its I/O,
observes stop in its wait set, and proves completion before its memory and
handles are released.

## ADR-005: measure before optimizing

Accepted. Optimization requires a baseline, reproducible scenario, and output-
equivalence check. CPU, latency, wake rate, USB transaction rate, and allocation
rate are measured independently.

## ADR-006: monotonic generations and a distinct `Joined` state

Accepted. Each start attempt receives a new nonzero `GenerationId`, including
failed starts. Only `Joined` proves completion of that generation and permits
resource replacement; timeout transitions to `Poisoned`.

## ADR-007: allocation-free machine-readable lifecycle errors

Accepted. `LifecycleError` remains trivially copyable and records operation,
source/requested state, supplied/active generation, and native error. Human text
is formatted outside `noexcept` and C ABI boundaries.

## ADR-008: platform primitives are separate from lifecycle state

Accepted. `WorkerPrimitives` abstracts wait, time, thread, and handle operations
so tests can reproduce startup, timeout, and join failures without real handles.
S01 deliberately introduced the seam before connecting runtime backends.

## ADR-009: one allocation-free C++ exception barrier

Accepted. Thin `noexcept` OS entries invoke one helper that catches only C++
exceptions, copies `what()` into fixed storage, and invokes `noexcept` callbacks.
Normal algorithms remain separate; exceptions are not control flow and the hot
path gains no mutex or allocation.

## ADR-010: faults close publications; owners reap generations

Accepted. A worker can close producer/running state and release worker-owned
resources, but only the owner destroys its thread, WSA, or file generation.
Reinitialization waits for owner-side stop/shutdown to confirm completion.

## ADR-011: realtime faults publish neutral state

Accepted. An unexpected C++ exception must not leave UI or virtual gamepad input
pressed. The allocation-free fail-safe clears snapshots/reports and attempts a
neutral XUSB update. Full ViGEm isolation remained assigned to S12.

## ADR-S02A1-01: validate generated patch content

Accepted. A self-modifying patcher is audited by extracting the here-string it
actually writes, not by searching the entire source script. The shared marker
must be present in `$preOpenBlock` and reused by post-patch validation.

## ADR-012: migrate native workers in ownership-complexity order

Accepted. MAD68 and Hex80 had no nested worker generation and formed S02B.1.
Addressed had a nested overlapped-I/O reader; SparkLink/Sayo had distinct Win32
handle lifecycles. Keeping them separate prevented exception work from masking
ownership and shutdown changes.

## ADR-013: reap a completed joinable `std::thread` before replacement

Accepted. A completed `std::thread` remains joinable; overwriting it calls
`std::terminate`. Start first acquires ownership, joins the old generation,
releases its wake handle, then creates the replacement. Fault records clear only
after reap.

## ADR-014: hardware validation has levels

Accepted. SparkLink served as the continuous available-hardware gate. A package
whose protocol/hot path was unchanged could proceed without unavailable
MAD68/Hex80 hardware, but remained `Implemented / device gate deferred` until a
device owner validated the pre-final archive. This avoided both indefinite
blocking and false claims.

## ADR-015: SparkLink keeps SEH outside the C++ barrier

Accepted. The Win32 entry retains a thin `noexcept` SEH wrapper; an inner
`noexcept` entry uses `RunWorkerEntryBarrier`. Both converge on one idempotent
neutral-publication contract while native SEH code remains separately recorded.
Polling, HID protocol, pacing, and cooperative-shutdown work stayed unchanged.

## ADR-016: SparkLink startup verifies worker completion state

Accepted. Every C++/SEH exit publishes `g_sparkWorkerExited`. `SparkStart()`
checks it before and immediately after `connected=true`, closing the race where
an already finished worker could be reported as healthy.

## ADR-017: a nested reader must be reaped before rethrow

Accepted. Stack unwinding through a joinable nested `std::thread` terminates
before an outer catch. Addressed therefore creates the reader inside a protected
session, requests stop/cancellation, joins it, clears stack-backed publications,
then rethrows to the main barrier. Reader overlapped-I/O ownership remained a
separate S06 concern.

## ADR-018: runtime packages require machine evidence

Accepted. From S02V1 onward, observation alone was insufficient. Acceptance
required a defined scenario, bounded log bundle, and deterministic analyzer:
PASS meant complete coverage with no prohibited event, WARN required a repeat,
and FAIL blocked the next package.

## ADR-019: trace events use memory mapping, not filesystem writes

Accepted. Fault/completion/shutdown paths cannot depend on disk latency. Events
copy fixed records into a pre-mapped 1 MiB buffer; flushing occurs only after
workers stop. Sequence assignment shares the append lock, and overflow is an
explicit FAIL.

## ADR-020: temporary instrumentation follows the change and is removed

Accepted. Once a contract passes, its specialized events are deleted while the
report and source bundle SHA remain. Temporary trace code, build flags, and
collectors are removed before release unless a separate decision approves an
opt-in support mode.

## D-S06: only the Addressed reader completes and releases pending I/O

Accepted for V14-12B. `CancelIoEx` is a request, not completion. The reader owns
the HID handle, event, buffer, and stack `OVERLAPPED`; owner threads may request
cancellation but never close the handle. A stalled driver keeps the entire
session generation alive. The main worker exposes a waitable handle, the
registry waits at most 3000 ms, and timeout retains resources, returns
`TimedOut`, poisons restart, and selects process containment.

## D-S07-HEX80: the worker alone closes the active session handle

Accepted for V14-12C. Stop publishes state, wakes the event, and requests
`CancelIoEx`; the worker-owned session unregisters before RAII closes HID.
Post-stop responses cannot decode or publish. A 3000 ms timeout retains the
generation and requires containment. Protocol bytes and routing proof do not
change.

## D-S07-MAD68: cancellation must preserve final A9 recovery

Accepted for V14-12D. Owner stop cancels only the persistent read. Worker-owned
write/control operations and final idempotent A9 remain available; no new A8 may
start after stop. Unconfirmed completion retains every session resource,
poisons restart, and selects containment.

## D-SYNC-001: the release risk register owns current statuses

Accepted. `docs/v1.4/RISK_REGISTER.md` is authoritative for the current release;
this directory preserves original IDs and detailed evidence. Status changes
require package evidence and cannot be inferred from code presence alone.

## D-S18-001: runtime does not install privileged dependencies

Accepted. HallJoy does not download a mutable executable to temporary storage
and launch it elevated. Missing ViGEmBus yields exact pinned manual guidance for
1.22.0 plus restart. No timeout disguises a live installer because HallJoy no
longer starts one.

## D-S21-001: proportional long-run qualification

Accepted for v1.4. Required evidence combined one attended production hour with
overlay, trace, and immutable state plus 1000 independent startup/`WM_CLOSE`
cycles. A longer 8-24-hour run could add confidence but could not prove absence
of failure at an arbitrary future hour. When a narrow soak defect was found, the
corrected artifact required isolated root cause, deterministic regression, full
build gates, targeted production runtime, and preserved old/new hashes rather
than blindly repeating every expensive gate at once. External device gates
remained independent.
