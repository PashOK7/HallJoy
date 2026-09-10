# Generic process-generation supervisor design

Date: 2026-08-22.

Package: `F1`, prerequisite for ViGEm output stages 3-5.

Status: production-routed and locally green; physical F3 evidence reopened raw
process-handle ownership, and the owner is now kernel-protected. Corrected exact
artifact hardware continuity remains pending.

## Problem / evidence

The ViGEm output boundary needs a hard process-level recovery limit, but moving
driver calls into a child before proving child ownership would mix two unknowns:
process lifecycle defects and ViGEm behavior. The existing analog-host contains
useful patterns, but its supervisor is coupled to UAP globals, snapshots,
debugger crash capture and plugin-specific restart policy. It also starts the
child before assigning it to the job, leaving a launch-to-containment window.

O1 and O2 already prove the legacy raw-handle and generation failures. R1.1
proves only the shared newest-value channel. F1 must therefore prove a reusable
process generation independently of ViGEm and independently of UI timers.

## Affected invariants and scope

1. Exactly one object owns the child process and job handles for a generation.
2. A child executes no application code before successful job containment.
3. Only an explicit allowlist of unnamed handles is inherited.
4. Startup ready and runtime progress belong to the expected PID, nonce and
   generation; stale observations cannot keep a replacement alive.
5. Graceful stop has a bound; timeout terminates the whole generation job.
6. A replacement cannot start until the previous process is confirmed reaped.
7. A failed reap retains ownership and makes restart explicitly unsafe.
8. The supervisor runs outside realtime; no publication path waits on it.
9. Planned stop, early exit, startup timeout, progress timeout, child fault and
   reap failure are distinct results.
10. This package neither invokes ViGEm nor changes the production output route.
11. Process and job handle slots are protected from foreign close by the kernel;
    only the sole owner removes protection for confirmed terminal close.
12. An accepted generation proves the process handle still identifies the PID
    returned by `CreateProcessW` and records that evidence in its result.

## Old-bug oracle

O1/O2 remain the legacy RED oracles. F1 adds process-boundary negative fixtures:

- an inheritable decoy handle omitted from the explicit list must be invalid in
  the child;
- a child that exits before ready must never be reported running;
- a child that never becomes ready must be terminated and reaped at the startup
  deadline;
- a ready child that stops progress must be terminated and reaped;
- a child that ignores stop must be job-terminated and reaped;
- repeated crash/restart cycles must never overlap active generations.

## Option A — reuse analog-host supervisor as-is

Benefits: smallest new code and existing field experience.

Rejected: it is not a reusable contract. It depends on `g_client`, UAP shared
state, debugger events, plugin error types and logging. More importantly,
`CreateProcessW` runs the child before `AssignProcessToJobObject`; copying this
would preserve a containment race. Special-casing ViGEm into that loop would
create two service policies inside one implicit state machine.

## Option B — extract and migrate analog-host immediately

Benefits: one supervisor implementation from the first integration and removal
of existing duplication.

Rejected for this package: simultaneous UAP migration would change a currently
qualified crash-isolation route while F1 is still proving the primitive. It
would combine analog-host ABI/debugger/diagnostic behavior with ViGEm
foundations and make rollback or failure attribution ambiguous. Analog-host can
be migrated later behind its existing characterization gates.

## Option C — clean reusable owner plus generic state machine

Create a production-linked module with two layers:

- `ProcessGenerationOwner`: exact Win32 resource owner. It creates a
  per-generation kill-on-close job, launches the child `CREATE_SUSPENDED` with
  `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`, assigns it to the job, and only then
  resumes the primary thread. Process/job handles remain private until confirmed
  reap.
- `ProcessGenerationSupervisor`: non-realtime one-generation state machine. It
  validates generic ready/progress observations, enforces startup/progress/
  graceful-stop/hard-reap deadlines and returns a truthful typed outcome.

The module is first exercised by a self-spawning fake child. ViGEm and UAP are
not dependencies of either layer.

## Comparison

