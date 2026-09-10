# RM-18 synchronous persistence-owner review (2026-09-06)

HallJoy currently uses a synchronous persistence model, intentionally retained
for durability. Each writer finishes its atomic transaction (including the
flush/validate/replace boundary) before returning. There is no detached or
background configuration writer; all interactive call sites execute from the
UI message path, and RM-17 prevents a second parent process for the same user
from reaching the shared settings root.

For that reason an asynchronous save queue would add a new stale-profile,
ordering, shutdown, and failure-reporting surface without reducing an existing
writer race. This is a reviewed synchronous design, not a deferred promise to
save later.

The binding panel had bypassed the common UI hand-off. It now uses
`KeyboardUI_SaveBindingsAfterUserChange`: the binding transaction must succeed
before dirty state is announced and before the root's debounced settings write
is requested. Aggregate settings saves now return a success result; failed
timer and shutdown saves are recorded explicitly rather than logged as
unqualified success. Profile switch/create/delete retain their necessary
synchronous barriers, because each must know whether the preceding durable
transaction completed before changing active state or deleting a profile.

The structural oracle covers the common binding hand-off and aggregate result
propagation. Full static audits and source-only syntax checks passed. No
HallJoy runtime, HID, controller, output child, or ROG diagnostic was started.
