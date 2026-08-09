# HallJoy stabilization worklog

This is the edited chronological record for the imported v3.9.0 stabilization
stream and the v1.4 packages that completed it. Exact command output remains in
`baseline/` and `tests/`; package conclusions remain in `STAGE_*.md`.

## July 30, 2026: baseline and Stage 01

Recorded the original archive and source hashes, imported the complete audit,
created the 45-item risk register, master plan, decision record, validation
matrix, and worklog, and proved that documentation changed no production source,
project, or build file. The unified portable gate passed. All findings remained
Open because Stage 01 made no runtime correction.

## July 30, 2026: S01 lifecycle contracts

Added platform-neutral, generation-aware lifecycle and worker-primitive headers
plus exhaustive state/primitive tests. No production translation unit included
them. GCC/Clang strict-warning and sanitizer gates passed; the package
established a contract rather than claiming existing workers were fixed.

## July 30, 2026: S02A core exception boundaries

Added the allocation-free shared C++ barrier and applied it to realtime,
diagnostic writer, and overlay worker entries. Fault paths publish neutral or
closed state; owner resources still require explicit reap. Normal worker loops
were byte-identical to S01. Portable compiler/sanitizer/project gates passed;
Windows build remained mandatory.

## July 30, 2026: S02A.1 Soup patcher hotfix

A clean Windows build stopped because the patch validator required a marker that
the generated `$preOpenBlock` did not actually emit. Earlier audits had searched
the whole PowerShell file and produced a false PASS. Added the marker to the
generated C++ block, reused one marker variable, and added a contract audit that
extracts and validates the here-string and its pre-`CreateFileW` placement.

Runtime source and project files remained identical to S02A. Clean Windows
`BUILD.cmd`, HallJoy/ViGEm, overlay start/stop, and normal shutdown then passed.

## July 30, 2026: S02B.1 MAD68/Hex80 exception boundaries

Moved each normal algorithm into a body function and wrapped its `noexcept`
entry. Fault closes ownership/publications; completion always clears running;
start reaps a completed joinable thread before replacement. Hot paths remained
symbolically identical. Portable gates passed; available SparkLink regression
passed; device-specific MAD68/Hex80 evidence remained deferred.

## July 30, 2026: hardware validation policy

Adopted layered status: available SparkLink supplied a continuous shared device
gate, while missing MAD68/Hex80/Addressed/Sayo did not block narrowly bounded
packages with unchanged protocol bodies. Such packages remained Implemented,
never Verified, until owners tested one immutable pre-final artifact.

## July 30, 2026: S02B.2 SparkLink C++/SEH boundary

Preserved the outer SEH wrapper, added the inner shared C++ barrier, unified
neutral publication, recorded native fault code separately, and closed the
startup race where an already exited worker could be published connected. The
poll loop remained symbolically identical. Clean Windows/MSVC and physical
SparkLink input, modes, row limit, hotplug/reconnect, neutral release, ViGEm,
restart, and shutdown passed: `Verified on SparkLink`.

## July 30, 2026: S02B.3 Addressed main/reader boundaries

Added barriers to both worker layers, reader completion signaling, RAII handle
registration, protected reader reap before rethrow, stack-publication cleanup,
and main-generation reaping before replacement. Packet builders, polling and
reader loops remained token-identical. Fourteen audits, nine tests, compilers,
sanitizers, and shared Windows/SparkLink smoke passed. Cross-thread active-I/O
closure remained assigned to S06; Addressed hardware was deferred.

## July 30, 2026: S02V1 evidence trace

Introduced a compile-time 1 MiB memory-mapped trace, deterministic analyzer, and
post-close bundle collector. The hot path performs no filesystem write, flush,
dynamic queue, or per-key event. Sequence and append share one lock; overflow is
an explicit FAIL. Synthetic PASS/WARN/FAIL and all portable gates passed. The
trace was temporary acceptance infrastructure, not a runtime correction.

## August 1, 2026: V14-12B/S06 Addressed I/O ownership

Removed cross-thread closure of the reader HID handle. Reader-side
cancel-and-reap now owns handle, buffer, event, and `OVERLAPPED`; the outer main
generation has a truthful three-second join and retained-resource containment.
Ownership audit, 39 static audits, 26 tests, MSVC `/W4 /WX`, simulator timeout,
official build, and Irok regression passed. Physical Addressed remained
deferred.

## August 1, 2026: V14-12C/S07 Hex80 lifecycle

