# HallJoy v1.4 risk register

## 2026-09-06 — current RM-36 reconciliation

The historical `RISK_TEST_MANIFEST_V1.json` records the original registration
state, so its `MISSING` entries must not be used as a current release verdict.
Every P0/P1 is now assigned a current, deliberately bounded disposition in
[RM36_RISK_RECONCILIATION_2026-09-06.md](RM36_RISK_RECONCILIATION_2026-09-06.md).
Hardware and signing rows remain open there; source-local evidence is never
promoted to device support by this reconciliation.

## 2026-09-06 — Owner clarification: preserve automatic Sayo letter mapping

The owner requires automatic user-configured letters with no manual assignment,
setup wizard or mandatory confirmation, and reports no complaints about current
behavior. This supersedes earlier blanket prohibitions on Sayo letter learning:
physical depth remains measured independently; automatic letter association is
intentional. RM-03 now preserves it and tests ambiguous correlation (addedCount==1
already guards multiple new HID keys in one report; cross-report candidates need
review). No per-press activation-threshold delay was established. Treat stale-depth
fallback separately according to its intended digital/analog contract. Do not
remove Sayo, replace letters with physical-only controls, or label learning itself
P0. Reconcile old HJ-V14-P0-003 subclaims rather than copying its blanket verdict.
See FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md v1.1, section 0, for other potential
intent traps: fallback/shadow removal, single-instance, async save, process
isolation and release scope. Proposed rewrites require Purpose/compatibility
review and evidence; product behavior changes require an explicit product decision.
Only documentation changed in this clarification; no new runtime PASS is claimed.


## 2026-09-06 — Full audit execution roadmap (planning, not qualification)

New detailed execution entry point:
[FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md](FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md).
It contains 37 packages / 185 steps, 17 protocol-family review cards, dependencies,
negative checks and acceptance criteria, plus 234-file inventory with review depth
and hashes. All implementation tasks start TODO; historical P0/P1 and release gates
remain authoritative until reconciled with current source/evidence. Current reads
confirm Sayo digital-derived mapping and the SparkLink row-freshness gap; first
packages after RM-00/01 are RM-03/04/05. Native contract, containment and host DLL
trust also require dedicated work. Production files and EXE were not changed;
no runtime/input/controller/hardware tests ran for this audit. A comprehensive
roadmap is not a claim of a completed line-by-line or hardware audit.


## 2026-09-05 — Profile audit remediation (D-079)

The eight findings F-01–F-08 in INDEPENDENT_CODE_AUDIT_2026-09-05.md are
implemented in source: bounded layout parsing; prepare-before-switch and guarded
active deletion; strict bindings validation without mutation on failure; complete
1033-key CSV parsing; checked numeric conversion; staged settings/bindings commit
behind a nonblocking realtime reader gate; and one atomic settings+bindings INI.
Legacy pairs remain readable. First bundled save preserves the old settings as
`.pre-bundle.bak` and leaves the old bindings file untouched. Older executables
require restoring the matching legacy pair; see README storage instructions.

A further startup/shutdown defect was found during validation: final shutdown
saved partial defaults after a rejected profile load. Autosave is now enabled
only after successful initialization. The file-only rejected-startup probe
checks exit code 1 and byte-identical settings and bindings after final shutdown.

Evidence: production-linked file/memory tests pass layout bounds, malformed
profiles, full CSV domain, legacy migration, switch/delete failure, 100 concurrent
profile loads with zero mixed reads, and all five atomic-save failure stages.
The test runner is `tools/run_profile_transaction_tests.ps1`; its explicit modes
exit before Backend_Init and never produce gamepad reports. The unified native
checks and ordinary MSVC build also pass; final artifact identity is recorded in
the remediation evidence directory and handoff after packaging.

Limits: the earlier storage migration simulator passed migration, replay and five
migration failure stages, but its portable-mode run exited on rejected existing
profile data. That full runtime suite is NOT qualified by this change. Synthetic
WASD mixed with live hardware first exposed an oracle contamination issue; the
optional simulator-only isolated input mode addresses its source selection, but
still emits gamepad reports. User is playing CS: do not run input/controller
simulation (including this isolated mode). Hardware/ViGEm validation and the full
portable runtime rerun remain pending. A separate fresh, marker-selected portable
file-only startup probe passed (exit 0 before backend/window creation); its log is
included in the remediation evidence. Existing protocol/release blockers remain
open. The root-level HallJoy.exe is not the new build.

Backups: `.analysis/backups/profile_audit_fixes_20260905_175549` (sources/docs)
and `.analysis/backups/profile_fixes_artifacts_20260905_181448` (build artifacts).

This register was inherited intact from the imported advanced archive and is
now authoritative for v1.4. The old `v3.9.0` name identifies provenance only.
Statuses remain conservative until the corresponding v1.4 package reruns every
required gate.

Source audit: `docs/stability/AUDIT_V3_9_0.md`. Current package mapping is
maintained in `ROADMAP.md`.

## Additional v1.4 integration risks

