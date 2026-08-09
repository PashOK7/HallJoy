# S01: lifecycle contracts and test scaffolding

- Date: July 30, 2026
- Status: Accepted by the portable gate; Windows/MSVC build not yet run

## Package boundary

S01 adds only generic lifecycle types, a tested state machine, and a platform
seam. No existing backend, worker, registry callback, or runtime start/stop path
uses the new headers. This deliberately establishes a provable contract before
later migrations without presenting it as a behavioral correction.

## Added contracts

### `worker_lifecycle.h`

- `WorkerState`: `Stopped`, `Starting`, `Running`, `StopRequested`, `Joined`,
  `Faulted`, and `Poisoned`;
- strongly monotonic `GenerationId`, where zero means no generation;
- allocation-free `LifecycleError` recording the operation, previous/requested
  state, supplied/active generation, and native error;
- `StartResult`, `StopResult`, and `TransitionResult` with safe predicates;
- `WorkerLifecycle` with named start, run, stop, join, fault, and poison
  transitions.

### `worker_primitives.h`

The platform-neutral `WorkerPrimitives` interface isolates:

- a monotonic clock;
- creation, signaling, and waiting of a stop primitive;
- thread start;
- bounded join;
- explicit closure of thread and wait tokens.

It exposes no Win32 types and is tested through a fake implementation.

## Key invariants

1. A generation ID increases for every accepted start attempt and is not reused
   after failed startup.
2. A stale or zero generation cannot mutate active state.
3. `Joined` proves completion of one generation; `StopRequested` does not.
4. `Poisoned` is not `Stopped`, forbids restart, and never masks timeout as
   success.
5. The first worker fault remains authoritative until the next generation.
6. `FailStartBeforeWorker` is valid only when no thread or live resource exists.
7. The state machine performs no OS operations; those results enter through the
   platform seam.

## Verified transition matrix

```text
Stopped       -> Starting
Starting      -> Stopped | Running | StopRequested | Faulted | Poisoned
Running       -> StopRequested | Faulted | Poisoned
StopRequested -> Joined | Faulted | Poisoned
Joined        -> Starting
Faulted       -> StopRequested | Joined | Poisoned
Poisoned      -> <none>
```

The unit test covers all 49 state pairs; named idempotent operations are tested
separately.

## Regression evidence

- pre-change and final GCC static/portable gates: PASS;
- final Clang static/portable gate: PASS;
- nine static audits and eight portable C++ tests: PASS;
- GCC 14.2 and Clang 17 with `-Werror`, ASan, and UBSan: PASS;
- `HallJoy.vcxproj` and `.filters` parse successfully;
- `ClCompile` count remains 58;
- modified existing production `.cpp/.h/.inc` files: 0;
- removed production files: 0;
- new production object files: 0;
- production translation units including the new headers: 0.

Detailed logs are under `docs/stability/tests/S01_*`.

## Deliberately deferred

- `TerminateThread` and existing join/timeout paths;
- backend-registry stop contracts;
- worker exception boundaries;
- Addressed overlapped-I/O ownership;
- analogue-host generation ownership;
- UAP lifecycle.

All 45 production risks therefore remained `Open` after S01. Windows/MSVC and
hardware fault matrices were unavailable in the original environment, so the
first Windows runtime integration required a separate Gate W.

Next package: `S02 — exception barriers`, beginning only with entry wrappers for
the realtime, debug, and overlay workers. Their algorithms, scheduling, and
shutdown behavior remain unchanged in that step.
