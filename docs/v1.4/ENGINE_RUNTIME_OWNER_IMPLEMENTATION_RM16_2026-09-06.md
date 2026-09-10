# RM-16 serialized engine owner — implementation record (2026-09-06)

## Scope of this change

`engine_runtime_owner` is the concrete, process-local command executor for the
already-defined RM-16 transaction. It starts closed in `Paused`; it has no
adoption mode for handles or providers that were started elsewhere. That rule is
intentional: adopting a live initial generation would make it impossible to
prove that the owner is the sole lifecycle authority.

The owner accepts exactly one pending Pause or Resume request, serializes it on
one Windows worker, delegates every transaction stage to an explicit noexcept
callback table, and exposes only a snapshot to callers. A duplicate request is
idempotent; a contradictory request while one is queued is rejected rather than
silently reordered.

## Failure and shutdown rules

- the callback table must be complete before the worker starts;
- every callback returns success only after its own resource/acknowledgement is
  confirmed; this includes the bounded posted-message UI bridge;
- transaction ordering is supplied exclusively by `engine_runtime_transaction`;
- worker stop is bounded to five seconds and retains/poisons its handles if the
  worker cannot be joined;
- an active owner receives the same Pause transaction before its worker exits;
  a `PauseFaulted` owner is never reported as successfully released.

## Application integration

Initial startup now registers the UI/input surface, starts the owner closed,
then sends its first Resume request. Fresh native routing, capability proof,
backend/UAP initialization, dependent start, and normal shutdown all run only
through the owner callback table. The old direct timer retry was replaced with
one owner Resume request and is permanently disabled after a user Pause.

The Global Settings page has a single state-derived control. It can request
Pause only from `Active` and Resume only from `Paused`; transitional states and
`PauseFaulted` are not presented as successful release. The UI bridge releases
hooks/cursor/mouse IPC before native/backend lease release and restores them
before backend admission opens. UI snapshot reads are lock-free: a slow proof
or release never blocks painting or message dispatch.

Every accepted state-machine transition is mirrored to that lock-free snapshot.
This makes the control truthfully show a release/start transition rather than
leaving the previous stable label visible until an entire transaction finishes.
The mirror is published by the sole owner while it holds the mutation lock; UI
readers never acquire that lock.

No physical device claim is made by this implementation record. The remaining
RM-16 evidence is the required hardware/web-driver coexistence and adverse-I/O
matrix; those tests remain deferred while the user is gaming.

The structural audit verifies that the compiled owner uses the common
transaction executor, begins from the closed state, serializes on its worker,
has a bounded stop, and never locks the UI while it reads owner state. Static
checks and source syntax checks were performed; no HallJoy runtime, HID,
controller, output child, or ROG diagnostic was started.