| ID | Priority | Description | Package | Status | Required validation |
|---|---|---|---|---|---|
| `HJ-V14-P0-001` | P0 | The private UAP/HallJoy key domain drops Fn, media, brightness and OEM codes above 255; Context/Menu is missing from both HID conversion directions and may collapse to invalid code 0 | UAP correctness follow-up | In progress; full-key same-generation UAP export/capture PASS, production migration pending | `KeyIdentityV1` distinguishes USB keyboard/consumer, UAP extended and HallJoy semantic domains; one pinned export returns V2 plus an exactly checked 256-cell ordinary-HID view, the exact DLL returned 127 namespaced samples, and the exact production EXE captured a coherent non-empty parent snapshot. Bindings/UI/profile/output migration, configured XUSB shadow, per-family regressions and physical confirmation remain required. |
| `HJ-V14-P0-002` | P0 | UAP family parsers and lifecycle contain open cross-family defects: unsafe NuPhy/Keychron short-report handling, disabled hotplug/ghost connectivity, partial Madlions failure accounting and family-specific validation gaps | UAP correctness follow-up | Open | complete `UAP_ALL_KEYBOARDS_AUDIT.md` P0/P1 gates, malformed-report tests, reconnect/hotplug tests and representative hardware qualification |
| `HJ-V14-P0-003` | P0 | Production Sayo derives physical analog-index identity from a later binary boot-keyboard event, starts with false F/G/H mappings and fabricates full analog 1000 when depth is stale | Native correctness follow-up | Open, static proof | replace binary-derived identity with firmware/model physical map, remove fabricated depth, modifier/NKRO/simultaneous regressions and physical O3C qualification |
| `HJ-V14-P0-004` | P0 | W669 can remain connected forever after its live-event stream stops and can retain a lost-release nonzero value indefinitely | Native correctness follow-up | Open, static proof | valid-live-event silence deadline, immediate neutralization, reconnect/snapshot recovery, lost-release and unplug tests on representative hardware |
| `HJ-V14-P0-005` | P0 | The realtime publisher can call `SetEvent` on the ViGEm output wake handle while watchdog recovery closes and recreates that ordinary global handle, causing a C++ data race and stale/reused Win32 handle use | Concurrency/lifecycle/ownership follow-up | Implemented; F3 route/O1/local O5 PASS; hardware pending | F3 atomically removed the legacy thread/mailbox/closeable wake and routes realtime only through an outer-leased process-lifetime session. Exact self-host, real ViGEmBus and routed stall recovery prove admission-close/reap/drain-before-replacement and zero survivor. O5 adds generation-bound topology and passes 20 runs/2,020 child generations without rebuild or survivor. Require long active-input soak plus the immutable final candidate on IROK and DrunkDeer before Verified. |
| `HJ-V14-P0-006` | P0 | Physical IROK first lost the legacy output-thread handle; a later physical GravaStar V75 run proved the F3 process route could likewise lose its parent-owned child-process handle (`ReapFailed`, `ERROR_INVALID_HANDLE`) after 135.6 s, freeze output and enter 218 blocked watchdog rebuild attempts while analogue input remained healthy | Concurrency/lifecycle/ownership follow-up | Reopened by physical F3 evidence; protected-owner correction implemented/local PASS; hardware pending | The sole generation owner now applies kernel-enforced `HANDLE_FLAG_PROTECT_FROM_CLOSE` to both process and job handles, verifies protection and PID identity, and removes protection only for confirmed owner close. An illicit-close Windows regression, 1,008-generation process suite, exact self-host/real-child/runtime-stress suites and production build pass. Unsafe recovery is now one-shot fail-closed with a complete `watchdog.recovery_blocked` record instead of an unbounded retry storm. Require a long exact corrected-artifact V75/IROK output-continuity run before Verified. |
| `HJ-V14-P1-009` | P1 | Hex80 immediately publishes partial chunks and resets its failure streak after every good chunk, so one permanently failing chunk can retain stale values forever while connected | Native correctness follow-up | Open, static proof | staged atomic matrix generations, per-chunk/whole-cycle failure accounting, stale neutralization and malformed/partial physical session tests |
| `HJ-V14-P1-010` | P1 | Addressed `09/94/02` applies a QBZ canonical physical map to any protocol-compatible candidate with fewer than 20 dynamic entries, although admission proves only a small WASD response set | Native correctness follow-up | Open, static proof | identity-scoped fallback or sufficiently complete device map, variant-layout negative tests and representative hardware qualification |
| `HJ-V14-P1-011` | P1 | Native ABI accepts 16-bit key identities but implementations store only 256 values; Fn/OEM/media are dropped or represented by an unversioned pseudo-HID | Shared UAP/native key-domain follow-up | In progress; R2 identity foundation PASS, adapters pending | The versioned non-byte identity and collision/invalid tests are production-compiled. Native descriptors/storage, serialization/UI/overlay and family-specific hardware still use the legacy route and must migrate. |
| `HJ-V14-P1-012` | P1 | SparkLink can accept a truncated layout, drops codes above 255, uses last-row-wins duplicate semantics and assumes a 3000..5000 scale not proven by capability discovery | Native correctness follow-up | Open, static proof | complete layout-generation proof, explicit scale, duplicate max aggregation, malformed-row tests and sibling hardware qualification |
| `HJ-V14-P1-013` | P1 | Several dynamic native admissions prove a wire protocol but then apply a fixed physical layout that was not proven for the admitted sibling PID/firmware | Native correctness follow-up | Open, static proof | separate protocol and layout evidence, fail-closed unknown layouts and physical position spot-checks for every claimed sibling |
| `HJ-V14-P1-014` | P1 | Digital fallback source arbitration is a global hand-maintained five-backend list: it omits Aula MAX/W669, disables unrelated digital keys for listed devices and can mix simulated Windows key state with zero-valued UAP input | Common analog pipeline follow-up | Open, static proof | catalog-driven per-device/per-HID source policy, explicit measured-versus-simulated identity and simultaneous analog/digital/UAP/native tests |
| `HJ-V14-P1-015` | P1 | Profile loads do not validate or stage complete settings/bindings, mutate live output piecemeal and report success for any existing path; the global switch persists the new name and ignores both load results | Common analog pipeline follow-up | Open, static proof | immutable parse/validate model, atomic whole-profile generation commit, pair transaction/rollback, malformed/partial/wrong-kind/concurrent-realtime tests |
| `HJ-V14-P1-016` | P1 | End-to-end key identity is inconsistent: axes/triggers accept 16-bit values while buttons, UI, capture and overlay are 8-bit; signed/overflow INI values can wrap into hidden `uint16_t` bindings | Common key-domain follow-up | In progress; R2 identity foundation PASS, surfaces pending | The common identity now represents Fn/Menu/media/OEM without namespace collision. Bindings, strict no-wrap profile migration, UI, capture, overlay and output remain on legacy numeric surfaces and are not yet qualified. |
| `HJ-V14-P1-017` | P1 | The native backend ABI irreversibly quantizes every source to 1001 levels before curves and adds hidden source-domain deadzones, although XUSB sticks have a much larger output domain and UAP retains float precision | Precision/performance follow-up | In progress; R2 precision foundation PASS, native migration pending | `AnalogValueV2` retains float plus optional raw numerator/domain; adjacent 12-bit values that both become milli zero remain distinct. Native providers, profiles, curves/output and hardware range/noise qualification still require migration. |
| `HJ-V14-P1-018` | P1 | Generic UAP per-key reads repeatedly copy and scan complete 256-value merged/per-device snapshots under an exclusive lock, while unchanged poll generations still traverse child IPC and wake the realtime loop | Precision/performance follow-up | Open, static proof | one immutable snapshot acquisition per generation/tick, O(1) key lookup, changed-generation signalling, bind-capture changed set, complexity/load tests and UAP physical profiling |
| `HJ-V14-P1-019` | P1 | Independent hard-coded 1 kHz limits in UAP poll pacing and XUSB publication are not derived from device or ViGEm capabilities and can discard the throughput advantage of 2/4/8 kHz sources while still allowing redundant high-rate pipeline work | Precision/performance follow-up | Open, static proof | unified observable rate policy, source/change/tick/submit telemetry, safe adaptive/configurable ceilings, ViGEm load/latency tests and representative 1/2/4/8 kHz hardware evidence |
| `HJ-V14-P1-020` | P1 | UAP stores a dynamic device vector, the parent tracks 16 IDs, but private dense snapshot/telemetry silently truncates to 8 devices, making identity and per-device reads incomplete without a truncation signal | Precision/performance follow-up | In progress; producer/layout and B2i-b exact Windows lifecycle PASS; B2i-c pending | Provider V2 captures exact owner demand and B2i-b provides a variable section with exact growth after child reap. Real Windows tests pass 1/8/12/32/8/1 generations, crash/resize/forged-handle/zero-survivor gates; the final packaged EXE also passes isolated physical zero-capacity demand, one controlled restart and a non-empty read-only commit. Fixed live `SharedState` arrays remain until B2i-c. |
| `HJ-V14-P1-021` | P1 | The analog-host IPC is one bidirectional all-access fixed `SharedState` that couples control, high-rate values, device telemetry and diagnostics under one snapshot ABI/event, so unrelated schema/rate changes contend and cannot negotiate capacity independently | Provider architecture follow-up | In progress; B2i-b separate Windows ownership and exact physical gate PASS; live migration pending | D-074/B2i-a define the plane; B2i-b proves parent read-only/child-only writer rights, explicit inheritance, odd-commit rejection, reader-held replacement exclusion, reaped topology changes and the exact physical restart/commit path. Control/dense rollback remains in `SharedState`; B2i-c must migrate the live shadow, remove fixed V2 payload arrays and qualify the engineering route. |
| `HJ-V14-P1-022` | P1 | UAP and native sources have no common provider contract: they diverge in precision, identity, capacity, freshness, merge and publication semantics, while the core consumes UAP through a compatibility SDK facade and native through a separate per-HID ABI | Provider architecture follow-up | In progress; common contract and same-generation read-only UAP capture PASS | One pinned UAP acquisition now produces V2 and its exactly checked compatibility view, IPC V12 labels coherent publication, and the exact production EXE captures a non-empty parent snapshot. Configured-XUSB shadow, route switch, native adapters, merge/lost-release/reconnect tests and removal of per-key realtime facades remain mandatory. |
| `HJ-V14-P1-023` | P1 | UAP protocol I/O is crash-contained in a disposable child, but native protocol workers and storage live in the UI/realtime process, so a native transport hang, SEH fault or memory corruption has a larger failure domain and inconsistent restart semantics | Provider architecture follow-up | Open, static proof | self-hosted native provider process using the common IPC, provider-scoped neutralization, bounded hang/crash/restart/survivor gates and representative physical native qualification |
| `HJ-V14-P1-024` | P1 | Mandatory tests, build preflights, development contracts and the backend generator actively require known-wrong legacy behavior: disabled UAP hotplug, milli `[0..1000]`, 256 key slots and fixed 1 kHz pacing | Test/evidence trust follow-up | Open, static proof | versioned migration contracts, old-bug negative fixtures and removal of every anti-fix requirement without losing compatibility coverage |
| `HJ-V14-P1-025` | P1 | Source-token and independent-model checks can remain green with dead/unwired code or broken runtime semantics; current labels overstate production coverage and no implementation-mutation or common coverage evidence validates the critical oracles | Test/evidence trust follow-up | Open, static proof | production-linked pure-core tests, exact wiring tests, honest static/model labels, selected P0/P1 mutation kill matrix and machine coverage reporting |
| `HJ-V14-P1-026` | P1 | Official build and CI publish a green artifact without executing sanitizer/fuzz, production smoke, fault matrix, release qualification, soak, migration/reset and hardware shutdown gates; runner filename presence is sometimes mistaken for invocation | Test/evidence trust follow-up | Open, executable proof | tiered fast/integration/release runners, machine manifest, missing-gate failure and exact final-artifact evidence chain |
| `HJ-V14-P1-027` | P1 | Sanitizer claims exceed actual parser coverage, Windows interceptor warnings are ignored and no known-bad health binary proves that the sanitizer would detect a violation | Test/evidence trust follow-up | Open, executable proof | all-parser sanitizer matrix, malformed/stateful corpus, mandatory failing health control, stderr classification and Windows/Linux result manifest |
| `HJ-V14-P1-028` | P1 | Desktop tracked HIDs are published by clearing an atomic count, rewriting a plain array and restoring the count; a realtime reader that already loaded the old count can race the rewrite and observe undefined or mixed state | Concurrency/lifecycle/ownership follow-up | Open, static proof | immutable/generation-coherent tracked snapshot, forced writer/reader interleaving, old-or-new-only invariant, production UI/realtime wiring and race-detector gate |
| `HJ-V14-P1-029` | P1 | Worker lifecycle state is not generation-bound to ready/completion/fault: production never calls `MarkFaulted`, native registry can remain `Running` after its worker exits, and several starts can report success before a ready acknowledgement | Concurrency/lifecycle/ownership follow-up | Open, static proof | generation-tagged ready and terminal callbacks, truthful registry state, stale-callback rejection, early-exit/fault/stop matrix and non-overlapping native recovery |
| `HJ-V14-P1-030` | P1 | `Backend_Tick` directly performs SparkLink/Sayo hotplug Start/Stop, HID discovery/proof and joins, so a provider reconnect can block the realtime/XUSB path for hundreds of milliseconds, seconds or longer | Concurrency/lifecycle/ownership follow-up | Open, static proof | move provider lifecycle outside realtime, immutable snapshots, injected slow lock/enumeration/proof/join tests, tick-duration bound and second-provider continuity evidence |
| `HJ-V14-P1-031` | P1 | Native HID operation timeouts call `CancelIoEx` and then an unbounded `GetOverlappedResult(..., TRUE)` drain; this preserves OVERLAPPED memory safety but a driver that never completes cancellation can hang a worker or shutdown forever | Concurrency/lifecycle/ownership follow-up | Open, static proof | never-completing-I/O oracle, provider process hard deadline/kill, neutralization/restart/no-survivor and repeated hang resource-leak gates; no early OVERLAPPED destruction |
| `HJ-V14-P1-032` | P1 | The global shutdown deadline depends on creating its watchdog event and thread only after shutdown begins; allocation failure merely logs and leaves every later join/storage/logger stage without a process liveness bound | Concurrency/lifecycle/ownership follow-up | Open, static proof | pre-provisioned or external shutdown supervisor, event/thread failure injection, resource-exhaustion WM_CLOSE, per-stage hang matrix and exact-EXE zero-survivor deadline |
| `HJ-V14-P1-033` | P1 | HallJoy has no global neutralize/release/resume transaction or single application owner, so a web driver or second HallJoy process can conflict with live UAP/native HID sessions; stopping only realtime would leave handles, hooks, virtual targets or stale protocol generations alive | Global pause/device lease follow-up | Open, static proof | `GLOBAL_PAUSE_DEVICE_LEASE_AUDIT.md`: one session owner, neutral-before-release, bounded stop of every provider, zero HID/child/hook/virtual-target survivors while paused, explicit new-generation resume, external-busy state, 1000 cycles and representative physical web-driver tests |
| `HJ-V14-P1-034` | P1 | The internal analog-host validates inherited handles/PID/nonce but accepts an arbitrary command-line DLL path and only checks ABI exports; a self-consistent foreign owner can make HallJoy load its DLL, and the normal resource comparison has a verify/close/load path-swap window | Trust/security boundary follow-up | Open, static proof | forged-owner/arbitrary-DLL negative oracle, child-side exact embedded digest, proven owner image, non-reparse owner-only runtime, no write/delete swap window, safe DLL search and signed-proxy rejection |
| `HJ-V14-P1-035` | P1 | External `LayoutPreset.Count` is checked only for `>0` before two vector reserves and a count-sized loop, allowing a malformed INI to drive huge allocation/work and crash or stall startup before semantic validation | Trust/security boundary follow-up | Open, static proof | common document byte/entry/string limits before allocation, exact count/full parse, immutable commit, max-int/huge/truncated/allocation-failure corpus and unchanged-old-generation evidence |
| `HJ-V14-P1-036` | P1 | Support diagnostics can disclose full HID paths, raw serials, absolute user paths and key-travel sequences, while untrusted descriptor control characters can forge text-log structure; no common privacy schema, consent or artifact scan exists | Trust/security boundary follow-up | Open, static proof | structured sanitized logger, per-log salted identities, logical path roots, explicit raw-input consent/data classes, control/bidi/length sanitization and ready-to-send artifact privacy scan |
| `HJ-V14-P1-037` | P1 | The distributed HallJoy EXE and embedded UAP build are Authenticode `NotSigned`; an adjacent self-generated SHA256 file detects accidental corruption but does not authenticate the publisher or release chain | Trust/security boundary follow-up | Open, executable proof | close arbitrary DLL load first, exact-artifact Authenticode plus timestamp, signed provenance manifest with source/dependency/toolchain/resource hashes, wrong-signer/post-sign mutation tests and no rebuild after qualification |
| `HJ-V14-P1-038` | P1 | A physical IROK shutdown was poisoned by `sayo stop.lock_timeout` after 500 ms even though SparkLink was the active route; an absent or unrelated backend can therefore fail the whole native shutdown and create a synthetic crash sidecar | Concurrency/lifecycle/ownership follow-up | Open, physical proof | identify lifecycle-lock owner/hold interval, serialize stop/start/proof outside realtime, absent-backend and concurrent-hotplug shutdown oracles, bounded zero-survivor exit and no false crash classification |
| `HJ-V14-P1-039` | P1 | SparkLink uses one global failure streak and freshness timestamp: a failed row advances to a successful neighbour that resets both, so one permanently unreadable row can retain stale nonzero values forever while the device remains globally connected and fresh | Native correctness/concurrency follow-up | Open, static proof | per-row and complete-cycle freshness, staged or equivalently coherent publication, permanently/alternately failing-row old-bug oracles, stale-row neutralization, lost-release/reconnect tests and physical IROK regression |
| `HJ-V14-P1-001` | P1 | Private UAP extraction beside the EXE fails in protected locations | `V14-03` | Verified | forced protected-directory fallback, atomic repair, exact hash and child-load gates passed |
| `HJ-V14-P1-002` | P1 | Legacy recovery recommends a system Wooting SDK that the embedded architecture does not use | `V14-03` | Verified | installer path removed; static diagnostic truthfulness gate passed |
| `HJ-V14-P1-003` | P1 | Imported product/build strings expose the wrong public version | `V14-01` | Verified | version-resource and active-document scan passed |
| `HJ-V14-P1-004` | P1 | SparkLink hotplug watchdog can reconnect a worker after shutdown has already stopped the current generation | `V14-06D.1` | Verified | Irok held-key unplug/reconnect, analog recovery, balanced generations, final reconnect suppression and clean shutdown passed |
| `HJ-V14-P1-005` | P1 | One-shot overlay HTTP connections can block `/state` for the five-second keep-alive receive timeout | `V14-06C.1` | Verified | captured 5.001-5.002 s fetch stalls; simulator and production socket gates now return the next `/state` in under 1 ms |
| `HJ-V14-P1-006` | P1 | Sayo freshness timestamps can underflow when a worker publishes a newer timestamp than the UI tick snapshot | `V14-12L` | Verified | saturating monotonic-age helpers, maximum-value regression, full automated/build gate and Irok regression pass |
| `HJ-V14-P1-007` | P1 | An expired Addressed probe deadline can wrap to `DWORD_MAX` and turn a bounded HID wait into an effectively infinite wait | `V14-12L` | Verified | saturating remaining-time helper, expired/future/max-deadline regressions, full automated/build gate and Irok regression pass |
| `HJ-V14-P1-008` | P1 | A tester's MAD68 HE build can remain running while the private UAP/Soup path is shutting down | `V14-12M` | Partial | permanent child-unload and whole-process shutdown stalls are contained automatically; physical MAD68 HE retest of the new EXE and the old trace remain required |
| `HJ-V14-P2-001` | P2 | v1.3 self-contained runtime improvements can be lost during architectural migration | `V14-03` | Verified | checkpoint `b3fefce` compared; embedded self-contained behavior retained in isolated ABI1 architecture |
| `HJ-V14-P2-002` | P2 | Imported historical validation may be mistaken for validation of later v1.4 code | all | Open | per-package evidence enforcement |
| `HJ-V14-P2-003` | P2 | Simulation could be mistaken for real analog-protocol evidence or leak into production | `V14-02` | Verified | sources excluded from production compile; compile/runtime gates and evidence labels passed |
| `HJ-V14-P2-004` | P2 | Moving remote build inputs can silently change the private UAP or CI behavior | `V14-04` | Verified | independent clone passed locked fresh bootstrap, overlay verification and full build |
| `HJ-V14-P2-005` | P2 | Local build can omit portable tests or accept warnings rejected by CI | `V14-04` | Verified | independent clone ran required portable tests and enforced the production warning allowlist |
| `HJ-V14-P2-006` | P2 | Firmware-derived Aula support could be mistaken for physical hardware validation or could claim a sibling interface too broadly | `V14-12A` | Implemented | exact firmware/oracle reproduction, 17-read proof, exact-path ownership, ambiguity/session tests and Irok regression pass; physical Aula input/hotplug/multi-device gate remains open |
| `HJ-V14-P2-007` | P2 | A contained overlay or ViGEm output worker exception permanently disables that service until HallJoy is restarted | `V14-12L` | Verified | owner-side reap/restart supervision and one-shot C++ fault injections pass without overlapping worker generations |
| `HJ-V14-P2-008` | P2 | The browser overlay can consume excessive CPU by redrawing an unchanged canvas, retaining oversized bitmap caches and polling at a fresh-install 1 ms default | `V14-12O` | Verified | retained dirty-frame rendering, exact smoothing convergence, bounded 512/256 constant-time LRU caches, 8 ms fresh default and production Irok/Chrome before-after profiling |
| `HJ-V14-P2-009` | P2 | A factory-reset action can be inaccessible or visually inconsistent, partially delete state, re-import legacy files, or relaunch before the old process finishes saving | `V14-12P/Q` | Open | transaction gates pass, but manual UI review rejected the accent/focus states; visual acceptance plus the existing rollback/relaunch gates are required |
| `HJ-V14-P2-010` | P2 | Snappy/LKP edge, direction and valley state is not reset when profiles, axis bindings, modes or compacted pad identities change | Common analog pipeline follow-up | Open, static proof | generation-tagged/reset SOCD state and held-both profile/rebind/toggle/pad-compaction regressions |
| `HJ-V14-P2-011` | P2 | Layout presets may load partially and contain duplicate or extended HIDs that desktop tracking cannot select coherently and overlay reports as zero; UI raw/output telemetry also has no shared generation | Common UI/overlay pipeline follow-up | Open, static proof | whole-preset validation, explicit duplicate policy, unified key domain and generation-coherent raw/output/source snapshots |
| `HJ-V14-P2-012` | P2 | Curve/deadzone settings and profiles round to 0.001, enforce a 1% low/high and anti-deadzone/cap gap, and restrict rational weights without an output-format requirement | Precision/performance follow-up | Open, static proof | precise versioned storage with legacy milli migration, mathematical validity policy separated from UI step size and exact save/load/curve tests |
| `HJ-V14-P2-013` | P2 | Axes and triggers accept only one HID per direction/action while buttons accept many; gamepad-button and bind-capture thresholds are fixed hidden responsiveness policy | Precision/performance follow-up | Open, static proof | multi-source aggregation policy, configurable/explicit thresholds, serialization/UI migration and simultaneous-source tests |
| `HJ-V14-P2-014` | P2 | Duplicate tracked HIDs, UI timers that ignore the requested slow interval, and full unchanged overlay JSON serialization create avoidable desktop/server work | Precision/performance follow-up | Open, static proof | deduplicated generation snapshots, consistent timer policy, split static/dynamic overlay state and idle/change load profiling |
| `HJ-V14-P2-015` | P2 | Aula MAX always adds a 1 ms success pause and Hex80 fixes travel chunks at four entries without a locally proven device limit, potentially reducing matrix throughput | Precision/performance follow-up | Open, static proof | protocol/hardware characterization before any change, adaptive pacing/chunk negotiation where proven and USB error/rate qualification |
| `HJ-V14-P2-016` | P2 | The append-only validation matrix mixes current, historical and superseded PASS results without a machine-readable risk/source/artifact relationship, so stale evidence can be mistaken for current qualification | Test/evidence trust follow-up | Open, static proof | generated current evidence index with one status per risk, source-manifest and artifact hashes, explicit missing/hardware-pending states and links to immutable historical evidence |
| `HJ-V14-P2-017` | P2 | Required checks are split across glob discovery, hard-coded arrays, required-file lists and duplicated build preflights; the UI static audit runs twice while no single execution plan proves which result is authoritative | Test/evidence trust follow-up | Open, executable proof | one machine-readable execution manifest, unique test IDs, once-per-stage execution and safe result reuse only for identical source/toolchain/environment manifests |
| `HJ-V14-P2-018` | P2 | Native values/ownership/connection, health telemetry and some reader-group/config/UI state are assembled from independent atomics rather than one generation; simultaneous Sayo completions can even overwrite a correct zero reader count with stale nonzero state | Concurrency/lifecycle/ownership follow-up | Open, static proof | provider/config/health generation snapshots, simultaneous-reader completion barrier, disconnect/reconnect TOCTOU tests, telemetry invariants and explicit approximate-counter labeling |
| `HJ-V14-P2-019` | P2 | Current HallJoy PE has ASLR/high-entropy/NX/cookie/CFG instrumentation, but UAP has no CFG guard flags and no machine-readable exact-artifact mitigation policy prevents a future silent downgrade | Trust/security boundary follow-up | Open, executable proof | justified EXE/UAP/provider mitigation profiles, post-link/post-sign PE load-config inspection, downgrade negative fixtures and documented performance/compatibility exceptions |
| `HJ-V14-P2-020` | P2 | Stable same-user endpoints have implicit ownership: HallJoy accepts a pre-existing schema-valid mouse mapping, DrunkDeer uses a global unversioned mutex name, and overlay session issuance protects browser origins but does not authenticate arbitrary local processes | Trust/security boundary follow-up | Open, static proof | explicit creator/PID/generation/DACL contract, stale/pre-created/wrong-owner tests, scoped bounded mutex naming and honest local HTTP threat model with Host/capability hardening evaluation |
| `HJ-V14-P2-021` | P2 | Provider command mutability and dependency security state exist only in scattered code/docs: there is no machine-readable read/volatile/persistent/firmware allowlist, SBOM or advisory-review status tied to the shipped artifact | Trust/security boundary follow-up | Open, static proof | compiled command-set manifest with consent policy, ordinary-provider persistent/firmware prohibition, SPDX/CycloneDX SBOM, pinned advisory review and characterized dependency upgrade gates |
| `HJ-V14-P3-001` | P3 | Raw Mouse accumulation can overflow signed `int`, and uncommon clipboard/UAP failure exits can leak process or SetupAPI resources | `V14-12L` | Verified | widened saturating addition, clipboard ownership transfer checks, HID-list cleanup, locked fresh-plugin build and resource audit pass |

