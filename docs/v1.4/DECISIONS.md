# HallJoy v1.4 decision log

Decisions are append-only. A superseding decision references the old ID instead
of silently rewriting it.

## D-001 - Product version is v1.4

Date: 2026-07-31

The next public HallJoy version is `v1.4`. The imported `v3.9.0` label describes
the source archive only and is retained solely in historical evidence paths and
provenance records.

## D-002 - The advanced archive is the integration baseline

Date: 2026-07-31

The advanced archive contains the more complete architecture and becomes the
v1.4 code baseline. v1.3 is not merged file-by-file. Valuable v1.3 behavior is
ported deliberately and tested against the new architecture.

## D-003 - Preserve both source lines before integration

Date: 2026-07-31

The v1.3 self-contained SDK work is preserved at commit `b3fefce`. The imported
archive is preserved at commit `f5e8c18`. Later packages must remain reviewable
against both points.

## D-004 - v1.4 documentation is normative

Date: 2026-07-31

`docs/v1.4` owns current status. Imported `docs/stability` files are immutable
historical evidence unless a correction is clearly marked as an erratum.

## D-005 - Documentation is part of each package gate

Date: 2026-07-31

Code, tests, roadmap, risk status, validation evidence, and worklog changes are
committed together. A code-only package cannot be marked complete.

## D-006 - No GitHub publication during integration

Date: 2026-07-31

Branches and commits remain local until explicit approval. `main`, tag `v1.4`,
and GitHub Releases are unchanged during implementation.

## D-007 - Analog support is self-contained

Date: 2026-07-31

HallJoy may require the ViGEmBus system driver, but it must not require a
system-wide Wooting Analog SDK or Universal Analog Plugin installation. The
private UAP runtime is versioned, integrity-checked, and stored in a writable
per-user location when the executable directory is protected.

## D-008 - Simulation and hardware evidence remain separate

Date: 2026-07-31

A development-only simulated analog backend will exercise the common
aggregation, curve, SOCD, ViGEm, telemetry, hotplug, and fault paths on machines
without an analog keyboard. It is excluded from production builds by default,
uses an explicit test launch mode, and cannot mark MAD68, Hex80, Addressed,
SparkLink, Sayo, or UAP device gates as verified.

## D-009 - Simulator output uses the production pipeline

Date: 2026-07-31

The simulator publishes only normalized native values. It does not implement a
parallel curve, SOCD, report, or ViGEm path. Its scenario assigns temporary
in-memory WASD bindings and verifies reports after the common processing path.
Simulator trace events are compiled only into the simulator target and are
labelled `simulated=1 hardware=0`.

## D-010 - Private UAP uses portable-first, per-user fallback storage

Date: 2026-07-31

HallJoy first preserves portable behavior beside the executable. If that
location is not writable, it stores the exact embedded ABI1 runtime in a
versioned `%LOCALAPPDATA%\HallJoy\Runtime` directory without elevation. The
isolated child receives the verified absolute path explicitly. System Wooting
SDK and global UAP installations are neither required nor valid repair actions
for this architecture; ViGEmBus remains the only offered external runtime
dependency.

## D-011 - One lock owns build provenance

Date: 2026-07-31

`tools/dependency-lock.json` is authoritative for remote source commits,
the reviewed Soup overlay and its hashes, GitHub Action commits, runner labels,
toolchain families, and binary dependency integrity. Build scripts consume the
lock rather than duplicating moving references. Full commit SHAs are required;
human-readable tags are comments.

Local and CI gates use the same checked-in entry points. The official Windows
build requires portable C++20 tests and rejects warnings outside the explicit
ViGEm `LNK4099` PDB allowlist. V14-04 verification requires an independent
local clone with empty build caches; GitHub Actions is useful but optional and
must not block development when account quotas are unavailable.

## D-012 - The production executable has one canonical public name

Date: 2026-07-31

The official x64 production build, package instructions and trace collector use
`HallJoy.exe`. Backend-specific implementation history does not appear in the
public filename. Diagnostic and simulator targets keep distinct names because
they are development artifacts and must not be confused with production.

## D-013 - Backend readiness is a transaction and wakes are durable state

Date: 2026-07-31

Backend readiness is published only after realtime, required native phases and
Raw Input prerequisites have all succeeded. A failure rolls back acquired
ownership in reverse order; an unconfirmed stop retains ownership and selects
process containment instead of continuing dependent cleanup.

Input-change correctness belongs to a process-lifetime monotonic sequence, not
to the transient wake primitive. `WakeByAddressSingle` reduces latency but does
not own the notification, and a worker restart never resets the sequence.
Settings writers release-publish curve generations and cache readers
acquire-observe them. ViGEm output ownership is deliberately unchanged here and
is isolated separately in V14-08B.

## D-014 - Runtime ViGEm calls have one isolated owner

Date: 2026-07-31

After initial client/target creation during backend startup, a dedicated output
worker exclusively owns ViGEm update, reconnect and destruction calls. Realtime
may only try-publish a complete newest-state report batch and signal the owner;
it never waits for driver I/O.

Coalescing preserves every pending virtual-pad bit but replaces report payloads
with the newest complete snapshot. Emergency neutralization discards older
queued reports and makes neutral the final driver write. Output shutdown has one
three-second bound.
If completion is not confirmed, thread/event and driver ownership are retained,
dependent teardown is forbidden, restart is poisoned and the disposable process
exits without normal CRT destruction. A simulator-only driver stall may verify
this containment path but can never be activated in a production build.

## D-015 - Durable file saves share one transaction contract

Date: 2026-08-01

A mutable HallJoy file is not considered saved until a unique temporary file in
the destination directory has passed checked writes, explicit physical flush
and format-specific parse/readback, then replaced the destination atomically
with write-through semantics. The destination is the commit boundary; no
earlier stage may modify it, and every failed stage removes its temporary file.

Save APIs return a truthful result and also publish the data kind, failed stage,
native error and destination path. Production surfaces the first failure in the
UI so ignored autosave returns are not silent. Simulator-only stage injection is
excluded from production. V14-09A established this contract for settings,
overlay metadata, active-profile metadata and bindings; V14-09B extends it to
layout presets, curve presets and curve active-state metadata. Writable-state
migration must use the same contract in V14-09C.
