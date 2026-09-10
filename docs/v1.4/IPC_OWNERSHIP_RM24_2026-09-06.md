# RM-24 IPC ownership and lifetime review (2026-09-06)

## Scope and decision

The review separates public compatibility IPC from private per-generation IPC.
The stable mouse bridge cannot be renamed or resized without a coordinated ASI
release. Its publisher is single-writer, and its fixed 40-byte v1 layout is
therefore retained. A two-phase heartbeat commit now makes a coherent capture
available to a reader that opts into it: read an even heartbeat, read the four
state fields, then accept only if the same even heartbeat is still present.
Older ASI binaries retain their existing monotonic-heartbeat behaviour.

The private analog-host and ViGEm paths deliberately use unnamed inherited
handles, not discoverable object names. Their owner PID, CSPRNG nonce and
generation checks are session identity, not merely diagnostics. The Provider
V2 mapping is read-only in the parent and read/write only in the child; the
parent gives up its writer handle after a successful child launch.

Changing every channel to a named/global mapping was rejected: it would widen
the trust boundary and regress the capability transport. Replacing the V2
broker leases with a plain seqlock was also rejected: a reader could retain a
slot while a writer reused its storage. No protocol route relies on a timeout
to make released storage safe.

## Ownership matrix

| Channel | Creator and visibility | Reader / writer | Schema and coherence | Identity, retirement and close owner |
|---|---|---|---|---|
| Mouse bridge | UI creates stable named `Local\\HallJoy_MouseBridge_v1` with the creator process default DACL in the Local namespace. This is the only public compatibility object. | HallJoy UI is the sole writer; ASI helper is external reader/writer only for its attachment heartbeat fields. | Fixed 40-byte v1 (`magic`, version, structSize). Publisher makes `heartbeat` odd, publishes four state scalars, then makes it even. Existing readers remain ABI-compatible; an updated ASI uses the equal-even capture rule. | No generation is represented because the public ASI ABI predates it. New/existing mappings are schema-checked before use; wrong magic/version/size is rejected. UI unmaps/closes on shutdown. A new UI can reopen a surviving mapping only after schema validation. |
| Analog-host control plane | Parent creates unnamed inheritable shared mapping, manual stop event, auto-reset snapshot event and inherited owner-process handle; child inheritance is an explicit handle list. Supervisor-ready event is parent-private. | Parent and contained child use only passed capabilities; no named-open path exists. | Shared control ABI v15 has magic/version/size plus owner PID, nonce and generation-bound control status. | Parent creates, signals stop, reaps child and closes resources only after bounded ownership rules. Child checks inherited handles, owner PID and nonce before accepting payload. Job close contains a crashed child. |
| Provider V2 data plane | Parent creates unnamed mapping, initializes it, duplicates a read-only non-inherited parent handle and inheritable read/write child handle. | Child is sole writer; parent is reader only. | Calculated bounded layout rejects capacity/size overflow and malformed/undersized mapping. Per-slot odd/even commits carry generation, nonce and transaction token. | Parent validates read-only protection and inability to map its handle writable. On retirement it withdraws publication, drains read leases, then closes view/handles; a timed-out drain retains resources. Parent closes its writer capability after child launch. |
| Provider V2 snapshot broker | Parent process heap, preallocated outside realtime; not an OS IPC object. | Snapshot bridge is sole writer; realtime obtains a bounded read lease. | Fully validated V2 snapshot is copied to an unleased slot before publication. | Resize/restart revokes publication and drains writer/readers before freeing slot storage. `NoFreeSlot` drops an intermediate snapshot rather than blocking/reusing a leased slot or inventing freshness. |
| ViGEm output child transport | Parent creates unnamed inheritable `SharedStateV1` mapping, wake event, stop event and restricted inherited owner-process handle. Runtime command event is parent-private. | Parent publishes reports/progress; output child reads reports and publishes telemetry. | Fixed V1 shared state; output generation admission and publication quiescence prevent commit after generation disable. | CSPRNG launch nonce and generation are command-line and shared-state checks. Parent reaps child, disables generation, then performs bounded producer drain before mapping reuse/close. Destructor retains containment ordering. |
| Latest-value mailbox | In-process object only; no mapping/event/name. | One declared producer and one consumer under its atomic mailbox contract. | Payload publication uses the mailbox atomic protocol rather than a pointer into external mutable storage. | Its owner embeds it and destroys it only after its worker/generation is stopped; it cannot outlive a process or be opened by another instance. |

## Fault evidence and remaining physical gate

Static gates cover named mouse schema rejection/legacy upgrade and the new
commit boundary; analog-host capability-only transport and identity rejection;
Provider V2 layout/read-only/lease rules; and ViGEm generation/quiescence
ordering. Source-only syntax validation of the changed mouse producer passes.
No HallJoy process, ASI helper, HID device, controller, output child or ROG
diagnostic has been started in this session.

The external ASI source is not part of this repository, so its actual adoption
of the equal-even capture rule requires a coordinated ASI build/test. Until
then, the compatibility guarantee is intentionally limited to the old ABI and
monotonic heartbeat semantics; HallJoy itself no longer publishes a torn state
to an opt-in coherent reader. Physical crash/restart and reader-during-shutdown
testing remain release evidence, not a reason to weaken the ownership contract.