### Evidence boundary for HJ-V14-P2-006

V14-12A accepts only the exact Aula `1CA2:1902`, `FFA0:0001`, 65-byte HID
envelope and firmware `App V1.1.6 / Feb 4 2026`. The supplied firmware verifier
passed 57 checks; independently fetched fixed npm packages matched all ten
oracle sources and reproduced the archived oracle JSON hash. Production tests
exercise exact framing, response correlation, double-generation Fn0 mapping,
two-half travel, session poisoning, reconnect identity and ambiguous devices.

The risk remains `Implemented`, not `Verified`, because no physical Aula device
was available. Real analogue input, held-key unplug/reconnect, multiple Aula
interfaces and alternate firmware coexistence remain V14-12 hardware gates.

V14-12N adds the previously missing permanent Aula worker-stop process test.
The worker exceeds its three-second join deadline, retains its generation
resources, poisons native shutdown, exits with the expected code and leaves no
process. This verifies shutdown containment in production code under simulation;
it does not change the physical Aula compatibility status.

### Evidence for HJ-V14-P1-004

The 2026-07-31 Irok MG75 Max hardware traces proved native SparkLink discovery,
polling and analog row input on `VID 1CA6`, `PID 0529`, usage page `FFB0`. The
later production run recorded 515 changed rows and 516 input notifications.

