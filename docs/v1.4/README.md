# HallJoy v1.4 development

This directory is the authoritative source for v1.4 scope, status, decisions,
risks, and validation. Documents under `docs/stability` preserve the imported
archive history and evidence, but they do not define the current product
version or release status.

## Current status

- Product version: `v1.4` (in development)
- Working branch: `v1.4-integration`
- Completed package: `V14-09` transactional persistence, writable-state
  migration and filename hardening
- Completed hotfix: `V14-06D.1` SparkLink shutdown/reconnect race, Verified
- Completed subpackage: `V14-10A` Mouse IPC creation, schema and atomic-read
  correctness, Verified locally
- Completed subpackage: `V14-10B` analog-host inherited-handle IPC,
  Verified locally
- Completed subpackage: `V14-10C` bounded overlay HTTP framing and telemetry
  parsing, Verified locally
- Completed subpackage: `V14-10D` bounded overlay concurrency, strict origin
  policy and cooperative client shutdown, Verified locally
- Completed package: `V14-10` IPC and overlay security/correctness
- Verified by automated gates: `V14-11A` deadline pacing for UAP poll
  transports; no physical UAP performance claim is made
- Verified by automated gates: `V14-11B` stable identity for identical UAP
  devices across enumeration reorder and reconnect generations
- Verified by automated gates: `V14-11C` bounded device-owner capture; snapshot
  and telemetry exports no longer wait on per-device locks under the registry
- Current package: `V14-11` UAP pacing, identity, modularization and measured
  performance, In progress
- Next work: `V14-11D` bound the remaining UAP modularization/performance scope
- GitHub publication: not started
- Release status: not release-ready

The current workstation has an Irok MG75 Max (`VID 1CA6`, `PID 0529`) that
passes SparkLink capability, analog-row, held-key unplug/reconnect and balanced
shutdown proof on usage page `FFB0`. The V14-06D.1 acceptance trace has three
balanced worker generations, two successful reconnects, input before and after
reconnect, and no reconnect after final service stop. Simulator evidence remains
separate. The V14-09A production smoke additionally proves that the Irok route,
realtime loop and dedicated ViGEm output worker start and stop cleanly while the
new transactional settings path commits without a persistence error. V14-09B
extends that contract to layout and curve files; all five injected failure
stages preserve the six known-good file hashes, and the production build reads
the user's legacy presets without persistence errors. V14-09C now defaults all
mutable state to `%LOCALAPPDATA%\HallJoy`, performs a source-preserving one-time
transactional migration with a hash-verified backup, and uses one NFC,
case-folded, reserved-name-safe filename policy for profiles and presets. The
real first migration and replay launch passed with the Irok route connected and
balanced shutdown. V14-10A now captures the mapping creation result before any
later Win32 call can overwrite it, never clears a pre-existing Mouse IPC
payload, validates the stable 40-byte v1 schema, and uses interlocked reads for
peer-written connection fields. Simulator policy tests, the full build and an
Irok startup/route/shutdown smoke pass; an external ASI attach was not exercised.
V14-10B removes the analog-host named-object attack surface: its mapping,
events and owner-process capability are unnamed handles inherited only by the
created child through an explicit handle list. The v10 shared schema binds the
owner PID and CSPRNG launch token, and both sides validate child identity.
V14-10C replaces recv-bound request handling with an incremental HTTP/1.0/1.1
frame parser. It consumes exact `Content-Length` bodies, supports fragmented
and pipelined requests, rejects transfer coding, and enforces 8 KiB header,
4 KiB body and 2 KiB target limits. Client metrics use exact-key
`from_chars` conversion with a one-billion upper bound, so malformed,
duplicate and overflowing telemetry is rejected before counters change.
V14-10D delegates accepted clients to a fixed 16-worker ownership table, so
slow or fragmented clients cannot serialize the live state stream. Stop wakes
and joins every client before WSA cleanup; a simulator gate held eight partial
requests through application shutdown. Each server generation also creates a
128-bit CSPRNG session cookie, protects state and telemetry with that cookie,
accepts only the exact `http://127.0.0.1:<port>` browser origin and emits no
wildcard CORS policy. An already open overlay page automatically refreshes a
cookie from the new generation after an overlay restart. Simulator and
production socket gates, timeout
containment, the full build and the Irok route/balanced-shutdown smoke pass.
V14-11A replaces the zero-delay UAP vendor-request loop with a 1 ms
start-to-start deadline. Fast poll transports are capped at 1 kHz, a slow USB
transaction receives no extra delay, and transient Madlions failures back off
from 2 to 64 ms. Report-stream devices remain on their blocking path. The
portable rate model, static audit, rebuilt ABI identity/unload gate, full build
and production Irok regression pass. The Irok is a native SparkLink device, so
V14-11A is accepted by deterministic production-code tests, GCC/MSVC, and
Clang ASan+UBSan because no UAP poll keyboard is available; this verifies the
scheduler but makes no physical USB/latency claim. V14-11B now hashes the
normalized Soup HID interface path and descriptor, so identical devices keep
their own IDs regardless of enumeration order. The exact production function
passes all 40,320 permutations of eight devices, 100,000 reconnect generations,
250,000 synthetic paths without a collision, 1,024 pathless fallbacks and
persisted-ID golden vectors under the same three compiler/toolchain gates.
V14-11C replaces exclusive registry ownership with pinned `shared_ptr` owners.
Both snapshot exports copy at most eight owners under `devices_mtx`, release it,
and only then lock each device and copy telemetry or 256 values. Deterministic
blocked-reader removal, 100,000 lifetime cycles, 50,000 coherent snapshot
reads, three toolchains, sanitizers, the ABI gate, full build and Irok
regression pass. This proves lock/lifetime behavior, not physical UAP latency.

## Authoritative documents

- [ROADMAP.md](ROADMAP.md) - ordered packages, dependencies, and completion
  criteria.
- [RISK_REGISTER.md](RISK_REGISTER.md) - inherited and newly discovered risks.
- [VALIDATION_MATRIX.md](VALIDATION_MATRIX.md) - required gates and current
  evidence.
- [DECISIONS.md](DECISIONS.md) - decisions that constrain later changes.
- [WORKLOG.md](WORKLOG.md) - chronological implementation and validation log.
- [ANALOG_SIMULATOR.md](ANALOG_SIMULATOR.md) - simulator purpose, isolation
  contract, and repeatable gate.
- [PRIVATE_UAP_RUNTIME.md](PRIVATE_UAP_RUNTIME.md) - embedded runtime locations,
  integrity, diagnostics, and protected-directory fallback.
- [BUILD_REPRODUCIBILITY.md](BUILD_REPRODUCIBILITY.md) - dependency lock,
  local/CI commands, toolchains, and warning policy.

## Documentation rule

Every implementation package must update these documents in the same commit:

1. `ROADMAP.md` package status and next package.
2. `RISK_REGISTER.md` for risks changed, discovered, or closed.
3. `VALIDATION_MATRIX.md` with commands and actual results.
4. `WORKLOG.md` with the scope, commit, and remaining limitations.
5. `DECISIONS.md` when an architectural or release decision changes.

A package is incomplete if its documentation is stale, even when the code
builds.

## Status vocabulary

- `Planned`: scope is recorded, implementation has not started.
- `In progress`: implementation or required validation is incomplete.
- `Implemented`: code is complete, but at least one required gate is missing.
- `Verified`: all package-specific gates have passed and evidence is recorded.
- `Blocked`: progress requires an unavailable device or external state.
- `Deferred`: explicitly moved out of v1.4 with a documented reason.
