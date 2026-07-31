# HallJoy v1.4 development

This directory is the authoritative source for v1.4 scope, status, decisions,
risks, and validation. Documents under `docs/stability` preserve the imported
archive history and evidence, but they do not define the current product
version or release status.

## Current status

- Product version: `v1.4` (in development)
- Working branch: `v1.4-integration`
- Completed package: `V14-06` cooperative per-worker lifecycle migration
- Next package: `V14-07` analog host and UAP boundary hardening
- GitHub publication: not started
- Release status: not release-ready

This workstation has a membrane keyboard. Common pipeline behavior is verified
with the development-only simulator, but protocol-specific hardware status
must not be inferred from those tests.

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