The original shutdown trace showed the first worker exit followed by a hotplug
reconnect and an unmatched second generation. `V14-06D.1` closes an outer
service gate before stopping the active poller. Its production Irok trace has
three worker starts and three matching exits, two successful reconnects, analog
input before and after reconnect, clean process exit 0, and no reconnect, device
open or connection after `service.stop.begin`. The user confirmed correct
held-key neutralization/recovery with no stuck input. The risk is Verified.

### Evidence for HJ-V14-P1-005

The user runtime `overlay_perf.log` recorded isolated fetch averages of
5,001,400-5,002,000 microseconds, exactly matching the server's receive timeout.
The periodic `/client_perf` fetch could leave the single HTTP worker waiting on
an idle keep-alive connection and block the live `/state` stream.

V14-06C.1 closes one-shot telemetry and error responses immediately while
preserving keep-alive for high-rate state polling. The deterministic socket gate
measured the next `/state` at 0.3 ms in simulation and 0.4 ms in the production
`HallJoy.exe`; the normal and forced-timeout overlay lifecycle gates also pass.
The user subsequently confirmed that the browser input overlay no longer
exhibits the observed five-second freezes.

### Evidence boundary for HJ-V14-P1-008

The reported device is MAD68 HE, handled by the modified private UAP with Soup
inside it. It is not MAD68 Pro R, whose native A0 backend has a separate
protocol and lifecycle. The old tester EXE and exact failure phase have not
been reproduced locally, so V14-12M does not claim a confirmed historical root
cause. The old `HallJoyStabilityTrace.log` is still requested.