Migrated Hex80 to a waitable generation with cooperative cancellation, worker-
owned session handle, post-stop publication guard, and three-second containment.
Protocol files remained unchanged. Forty audits, 26 tests, MSVC, simulator,
official build, and Irok regression passed; physical Hex80 remained deferred.

## August 1, 2026: V14-12D/S07 MAD68 lifecycle

Migrated MAD68 similarly while canceling only its persistent read, forbidding a
new A8 after stop, and preserving mandatory final A9. Protocol, transport,
strategy, restore, and decoder remained unchanged. Forty-one audits, 26 tests,
MSVC, simulator, build, and Irok regression passed; physical MAD68 remained
deferred.

## August 1, 2026: V14-12E normal-operation pilot

Added a production runner that rejects fault/simulator arguments, targets the
exact child process, closes it through bounded `WM_CLOSE`, and checks exit,
trace, survivors, handles, and user-state hashes. The post-build 25/25 pilot
passed in 138-326 ms with 11/11 files unchanged and no accumulating handle
trend. The 1000-cycle and soak gates remained.

## August 1, 2026: status reconciliation

Synchronized all 45 shared IDs between the historical and current release risk
registers without erasing hardware/soak limitations. Statuses after the later
S20 package became 0 Open, 3 Implemented, 1 Partial, and 41 Verified.

## August 1, 2026: V14-12F/S18 dependency installer removal

Removed runtime download, verification, elevation, launch, and wait rather than
attempting to repair a privileged helper. Missing ViGEmBus now yields exact
manual-only 1.22.0 guidance pinned in the dependency lock. Negative primitive
audits, 43 static audits, 27 tests, MSVC `/W4 /WX`, official build, and Irok
3/3 passed.

## August 1, 2026: V14-12G/S20 build and documentation

Aligned TESTING, README, and build instructions with shipped v1.4 commands;
made the Addressed validator match catalog/lifecycle architecture and require it
in the unified runner; marked v3.9 validation as historical; removed unsupported
Win32/x86 configurations; and moved Debug/Release x64 to W4 with a narrow
allowlist. The current validator, 44 audits, 27 tests, clean official build, and
normal Irok 3/3 passed with no unexpected warning.

## August 1, 2026: V14-12H qualification automation

Made the lifecycle runner persistent and resumable with per-cycle checkpoint,
terminal result, state manifests, and Spark counters. Added a soak runner that
records handle/thread/GDI/USER/memory/CPU CSV data, checks overlay, performs
bounded close, analyzes trace, and applies post-warmup leak limits.

The one-minute pilot retained 210 handles, reduced private bytes by 86,016,
completed 234,846/234,846 Spark routes, produced no trace error or survivor, and
left 11 files unchanged. Post-build Irok 3/3 passed.

## August 1, 2026: V14-12I 1000 production cycles

Executable SHA-256
`6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`
passed 1000/1000 cycles. Every run exited zero, left no process, produced no
trace error/cap, preserved 11 state files, and generated the expected identical
trace hash. Shutdown min/average/p50/p95/p99/max was
`101/277.1/250/430/1315/2662 ms`; maximum handles 217. Spark totals were
1,598,454 successes and 425 expected shutdown-window cancellations.

## August 1, 2026: V14-12J unattended-soak sleep prevention

Added a Windows power request for the duration of unattended qualification with
guaranteed cleanup on every exit path. A corrected one-minute pilot confirmed
that the system remained awake and the request was released.

## August 1, 2026: V14-12K attended soak and Spark age correction

The attended run lasted 3,604.553 seconds with 703 resource samples, overlay
12/12, 92 ms shutdown, no surviving process, and unchanged user state. It found
two false SparkLink stale/reconnect transitions among 14,138,221 queries. The
root cause was unsigned timestamp underflow when observation preceded a racing
publication.

Replaced raw subtraction with a saturating age helper, added the observed high-
bit regression, aggregated every generation in soak evidence, and rejected
impossible ages. Forty-six audits, 28 portable tests, and the official build
passed. Corrected executable SHA-256:
`81609DC44D12F7DF44C2A7D801D8992CBFDCB45221F07AE041ED4711F4EB840C`.
A targeted two-minute production run completed 464,905/464,905 routes with zero
stale/reconnect, survivor, or state mutation. Because the correction was narrow,
deterministic, and fully covered, the already completed hour and 1000-cycle gate
were not repeated blindly; external hardware gates remained independent.
