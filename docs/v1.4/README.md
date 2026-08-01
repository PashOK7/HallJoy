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
- Current package: `V14-10` in progress
- Next work: `V14-10B` analog-host IPC precreation/spoofing resistance
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