Code-level containment is deterministic. A child deliberately blocked forever
before plugin unload produced `child.stop_timeout` 2,641 ms after shutdown
began, was terminated without restart, joined both parent workers and exited
the main process normally. A separate permanent owner-thread stall was ended by
the 12-second watchdog with exit code 4 and zero survivor. Raw traces are under
`build/evidence/V14-12M-uap-shutdown-20260801`.

The exact production `HallJoy.exe` then passed 5/5 ordinary Irok close cycles in
122-226 ms with exit zero, no trace error, no survivor and no change to 11 user
state files. Evidence is
`build/evidence/release-qualification/20260801-235837`. The risk remains
`Partial` and release-blocking until the MAD68 HE tester closes this exact
artifact successfully; simulator evidence is not hardware validation.

The V14-12N matrix also reruns the shared UAP/Soup permanent child-unload stall
beside all six native routes and the global watchdog. Its 9/9 result with zero
survivors strengthens the code-level containment claim but cannot replace the
reported device's physical retest.

### Evidence for HJ-V14-P2-008

The pre-change production profile at
`build/evidence/input-pipeline-profile/20260802-105954` found that HallJoy used
0.835% of the 12-logical-processor machine while the real headless Chrome tree
used 14.407% and drew about 174.7 frames per second at the existing 1 ms poll
setting. The hot spot was therefore the browser renderer, not HID/realtime.

The final artifact retains the last canvas frame and redraws only after visual
invalidation. Smoothing snaps exactly to its target below the visual epsilon;
layout and every visual-style field still invalidate correctly. Bitmap caches
are bounded and their eviction no longer scans the entire Map. Two repeated
final 1 ms runs record HallJoy at 0.809-0.929% and Chrome at 5.451-6.120%.
The exact 8 ms fresh-default comparison records 0.721% and 3.747%, one settled
draw in ten seconds, 217,628/217,628 Spark routes, unchanged state and zero
survivors. Evidence is under
`build/evidence/input-pipeline-profile/20260802-113109` and
`build/evidence/input-pipeline-profile/20260802-113432`.

Headless Chrome is deliberately run without background throttling, so these
are repeatable upper-pressure comparisons, not an exact OBS CPU prediction.
System-busy CPU is recorded only as workstation context. The physical device is
Irok MG75 Max; other protocols inherit only the measured shared downstream path
and retain their own hardware qualification requirements.

### Evidence for HJ-V14-P2-009

The reset request uses the existing atomic INI transaction and is parsed back
before commit. No state is moved while the live UI is running. The old process
performs its normal settings save and complete bounded shutdown first; only
after GDI+, diagnostic logging, stability trace and watchdog teardown does it
start a clean process.

That process applies the request before `SettingsIni_Load`. Only five exact,
non-reparse targets are moved to a unique backup. Completed migration markers
remain outside the transaction, preventing legacy settings beside the EXE from
being imported again. A simulator-only failure after the third move proved
byte-exact reverse rollback and retained the request for retry; the next pass
committed all five backup hashes, fresh defaults, unchanged unrelated files and
zero surviving processes. Evidence is
`build/evidence/factory-reset/20260802-121843-951/summary.json`.

The official artifact then passed a physical Irok production cycle with
6,542/6,542 successful SparkLink queries, 164 ms shutdown, unchanged 12-file
user state and no survivor. This verifies the reset transaction and shared
application lifecycle; it does not change the pending MAD68 HE or Aula hardware
release gates.

## Imported audit statuses

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
| `HJ-AUD-P1-013` | P1 | built-in dependency installer had TOCTOU and supply-chain gaps | `S18` | Verified | runtime download remains removed; D-049 embeds exact ViGEmBus 1.22.0, validates size/SHA/signature on a read-only locked random path and re-hashes before explicit elevation |
| `HJ-AUD-P1-014` | P1 | dependency installation could freeze the UI indefinitely | `S18` | Verified | D-049 uses a 20-minute message-pumped bound, truthful timeout/restart results and no force-kill; linked self-test/build regression passed |
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


The current release retains these imported IDs while the sections below track
v1.4-specific UI, hardware, documentation, and layout risks.

## Imported stabilization evidence

The detailed S01-S12 narratives are maintained in `docs/stability/`, where the
initial audit, stage results, decisions, validation matrix, and chronological
worklog have been fully edited in English. This release register retains their
accepted IDs and current statuses without duplicating those historical reports.

The evidence sequence established generation-aware lifecycle contracts, shared
allocation-free C++ exception barriers, truthful registry stop results,
cooperative worker shutdown, reader-owned overlapped I/O, bounded analogue-host
and UAP generations, durable startup wake publication, a dedicated ViGEm output
owner, transactional persistence, hardened inherited-handle IPC, framed and
origin-bound overlay HTTP, deadline-paced UAP polling, stable device identity,
exact HID-interface ownership, pinned dependencies, and x64/W4 build gates.

Status promotion remained conservative: a code correction with an outstanding
physical-device or final-soak requirement stayed Implemented or Partial rather
than being treated as Verified. Exact package evidence is indexed by
`docs/stability/VALIDATION_MATRIX.md` and `docs/stability/tests/README.md`.

## UI risks reopened after V14-12Q

