# HallJoy v3.9.0 stabilization risk register

Source: `AUDIT_V3_9_0.md`. The v3.9.0 label identifies the imported baseline;
later v1.4 packages updated these statuses. Detailed evidence is in
`WORKLOG.md`, `VALIDATION_MATRIX.md`, and the stage-result documents.

Status meanings:

- `Open`: proven issue with no accepted production correction;
- `Partial`: only part of the issue has passed its required gates;
- `Implemented`: code correction is complete, but a required platform/hardware
  gate remains;
- `Verified`: every mandatory gate for this record passed;
- `Deferred`: explicitly postponed and not counted as corrected.

| ID | Priority | Finding | Package | Status | Acceptance evidence |
|---|---|---|---|---|---|
| `HJ-AUD-P1-001` | P1 | `TerminateThread` used as a normal shutdown mechanism | `S04/S05/S08` | Implemented | normal occurrences removed in V14-06A-F; final soak/device qualification remained |
| `HJ-AUD-P1-002` | P1 | supposedly bounded shutdown in three native backends could still hang indefinitely | `S06/S07` | Implemented | Addressed, Hex80, and MAD68 bounded join/retention/containment gates passed in V14-12B-D; physical device qualification remained |
| `HJ-AUD-P1-003` | P1 | backend `stop()` contract could report false success | `S03` | Verified | generation-scoped result, poison-on-failure, and registry tests passed in V14-05 |
| `HJ-AUD-P1-004` | P1 | key worker entries lacked complete C++ exception boundaries | `S02` | Partial | UAP workers/C ABI passed V14-07C; deferred device-owner/final-soak gates remained |
| `HJ-AUD-P1-005` | P1 | Addressed closed a HID handle cross-thread while stack `OVERLAPPED` remained active | `S06` | Implemented | reader-only close, cancel-and-reap audit, and simulator containment passed; hardware gate deferred |
| `HJ-AUD-P1-006` | P1 | analogue host could reinitialize over an incomplete generation | `S09` | Verified | parent generation, bounded group timeout, retained ownership, and restart rejection passed in V14-07A |
| `HJ-AUD-P1-007` | P1 | partial-start cleanup could unmap IPC beneath a live bridge thread | `S09` | Verified | injected supervisor-start failure joined the bridge before rollback in V14-07A |
| `HJ-AUD-P1-008` | P1 | manual-reset event handling could lose a device-change wake | `S11` | Verified | monotonic wake sequence, race tests, and production Irok smoke passed in V14-08A |
| `HJ-AUD-P1-009` | P1 | startup ignored realtime and dependent native-phase failures | `S11` | Verified | explicit commit/reverse rollback and phase fault injection passed in V14-08A |
| `HJ-AUD-P1-010` | P1 | synchronous ViGEm update ran inside the realtime worker | `S12` | Verified | dedicated output owner, equivalence tests, and bounded driver-stall containment passed in V14-08B |
| `HJ-AUD-P1-011` | P1 | mouse IPC misdetected whether a mapping was newly created | `S15` | Verified | immediate creation-disposition capture and preserve/validate self-test passed in V14-10A |
| `HJ-AUD-P1-012` | P1 | named analogue-host IPC allowed same-session precreation/spoofing | `S15` | Verified | inherited-handle transport, owner/token schema, identity checks, and invalid-handle restart gate passed in V14-10B |
| `HJ-AUD-P1-013` | P1 | built-in dependency installer had TOCTOU and supply-chain gaps | `S18` | Verified | automatic download/elevation removed; exact ViGEmBus 1.22.0 manual page pinned in V14-12F |
| `HJ-AUD-P1-014` | P1 | dependency installation could freeze the UI indefinitely | `S18` | Verified | installer launch/wait paths removed; guidance policy and build regression passed in V14-12F |
| `HJ-AUD-P1-015` | P1 | UAP C ABI leaked exceptions and used manual lock/unlock | `S10` | Verified | C ABI barrier, RAII tests, and no-manual-lock audit passed in V14-07C |
| `HJ-AUD-P1-016` | P1 | UAP unload performed unbounded joins while holding the device mutex | `S10` | Verified | bounded joins moved outside the mutex; ABI unload and child containment passed in V14-07C |
| `HJ-AUD-P2-001` | P2 | overlay served only one client synchronously | `S16` | Verified | fixed 16-client table, slow/parallel/limit gates, and active-client shutdown passed in V14-10D |
| `HJ-AUD-P2-002` | P2 | overlay HTTP parser lacked framing | `S16` | Verified | incremental fragmentation/pipelining/body parsing and strict size/encoding rejection passed in V14-10C |
| `HJ-AUD-P2-003` | P2 | overlay telemetry parser allowed unsigned overflow | `S16` | Verified | exact-key `from_chars`, duplicate/junk/overflow rejection, and one-billion bound passed |
| `HJ-AUD-P2-004` | P2 | overlay endpoint accepted arbitrary origins | `S16` | Verified | 128-bit session cookie, exact loopback origin, and hostile/null-origin rejection passed in V14-10D |
| `HJ-AUD-P2-005` | P2 | mouse IPC used ordinary volatile reads for interlocked peer fields | `S15` | Verified | peer-owned schema/heartbeat fields use interlocked reads; audit and simulator passed in V14-10A |
| `HJ-AUD-P2-006` | P2 | UAP polling workers had no pacing | `S17` | Verified | deadline pacing and failure-backoff policy/property/sanitizer/build gates passed in V14-11A |
| `HJ-AUD-P2-007` | P2 | identity of two identical UAP devices was unstable | `S17` | Verified | path identity passed exhaustive reorder/reconnect stress, golden vectors, GCC/MSVC, and sanitizers in V14-11B |
| `HJ-AUD-P2-008` | P2 | plugin `is_initialised()` always returned true | `S10` | Verified | runtime ABI gate proved false-before, true-after, and false-after bounded unload |
| `HJ-AUD-P2-009` | P2 | `_device_info` failed to validate a null buffer | `S10` | Verified | ABI gate proved null and full-buffer calls return zero safely |
| `HJ-AUD-P2-010` | P2 | snapshot export held the global device mutex during value copying | `S17` | Verified | bounded owner pins, blocked-reader removal, lifetime/coherence stress, and ABI gates passed in V14-11C |
| `HJ-AUD-P2-011` | P2 | `settings.ini` save could replace a valid file with an incomplete temporary file | `S13` | Verified | unique same-directory temp, checked flush/readback/replace, and five failure stages preserved the known-good hash |
| `HJ-AUD-P2-012` | P2 | overlay settings wrote directly and always reported success | `S13` | Verified | transactional copy, checked writes/readback, and exact failure-stage reporting passed |
| `HJ-AUD-P2-013` | P2 | profile stream state was not checked after flush/close | `S13` | Verified | flush/good/close checks, schema readback, and binding fault probes passed |
| `HJ-AUD-P2-014` | P2 | layout presets were truncated and rewritten non-atomically | `S13` | Verified | checked transaction, exact-entry readback, and five injected stages preserved the layout hash |
| `HJ-AUD-P2-015` | P2 | curve-preset writes ignored I/O results | `S13` | Verified | checked preset/state writes, exact readback, and five stages preserved both hashes |
| `HJ-AUD-P2-016` | P2 | most save callers ignored returned errors | `S13` | Verified | central trace/UI reporting plus rollback/retained state covered automatic and manual paths |
| `HJ-AUD-P2-017` | P2 | writable state lived beside the executable | `S14` | Verified | LocalAppData default, explicit writable portable marker, and source-preserving transactional migration passed |
| `HJ-AUD-P2-018` | P2 | profile names lacked adequate normalization and path policy | `S14` | Verified | shared NFC/case/reserved-name/length/direct-child policy and Unicode collision tests passed |
| `HJ-AUD-P2-019` | P2 | curve settings used a weak memory-order contract | `S11` | Verified | release publication, acquire cache observation, and concurrency test passed in V14-08A |
| `HJ-AUD-P2-020` | P2 | lifecycle registry was not serialized and did not enforce thread affinity | `S03` | Verified | owner-thread registry plus wrong-thread tests passed in V14-05 |
| `HJ-AUD-P2-021` | P2 | VID:PID ownership was too coarse | `S17` | Verified | exact-path claims, same-pair sibling/collision tests, native/Soup gates, toolchains, ABI/build, and Irok regression passed in V14-11D |
| `HJ-AUD-P3-001` | P3 | `TESTING.md` described a nonexistent test flow | `S20` | Verified | shipped commands, unified runner, and documentation audit passed |
| `HJ-AUD-P3-002` | P3 | internal README/build documents belonged to older generations | `S20` | Verified | current v1.4 x64 build/runtime/storage guidance and stale-marker audit passed |
| `HJ-AUD-P3-003` | P3 | an Addressed static audit no longer matched current architecture | `S20` | Verified | validator rewritten for catalog/lifecycle and required by the unified runner |
| `HJ-AUD-P3-004` | P3 | validation documents overstated lifecycle coverage | `S20` | Verified | v3.9 record marked historical; current matrix owns release claims |
| `HJ-AUD-P3-005` | P3 | Win32 configuration referenced an x64 ViGEm library | `S20` | Verified | unsupported Win32/x86 configurations removed; exact x64 audit/build passed |
| `HJ-AUD-P3-006` | P3 | project compiled at Warning Level 3 | `S20` | Verified | Debug/Release x64 moved to W4 with a narrow documented baseline and zero unexpected build warnings |
| `HJ-AUD-P3-007` | P3 | root README omitted real build dependencies | `S20` | Verified | dependency list and independent clean-room build passed |
| `HJ-AUD-P3-008` | P3 | Sun build tool followed a moving branch | `S20` | Verified | exact locked commit, fresh fetch, and independent build passed |

## Status summary

The register contains 45 findings: 0 Open, 3 Implemented, 1 Partial, and 41
Verified after V14-12G/S20 reconciliation. “Implemented” and “Partial” remain
deliberately distinct from “Verified”; deferred physical-device and final-soak
requirements were never silently promoted by static tests.

Package-level implementation narratives are retained in `STAGE_*.md`; exact
commands and evidence artifacts are indexed by `VALIDATION_MATRIX.md` and
`tests/README.md`.
