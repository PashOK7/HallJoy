# RM-14 — native HID containment review

## Existing boundary

SparkLink, Sayo, MAD68, Hex80, Addressed and Aula use overlapped HID handles,
cancellation/stop events, bounded joins and generation poison on an unconfirmed
join.  Published input is neutralized before/after appropriate joins; handles,
buffers and `OVERLAPPED` state remain retained until the owning worker has
actually exited.  `TerminateThread` is forbidden.

Addressed is the most explicit case: its reader join is deliberately inside the
worker/session lifetime.  Moving it to a detached thread or freeing session
resources on the timeout would create use-after-free risk, not containment.

## Decision

The existing fail-closed, retained-resource boundary is correct for known native
in-process transports.  A process transport for every provider is not added
without a reproduced blocking-I/O threat model and a vertical test adapter;
doing so would materially expand device ownership/routing and may introduce new
HID conflicts.  The isolated UAP host is not evidence that every native protocol
can be moved safely without its own capability/teardown proof.

## Remaining gate

Each enabled family still needs hardware pending-I/O/unplug timing evidence in
RM-30.  A provider with an observed irrecoverable cancellation hang must become
a dedicated RM-14 vertical child-transport candidate, with generation/job/reap
proof.  Until then restart poison plus retained resources is the honest safe
outcome.