| ID | Priority | Risk | Mitigation/evidence | Status |
|---|---|---|---|---|
| `HJ-UI-P1-001` | P1 | analogue input repaints tab/static pixels and visibly flickers | dirty-region commit, buffered tab row, visible Remap invalidation gate, compile-time paint trace | Implemented; visual pending |
| `HJ-UI-P1-002` | P1 | Configuration telemetry is stale or refreshes the whole page | changed-hash/visible-tab gate and `720x36` live rect; static guard and Irok log | Implemented; visual pending |
| `HJ-UI-P1-003` | P1 | Spark selectors violate combobox mouse/keyboard/focus semantics | real `PremiumCombo` children and keyboard contract | Implemented; interaction/visual pending |
| `HJ-UI-P2-001` | P2 | diagnostics diverge between two pages | one route-complete builder consumed by Gamepad Tester; Configuration is brief status only | Implemented; visual pending |
| `HJ-UI-P2-002` | P2 | reset danger/focus styling is visually inconsistent | one rounded renderer, no accent strip or `DrawFocusRect` | Implemented; visual pending |

No UI row is `Verified` until the owner accepts the final EXE visually. Physical
MAD68 HE/UAP and Aula risks are unaffected.

## UI refresh risks after V14-12S

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-UI-P1-004` | P1 | shared keyboard preview loses fast release outside Remap | removed active-tab gate after dirty-bit consume; regression guard and six-tab harness | Implemented; visual pending |
| `HJ-UI-P1-005` | P1 | Remap thumb drag relayouts every raw pointer event and loses icon frames | 16 ms latest-target coalescing, deferred no-redraw child batch, one no-erase repaint | Implemented; visual pending |
| `HJ-UI-P1-006` | P1 | Tester gamepad motion is capped near 10 FPS | gamepad report hash sampled every UI tick; 100 ms retained only for diagnostics | Implemented; visual pending |
| `HJ-UI-P1-007` | P1 | Global scroll flashes owner-draw buttons white | atomic frame layout; removed synchronous per-pointer all-child update | Implemented; visual pending |
| `HJ-UI-P2-003` | P2 | Configuration combos leave intermediate move/erase artifacts | all four PremiumCombos use deferred no-redraw batches; scroll is frame-coalesced | Implemented; visual pending |
| `HJ-UI-P3-001` | P3 | new Input Overlay profile starts with excessive smoothing | native/web missing-value default set to 15%; persisted value preserved | Implemented; visual pending |

No row is `Verified` until the owner accepts the final production EXE.

## Unified viewport risks after V14-12T

V14-12S.1 was rejected by the owner because higher FPS exposed disappearing
elements. Rows `HJ-UI-P1-005`, `HJ-UI-P1-007` and `HJ-UI-P2-003` remain open and
their earlier child-batch mitigation is superseded by D-040.

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-UI-P1-008` | P1 | per-page scroll implementations diverge and regress independently | one `CustomPageScrollController` required by static audit on all six pages | Implemented; visual pending |
| `HJ-UI-P1-009` | P1 | child HWND erase/move cycles make elements flash or disappear | retained content-space composition; no active Remap/Global/Configuration HWND movement | Implemented; visual pending |
| `HJ-UI-P2-004` | P2 | compatibility combo/icon HWNDs escape back into visual layout | native HWNDs limited to popup/keyboard/action ownership; active layout guards | Implemented; removal debt tracked |
| `HJ-UI-P2-005` | P2 | retained page caches leak GDI/USER resources under repeated scroll | warm-cache 6-page stress: GDI 211/211/207 and USER 227/227/227 start/max/end | Implemented; visual pending |

The final production stress and lifecycle gates pass, but none of these rows is
`Verified` before owner testing of the exact SHA-256 artifact.

## Retained-control regression risks after V14-12T.1

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-UI-P1-010` | P1 | retained combo face differs from popup controller and jumps on open | canonical `PaintRetainedFace`; duplicate focus outline removed; three popup lifecycle cases PASS | Implemented; visual pending |
| `HJ-UI-P1-011` | P1 | popup controller stays visible and re-enters scroll composition after close | explicit `MsgDropStateChanged`; runtime verifies one visible while open and zero after close | Implemented; visual pending |
| `HJ-UI-P2-006` | P2 | encoding-sensitive text glyphs render as mojibake | vector Remap power/save icons; static guard forbids the text substitutes | Implemented; visual pending |

The unified scroll implementation remains active and its 6/6 stress gate still
passes. These rows are not `Verified` until owner review of the exact artifact.

## Retained interaction risks after V14-12T.2

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-UI-P1-012` | P1 | Remap power click is consumed as icon drag | bounded icon-domain classifier; static guard | Implemented; owner pending |
| `HJ-UI-P1-013` | P1 | binding edits do not expose global profile dirty/save state | shared save/dirty/notify transaction | Implemented; owner pending |
| `HJ-UI-P2-007` | P2 | Overlay choices behave as cycling buttons | three canonical PremiumCombos; 3/3 Overlay lifecycle PASS | Implemented; owner pending |
| `HJ-UI-P1-014` | P1 | wheel input over a top-level PremiumCombo popup is lost | popup routes `WM_MOUSEWHEEL` to controller state | Superseded by P1-015 |
| `HJ-UI-P1-015` | P1 | combo wheel navigates options or affects fitting lists | overflow-only `scrollTop`; `curSel`/`hotIndex` invariant; runtime state assertions | Implemented; owner pending |
| `HJ-UI-P1-016` | P1 | Configuration graph invalidates but replays a retained stale live marker until unrelated input dirties the page | split retained plot/curve from post-present marker/handle overlay; graph-only timer invalidation; static layer-order guards and 6/6 UI stress | Implemented; physical visual pending |

Automated tests do not replace the owner's interaction check.

## Aula physical diagnostic risks after V14-12U

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-AULA-P1-001` | P1 | broad Spark discovery opens and probes Aula `1CA2:1902 / FFA0` | dedicated-family rejection before Spark `CreateFileW`; static audits and repeated physical skip events | Closed/PASS |
| `HJ-AULA-P1-002` | P1 | production trace hides the exact first Aula proof failure | isolated raw/decoded aggressive trace identified the 54/60-byte gate | Closed/PASS |
| `HJ-AULA-P1-003` | P1 | relaxed diagnostics accidentally publish incompatible input | mismatch-mask zero required for claim/publication; runtime semantic mismatch also blocks publication | Guarded/PASS |

The earlier `HallJoyStabilityTrace (1).log` was not a clean lifecycle run and
proved only the Spark/Aula collision. The later single-EXE logs supersede it for
transport and parser diagnosis, but still do not verify strict Aula support.

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-AULA-P1-004` | P1 | inferred 54-byte sync gate rejects valid physical 60-byte response before later proof | diagnostic-only dual envelope exposed all later stages | Superseded by P1-006/P1-007 |
| `HJ-AULA-P1-005` | P1 | transient sharing violation motivates unsafe shared HID fallback | second trace proves 62/62 exclusive opens; no shared fallback added | Closed by evidence |

That diagnostic-only assessment was superseded by V14-12U.2 after three
complete physical proofs authenticated the immutable descriptor structure.

