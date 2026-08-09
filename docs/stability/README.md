# HallJoy v3.9.0 stabilization record

This directory is the single coordination point for stability and performance
work that was required to preserve existing user-visible behavior. The
`v3.9.0` name records the imported baseline; later entries include the v1.4
release-hardening packages that continued from it.

## Documents

1. `STABILITY_MASTER_PLAN.md` — mandatory process, validation gates, and
   completion criteria.
2. `RISK_REGISTER.md` — every audit finding and its assigned work package.
3. `BASELINE_V3_9_0.md` — starting state, checksums, and reproducible checks.
4. `WORKLOG.md` — actual changes and validation results.
5. `DECISIONS.md` — architecture decisions and their rationale.
6. `AUDIT_V3_9_0.md` — complete initial technical audit.
7. `STAGE_S02A1_RESULT.md` — root cause and correction of the deterministic
   Soup patcher build blocker.
8. `STAGE_S02B1_RESULT.md` — exception barriers for MAD68 and Hex80 workers.
9. `STAGE_S02B2_RESULT.md` — combined C++/SEH boundary for SparkLink.
10. `STAGE_S02B3_RESULT.md` — exception containment for Addressed main and
    reader workers.
11. `VALIDATION_MATRIX.md` — available hardware, required gates, and rules for
    deferred external validation.
12. `STAGE_S02V1_RESULT.md` — temporary evidence tracing and machine acceptance
    criteria.
13. `TRACE_PROTOCOL_S02V1.md` — trace format, privacy/performance contract, and
    removal rules.
14. `STAGE_S06_RESULT.md` — ownership-safe Addressed overlapped-I/O cancellation
    and bounded outer shutdown.
15. `STAGE_S07_HEX80_RESULT.md` — bounded Hex80 lifecycle and timeout
    containment.
16. `STAGE_S07_MAD68_RESULT.md` — bounded MAD68 lifecycle with A9 recovery.
17. `baseline/` — machine-verifiable evidence from the imported baseline.

## Current status

- Stage 01 established the baseline and stabilization plan without changing
  production code.
- S01 added lifecycle contracts and test scaffolding.
- S02A and hotfix S02A.1 passed clean Windows/MSVC x64 Release builds and
  runtime smoke tests.
- Realtime, diagnostic-writer, and overlay workers use the shared
  allocation-free `noexcept` exception barrier.
- S02B.1 completed local MAD68 and Hex80 exception-containment work. The
  available SparkLink regression passed; unavailable MAD68/Hex80 hardware
  checks were deferred to device owners.
- S02B.2 passed Windows/MSVC and a real SparkLink hardware gate.
- S02B.3 completed local Addressed main/reader containment. Shared Windows and
  SparkLink smoke passed; the Addressed device gate was deferred.
- V14-12B/S06 removed cross-thread closure of active Addressed HID I/O, bounded
  the external stop to three seconds, and poisons the registry on an incomplete
  generation.
- V14-12C migrated Hex80 to waitable generations, cooperative cancellation,
  truthful three-second joins, and simulator-backed containment.
- V14-12D applied the same bounded ownership model to MAD68 while retaining its
  mandatory final A9 recovery.
- V14-12E added the normal-operation lifecycle runner. Its initial 25/25
  `WM_CLOSE` cycles exited cleanly with no remaining process, trace error, or
  user-state mutation.
- V14-12F/S18 removed the automatic privileged dependency installer. Missing
  ViGEmBus now produces only pinned manual guidance for version 1.22.0.
- V14-12G/S20 aligned documented build instructions with the actual scripts,
  folded Addressed checks into the unified runner, removed Win32/x86, and built
  both x64 configurations under `/W4` with no unexpected warnings.
- V14-12I completed the final normal-operation gate: 1000/1000 clean
  `HallJoy.exe` cycles, stable trace hashes, and no mutation of 11 user-state
  files.
- V14-12K found and corrected two false SparkLink stale/reconnect events caused
  by unsigned timestamp underflow during an attended soak. The saturating age
  helper passed 46 static audits, 28 portable tests, the official build, and
  targeted production regression.

The shared 45-item `HJ-AUD-*` register is synchronized with the release record:
0 Open, 3 Implemented, 1 Partial, and 41 Verified after V14-12G/S20. The
remaining Partial status is the deferred device-owner portion of
`HJ-AUD-P1-004`; Sayo and UAP code-level gates were completed by V14-06F and
V14-07C.

The V14-12K production executable recorded here had SHA-256
`81609DC44D12F7DF44C2A7D801D8992CBFDCB45221F07AE041ED4711F4EB840C`.
At that point the remaining release work was external Aula validation and the
manual hardware matrix, not unfinished S20 implementation.

This stabilization process did not write to GitHub or any other remote
repository.
