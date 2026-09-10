# RM-06 RuntimeConfig review

Date: 2026-09-06. This is a characterization record before changing the
profile/runtime ownership boundary.

## Existing useful contract

`Backend_Tick` acquires `profile_runtime::ReadLease` before reading profile
settings or bindings. Profile load and profile switch prepare and validate the
complete settings/key/bindings values first, then acquire `CommitLease` and
apply them together. The writer performs allocation and disk work before the
lease, while the realtime reader never waits. This is a real compatibility
property and must not be removed merely because the implementation is a gate.

The direct setting widgets are different: one UI gesture normally changes one
atomic setting or one independently versioned key snapshot. Those values are
not a profile transaction, and forcing every such single-field edit through a
large immutable object would create allocation/reclamation work without a
demonstrated benefit.

## Confirmed gaps and decision

The current gate is not a general immutable runtime configuration: its reader
uses a bounded three-attempt admission (so a concurrent reader no longer causes
a spurious one-shot rejection), `CommitLease` may yield for 500 ms off
realtime, and `KeySettings_Get` has an unbounded retry loop.
The latter is an RM-07 concern because it is the extended-key/curve-reader
implementation itself. The profile gate still provides the needed all-or-none
profile application today, provided every full profile writer uses it.

The next implementation step is therefore not an atomic shared_ptr rewrite.
First enumerate profile writers and tick consumers, then introduce a bounded
prepared read view only where one tick combines fields that can currently come
from different publication domains. The migration must retain the successful
prepared profile transaction, move all profile writers together, and avoid
turning ordinary UI/window/overlay preferences into realtime-owned state.

The bounded admission regression was implemented and its portable test covers
simultaneous readers, complete profile commits, writer rejection, and recovery
after a deliberately held reader. A full immutable RuntimeConfig remains a
later migration only if the writer/consumer inventory proves a mixed-domain
tick that the retained transaction boundary cannot protect.