## Aula physical production-contract risks after V14-12U.2

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-AULA-P1-006` | P1 | binary build descriptor is misread as `Feb  4 6320`, causing a false firmware mismatch | exact 16-byte physical descriptors replace inferred date parsing | Implemented/PASS |
| `HJ-AULA-P1-007` | P1 | legacy inferred 54-byte sync remains an unproven compatibility path | production and diagnostic parsers now require physical 60-byte sync | Implemented/PASS |
| `HJ-AULA-P1-008` | P1 | physical travel channel exists but no non-zero runtime input has been observed | two strict claim-capable runs sustained polling for about 58 and 17.5 minutes; tester reports analogue travel visible in HallJoy; startup-only raw frames remain zero because of the 256-report trace cap | Closed/PASS within owner-observed input boundary |
| `HJ-AULA-P1-009` | P1 | device disconnect clears input but return-after-disconnect may fail to reclaim the same Aula | log 8 proves three successful reconnects with retained identity and strict proof; the first recovered session delivered 1,008 matrices at 341.463 Hz with 329 non-zero frames across 12 HIDs | Closed/PASS |
| `HJ-AULA-P1-010` | P1 | diagnostic raw cap proves startup but hides runtime Hz, 10-key behavior and later analogue values | log 7: 21,027/21,027 matrices, 343.973 Hz, all transactions <=4 ms, max 22 active, 2,654 frames at 10+, 8 releases to zero, 40 HID coverage | Closed/PASS |
| `HJ-AULA-P2-001` | P2 | repeated Spark dedicated-interface skips drown useful evidence | per-scan count plus cumulative summary, rate-limited to one event per 60 seconds | Closed/PASS |
| `HJ-AULA-P2-002` | P2 | shutdown `CancelIoEx` is misreported as a runtime transport failure | stop-aware `protocol.cancelled`/`poll.cancelled`, INFO severity, failed counter unchanged | Closed/PASS |

## Aula family-admission risks after V14-12V

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-AULA-P1-011` | P1 | flexible support probes unrelated HID devices or claims a false positive | SetupAPI brand/VID prefilter before metadata open; exact transport shape; complete exclusive-session structural proof; exact-path claim only | Guarded/PASS automated |
| `HJ-AULA-P1-012` | P1 | a larger sibling map overruns fixed five-batch assumptions | unique dynamic map bounded to 126 positions; nine batches/generation and 25 total proof transactions; 84-position regression under ASan/UBSan | Guarded/PASS automated |
| `HJ-AULA-P2-003` | P2 | protocol-compatible sibling is presented as physically verified | exact/family classification is retained in capability proof; public matrix limits physical evidence to WIN 60 HE MAX | Guarded; sibling hardware untested |

## AULA W669 / Standard risks after V14-14

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-W669-P1-001` | P1 | declared length `05` is mistaken for live subtype `05`, so valid events are rejected or configuration frames are published | firmware queue layout and official driver independently prove subtype `01`; parser test covers length `03/05` and rejects subtype `05` | Closed/PASS automated |
| `HJ-W669-P1-002` | P1 | shared VID/PID causes WIN60 geometry to be assigned to WIN68/KP-TE153 or unrelated devices | read-only `0D` firmware product selects exact official SI2825/SI2828/SI2851 61/68/69-key profiles; unknown products remain explicit-only; PID, substring and key count never select a factory map | Guarded/PASS automated |
| `HJ-W669-P1-003` | P1 | a synchronous fallback competes with the live dispatcher and drops change/release events | physical log proves 532 live packets were consumed by the old snapshot collector; all in-session snapshot/fallback I/O removed, leaving one receive owner | Corrected/PASS automated; physical rerun pending |
| `HJ-W669-P1-004` | P1 | an exploratory fallback changes calibration or persistent keyboard configuration | streaming allow-list is now `18/80`, `21/02`, `21/03`, `21/04`, `21/0A`; `21/0E` removed from runtime and mutating subcommands remain absent | Guarded/PASS code review and protocol tests |
| `HJ-W669-P2-001` | P2 | diagnostic I/O hides actual event delivery or mixes sensor-domain values with live travel | `21/0E` publication removed; raw trace and live counters cover subtype-`01` only; firmware-default poll code `0` is reported as unspecified | Corrected/PASS automated; physical rerun pending |
| `HJ-W669-P2-002` | P2 | diagnostic package produces several competing `.log` files | single-log profile writes parent/W669 evidence to `HallJoy.log`; isolated UAP host file logging is disabled for this profile | Closed/PASS smoke |
| `HJ-W669-P1-005` | P1 | zero `18/80` records are mistaken for absent keys, so a stock WIN60 is rejected before live subscription | physical log proves zero means factory inheritance and `01 FA` at official Fn position 122; known SI2825 factory baseline is overlaid with explicit records and covered by a ten-fragment regression fixture | Corrected/PASS automated; physical live rerun pending |
| `HJ-W669-P1-006` | P1 | raw `21/0E` values around `0x0Axx` are clamped against processed maximum 340, creating false full presses | physical packet analysis separates sensor-domain snapshots from processed subtype-`01` travel; snapshot values can no longer reach `Publish` | Corrected/PASS code review; physical rerun pending |
| `HJ-W669-P2-003` | P2 | expected idle read cancellation is counted as a transport failure | timed I/O preserves timeout status; stop-time `ERROR_OPERATION_ABORTED`/`ERROR_INVALID_HANDLE` is classified as informational before failure accounting | Corrected after physical reproduction/PASS automated |
| `HJ-W669-P1-007` | P1 | an absent MAX-family backend repeatedly enumerates every HID interface and periodically stalls an active W669 stream | absent discovery is event-driven through `WM_DEVICECHANGE`; only a present/transient candidate receives timed retries; static gate and local timeline pass | Corrected/PASS automated; physical rerun pending |
| `HJ-W669-P1-008` | P1 | competing stability/debug `CREATE_ALWAYS` owners make the only support log blind to W669 raw/timing data | one mapped file owner plus asynchronous sanitized append API; build requires diagnostic/single-log flags and W669 markers | Corrected/PASS build and local smoke |
| `HJ-W669-P1-009` | P1 | synchronous mapped-log flush stalls after clean shutdown until the 12-second process watchdog kills HallJoy | clean shutdown uses unmap/truncate/close through the cache manager; no explicit blocking flush; repeated local smoke exits in 75 ms | Corrected/PASS local lifecycle |
| `HJ-W669-P2-004` | P2 | normal diagnostic runs create MAD and exit sidecars despite the one-log contract | MAD is inline; successful watchdog exit creates no report; crash-only sidecars remain | Corrected/PASS local package smoke |
| `HJ-W669-P1-010` | P1 | a future W669 sibling with a different key count is either unnecessarily rejected or receives a plausible but wrong HID layout | seven current official products are covered by exact firmware-product profiles; unknown products may prove an explicit map but all-zero factory inheritance fails closed because key count cannot identify HID usages | Guarded/PASS automated; future unknown factory layouts require evidence |

## Public documentation and built-in layout risks

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-DOC-P1-001` | P1 | Quick start dependency instructions disagree with production behavior | README and production now agree on D-049 one-click embedded ViGEmBus 1.22.0 installation, direct official-page fallback and automatic private UAP preparation | Closed/PASS code, linked-resource and documentation audits |
| `HJ-DOC-P2-002` | P2 | a technical README rewrite drops material user capabilities, storage guidance or licensing while retaining obsolete dependency advice | public/local semantic comparison; restored code-backed video/features/editor/storage/troubleshooting/license content; obsolete system SDK/UAP instructions explicitly excluded; relative-link and content checks pass | Closed/PASS code and documentation audit |
| `HJ-DOC-P2-003` | P2 | mixed-language, literal machine translation, `_RU` filenames, or stale links make the public project inconsistent and difficult to trust | every translated narrative document compared with its preserved source and manually rewritten; generated evidence retained verbatim; references, tables, headings, fences, Cyrillic, mojibake, filenames and build-document contracts audited | Closed/PASS editorial and static audit |
| `HJ-K4-P1-001` | P1 | K4 HE is advertised as gaming-ready on stock firmware even though an idle key can arrive roughly one second late | public matrix requires the physically validated custom `A9 31` full-report firmware and explicitly rejects stock `A9 30` for gaming | Closed/PASS physical and documentation evidence |
| `HJ-K4-P1-002` | P1 | a general firmware link is mistaken for a compatible K4 image and a user flashes another model's binary | README and hardware matrix state that AnalogSense currently lists no pre-built K4 image and require exact model/layout/MCU verification plus a stock rollback image | Guarded/PASS documentation review |
| `HJ-K4-P1-003` | P1 | a physical test opens a second UAP/HID owner beside a live HallJoy and Keychron's four unnumbered `A9 31` response packets are assembled across requests, producing a stable shifted matrix until reopen | D-076 makes official/leaf ABI, exact dual-capture and production-smoke gates fail closed when any HallJoy is active; no process is terminated and synthetic tests remain parallel-safe; future token/part framing or exclusive-open policy requires separate evidence | Guarded for current test paths; protocol hardening/multi-instance product policy remains open |
| `HJ-LAYOUT-P1-001` | P1 | a newly shipped built-in layout is invisible to every existing user with a non-empty `Layouts` directory | built-ins load first, user files override same-name entries, missing built-in files are then created; ordered static gate | Closed/PASS automated |
| `HJ-LAYOUT-P2-001` | P2 | adding Keychron K4 HE changes the fresh-install default or loses the user's edited Generic layout | K4 is third; preset zero remains active by default; file override order preserves existing Generic edits; exact 100-key K4 tuple comparison passes | Closed/PASS automated |
| `HJ-LAYOUT-P2-002` | P2 | historical or bad revision-1 88 px values make numpad `+`/`Enter` end one rendered pixel too low after Reset | both built-ins use visually validated 87 px; transactional geometry revision 2 converts only old 88 px HID 87/88 values in the two named presets and explicitly forbids 87-to-88 expansion; four exact source guards and full production gate pass | Closed/PASS automated; visual evidence supplied by user |