| Criterion | A: reuse as-is | B: migrate analog-host now | C: clean common primitive |
|---|---|---|---|
| Correctness | retains launch/job race and implicit globals | can be correct but changes two services at once | suspended-before-job and explicit state contract |
| Latency | off realtime | off realtime | off realtime; bounded observation polling |
| Liveness | UAP-specific and mixed with debugger behavior | broad but migration-heavy | explicit startup/progress/stop/reap outcomes |
| Ownership | spread across `g_client` and loop | could improve after risky extraction | one private owner per generation |
| Compatibility | ViGEm special cases enter UAP code | risks qualified UAP route | no route changes in F1 |
| Testability | difficult without UAP shared state | failures hard to attribute | deterministic fake child and real Win32 objects |
| Security | existing explicit list, but pre-job execution window | depends on full extraction | explicit handles and containment before resume |
| Maintainability | increases coupling | good end state, high immediate risk | reusable narrow primitive; later adapters possible |
| Rollback | entangled | broad | isolated new files plus project/test entries |

## Chosen option

Option C. It removes the whole launch/containment/ownership class without
disturbing either current output or analog-host routing. It reaches the desired
shared architecture through staged replacement and leaves a clean later path
to migrate analog-host rather than permanently duplicating supervisors.

## Rollback boundary

F1 consists only of the new common module, its production project entries, the
fake-child integration test, native-runner entry and documentation. Removing
that complete set restores the pre-F1 state. No partial production routing
change is allowed.

## Required gates

- explicit-handle negative test including an inheritable decoy;
- containment-before-resume proof;
- normal ready/progress/graceful-stop pass;
- exit-before-ready, startup-timeout, progress-timeout and ignore-stop pass;
- forced child exit at lifecycle boundaries;
- at least 1,000 sequential generations with zero overlap and zero survivors;
- invalid launch/job/ResumeThread/reap results remain restart-blocking;
- full native suite and MSVC x64 simulator/production builds;
- static audit proves no ViGEm call and no production routing switch.

## Documentation to update

`DECISIONS.md`, ViGEm architecture, roadmap, validation matrix, worklog,
risk/test manifest and retained privacy-safe evidence.

## Physical F3 reopening and protected-owner amendment

`HallJoy (10).log` showed that the clean process boundary alone was not a
sufficient ownership proof. Generation 1 ended `ReapFailed` with
`ERROR_INVALID_HANDLE` at 135.641 seconds while the GravaStar V75 reader kept
delivering error-free matrices. The owner retained an integer handle value, but
Windows no longer had the corresponding parent handle-table entry.

D-064 adds a narrow kernel-enforced layer rather than PID reopen or duplicated
fallback handles. `ProtectedNativeHandle` sets
`HANDLE_FLAG_PROTECT_FROM_CLOSE` at adoption, is non-copyable/non-movable, and
clears protection only in its sole-owner `Close`. The supervisor stores both
the process and job in this type and records protection/PID identity in every
generation result. A Windows regression deliberately calls `CloseHandle` on
the protected value, requires `ERROR_INVALID_HANDLE`, proves the object remains
waitable, then closes it successfully through the owner.

The amended exact suite passes 1,008 generations with protected process/job
handles, PID match, zero overlap and zero survivor. This converts accidental
cross-subsystem close from a latent use-after-close/rebound-handle failure into
a rejected syscall. Hardware status remains pending until the exact corrected
artifact survives a long active-output run.

## F1 implementation result

The selected common module is production-linked but not routed. One supervisor
instance passed normal ready/progress/stop, exit-before-ready, startup timeout,
progress timeout, ignored stop, post-ready exit, wrong-generation and explicit
child-fault scenarios. Natural non-inheritable-handle and missing-executable
launch failures left the same owner restart-safe. It then completed 1,000 more
sequential generations with containment before resume, no decoy-object
inheritance, no overlap and no live survivor. Full native and both MSVC x64
targets pass. Evidence:
`../stability/tests/V14_F1_PROCESS_GENERATION_SUPERVISOR_2026-08-22.txt`.

F1 does not satisfy the later self-host/ViGEm gates. Next, the actual HallJoy
executable must branch early into `--halljoy-vigem-output-host` with a fake
transport, using this owner and the R1.1 shared channel. Only after that exact-
EXE lifecycle passes may ViGEm library calls move into the child.