## Redragon K673RGB-M admission risks

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-K673-P1-001` | P1 | the desktop SDK's generic M484 hint routes K673 through the wrong `0416:7372` backend | live iLLumiPC identity and JavaScript independently prove exact `2E3C:C365`, `FF1B:0091`, report-ID-1 W669 `0D/18/21` transport | Corrected/PASS static evidence |
| `HJ-K673-P1-002` | P1 | shared PID or ambiguous `K673RGB-M` HID name assigns BR, UK or US keys to the wrong matrix positions | three exact opcode-`0D` products select separate hash-pinned 132-position maps; no K673 descriptor fallback | Guarded/PASS automated |
| `HJ-K673-P1-003` | P1 | a configuration/calibration response is mistaken for real analogue input | publication remains restricted to W669 subtype `21/01` after RAM-only `21/02` subscription; 1,491 physical live publications match the official format | Closed/PASS physical BR run |
| `HJ-K673-P1-004` | P1 | a single tester build fails to show early travel, releases, multi-key behavior or stream health | physical BR run proves raw 7/340 onset, 72 balanced press/release edges, 17 used keys, four simultaneous keys, 1,491 publications and all final values zero; numeric polling-rate and ten-key claims remain intentionally unmade | Closed for support/PASS physical BR run |
| `HJ-K673-P1-005` | P1 | an unrelated firmware recovered from the vendor package is flashed to K673 | package/update-channel analysis proves no exact image; implementation is read-only and requires no flashing | Guarded/PASS static review |

## IROK ND75 M484 experimental risks

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-ND75-P1-001` | P1 | ND75 is treated as W669 because both expose `FF1B:0091` and 64-byte reports | separate protocol/backend; exact `0416:7372` plus `M484/X86HERGB`; host `29` and device `21` asymmetry is fixture-tested | Guarded/PASS automated |
| `HJ-ND75-P1-002` | P1 | a plausible 75% layout publishes the wrong HID for a sensor position | exact official `KeyInfo_X86HERGB.config` 6x22 map, hash and subscription-mask tests; no descriptor/name fallback | Guarded/PASS automated; physical spot-check pending |
| `HJ-ND75-P1-003` | P1 | one-byte live depth has a different maximum or polarity than inferred from static artifacts | diagnostic records raw packets and `out_of_range`; production enablement blocked on slow press/release physical evidence | Open/PENDING OWNER |
| `HJ-ND75-P1-004` | P1 | capability probing changes persistent keyboard configuration | emitted-command allow-list is identity `0D`, read-only `29/18/04`, RAM subscription `29/18/02`, unsubscribe `29/18/03`; no calibration/remap/firmware code | Guarded/PASS static review |
| `HJ-ND75-P1-005` | P1 | pending proof/live HID read leaves HallJoy running after close/unplug, or a dead session prevents reconnect | every proof/live session registers its cancellable handle; proof loops observe stop; terminal transport errors end immediately, unknown errors end after three attempts, input is neutralized and reconnect resumes; 3-second join policy, portable policy test and no-device lifecycle smoke pass | Corrected/PASS automated and local; physical unplug pending |
| `HJ-ND75-P2-001` | P2 | an unverified backend is shipped as normal v1.4.1 support | catalog entry requires `HALLJOY_IROK_ND75_EXPERIMENTAL`; public matrix labels only the isolated test candidate | Guarded/PASS automated |
| `HJ-ND75-P2-002` | P2 | keyboard connected after startup is never claimed because initial routing found nothing | experimental worker remains alive, responds to device change and can run exact proof/claim on arrival | Implemented/PASS no-device smoke; physical hotplug pending |
| `HJ-ND75-P1-006` | P1 | a `V1.*` identity is claimed after the required asymmetric `21/04` capability response times out | firmware-family soft fallback removed; both admission and the reopened live session require a decoded `21/04` response before claim/publication | Corrected/PASS automated; physical capability response pending |

## GravaStar Mercury V75 family risks

| ID | Priority | Risk | Root correction/evidence | Status |
|---|---|---|---|---|
| `HJ-GV75-P1-001` | P1 | V75/Pro are discarded before the correct 6x21 proof because their VID is `1CA5`, or V75 Lite is opened by the legacy SparkLink reader | one exact USB/board registry is shared by 6x21 discovery and legacy pre-open exclusion; routing/static tests cover all three identities and nearby negatives | Corrected/PASS automated |
| `HJ-GV75-P1-002` | P1 | a matching VID/PID is treated as sufficient proof and an unrelated device is claimed, or an exact known board is rejected for not copying Aula-only model bytes | exact identity permits only read-only probing; the exact known-board profile requires its nonzero expected board ID plus structured descriptors and the unchanged `FFA0:0001`, 65-byte transport, framing, checksum, correlation, scale, map, two stable generations and travel proof; unknown-family probing retains the stricter Aula `C0/01/00` signature | Corrected/PASS automated and physical identity/proof evidence |
| `HJ-GV75-P1-003` | P1 | physical Fn `F001` is truncated, lost or reconstructed from a later digital event | 16-bit active map publishes `F001` as established semantic `0x409`; Menu stays USB `0065`; unknown vendor functions fail closed; the returned physical map contains `F001`, and end-to-end/diagnostics tests cover its publication without digital input | Corrected/PASS physical map plus automated publication; live Fn travel pending |
| `HJ-GV75-P1-004` | P1 | firmware evidence is mistaken for real Windows transport, stable polling or reconnect support | returned traces prove the exact Windows HID endpoint, board/App, map, scale and direct travel replies; corrected diagnostic records all 126 positions, rate, latency, releases, coverage and reconnect | Partial/PASS physical analogue stream and sustained polling; unplug/reconnect pending |
| `HJ-GV75-P1-005` | P1 | a user-facing diagnostic log exposes private filesystem/raw HID paths or unrelated hardware inventory | all diagnostic text is redacted at the final trace sink; the V75 single-log bridge accepts only its targeted backend lines; an isolated exact-artifact smoke rejects user/temp roots, raw `\\?\`/`\\.\` paths and unrelated Raw Input/Spark inventory | Corrected/PASS runtime smoke |
| `HJ-GV75-P1-006` | P1 | a physical run produces neither analogue data nor enough evidence to distinguish identity, enumeration, open, proof, map, travel, claim or lifecycle failure | diagnostic-only monotonic progress/first-failure state and one mandatory `diagnostic.verdict`; safe family-VID/GravaStar-brand observation catches changed identities without probing them; even the returned incomplete 99.9-239.7 s slice conclusively proved the analogue stream and independently isolated the ViGEm owner failure | Corrected/PASS diagnostic content; handled-close finalization remains required for a self-contained verdict |
| `HJ-GV75-P1-007` | P1 | a deterministic semantic mismatch repeats the entire proof on a timer, flooding HID and the log without any possible state change | semantic firmware/scale/map/travel rejection now clears stale notifications and sleeps until stop or a real device-change notification; transient transport/claim failures retain bounded retry; static guards and physical-packet end-to-end regression cover the split | Corrected/PASS automated; physical no-flood confirmation pending |
| `HJ-TEST-P2-012` | P2 | the 1,000-generation pre-signalled supervisor stress can terminate a correctly contained child before its entry marker is scheduled, making the official build gate nondeterministically fail | corrected oracle requires either the child's positive containment observation or a forced reap after the production launch path assigned the suspended child to its Job; five standalone 1,008-generation runs plus the official full run pass with no overlap/survivor | Corrected/PASS |
