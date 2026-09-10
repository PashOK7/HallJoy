# HallJoy current handoff — 2026-08-20

## 2026-09-06 — Unified layout completed

Current ordinary artifact: build/release/HallJoy.exe, SHA256
A4D8B0C52D826B7ED723819DD441A7DDD05501D84AEAF647541F0F6DDCEE6E37.
Official + Aula diagnostic clean builds exited 0; 80 static audits and 49 portable
C++ checks passed. No keyboard/gamepad runtime tests were run. 227 primary code
files and plugin main/linker inputs are unchanged. The old source x64 trees,
root EXE and build/output are preserved in .local/backups/legacy-builds;
old backups are consolidated under .local/backups/imported. 16 docs have moved
with redirects. Do not use older "current artifact" hashes below as this build.

Start with [PROJECT_LAYOUT](../current/PROJECT_LAYOUT.md) and
[STRUCTURE_MIGRATION](../validation/STRUCTURE_MIGRATION_2026-09-06.md).
They define exact paths, manifests, checked boundaries and rollback lookup.
Remaining roadmap/runtime findings are not closed by this filesystem change.


## 2026-09-06 — Structure review and previously missed Sayo design notes

See [documentation index](../README.md) and
[file-structure review / FS-00..05](../PROJECT_STRUCTURE_REVIEW_2026-09-06.md).
The tree had 4707 files / 1840.57 MiB at measurement; 927 generated intermediates
in three source-local x64 roots account for 1130.35 MiB, subject to rebuild/owner
checks before cleanup. No source/output files were deleted or moved.
Important RM-03 input: [SAYO_DEVICE_NOTES.md](../../src/HallJoyProject/SAYO_DEVICE_NOTES.md)
explicitly documented automatic user-letter matching on 2026-05-14. The earlier
audit missed this source-adjacent document. Read it before proposing Sayo changes;
its capture paths are placeholders, not newly verified raw evidence. FS tasks
feed RM-00/32/36; Discord RM-37 remains the final feature card in this Roadmap.


## 2026-09-06 — Disputed behavior: ask the owner; Discord feature planned

If intended behavior is disputed, unclear or contradicted by documents, ask the
owner what HallJoy should do in that concrete scenario before implementing the
contested change. Continue independent work; record the answer and do not re-ask
already resolved decisions. Routine implementation choices remain autonomous.
FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md v1.2 adds RM-37 at the very end:
unsupported-keyboard status with a Discord community invitation, plus a permanent
Discord link in the UI. Total: 37 audit cards + 1 feature card. Official community
invite must be obtained from the owner before shipping buttons; no invite was
found in the inspected support docs. This is documentation only, not implemented UI.


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


## 2026-09-06 — Architecture review (analysis only)

See [ARCHITECTURE_REVIEW_2026-09-06.md](ARCHITECTURE_REVIEW_2026-09-06.md).
Eight architectural work areas are grounded in current code: output freshness,
immutable configuration, extended-key cost, V2 migration completion, UI/disk and
supervision ownership, module boundaries, release catalog, and qualification
provenance. First proposed package: producer-progress lease and independent
neutralization, tested with fake transport. No production code, artifacts or
settings changed; no input/controller tests ran. These are proposals and source
review risks, not new runtime PASS claims. The 2026-09-05 EXE is unchanged.


## 2026-09-05 — Profile audit remediation (D-079)

Final rebuilt EXE: `build/release/HallJoy.exe` (same bytes in `build/output`).
SHA-256: `44CFC0793A9CFD2A74B2B677AD8704708B1E16C6CD764EA8852C14962ECBE012`.
Evidence and runner-status caveat: `.analysis/profile_audit_fixes_20260905_evidence/RESULT.md`.
The final source suite passed 79 static audits and 49 C++ tests.

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

This file is the short entry point when development continues in another chat.
The linked documents, not chat history, are the source of truth.

## Working rules

- Read and follow `ENGINEERING_WORKING_METHOD.md` before every implementation
  package. Compare a local root fix, staged migration and clean redesign/rewrite
  before changing production architecture; choose by correctness, latency,
  liveness, ownership and evidence, not by smallest diff or speed.
- Do not use git for this workspace.
- Preserve unrelated local files and changes.
- Do not infer analog key identity from binary keyboard events.
- Do not add a VID/PID to an existing family without a wire-protocol proof.

## Active state

No next stable release is allowed while the mandatory protocol correctness
roadmap is open. `CORRECTNESS_RELEASE_BLOCKERS.md` is authoritative; the old
v1.4.1 qualification cannot be reused to bypass later audit findings.

Dependency installation state changed on 2026-08-21. V14-12F remains valid as
historical evidence that the old mutable `latest` downloader, predictable temp
path and infinite privileged wait were unsafe, but its manual-only UI is
superseded by D-049. Production now embeds the exact official ViGEmBus 1.22.0
installer, performs no runtime download, validates resource size/SHA-256 and
Authenticode on a read-only locked file, re-hashes immediately before explicit
user-approved elevation, and waits for at most 20 minutes while pumping UI
messages. The missing-driver dialog has explicit Install / official-page /
continue-without-output actions and retries the backend after a successful
install. Exact current artifact: `build/release/HallJoy.exe`, 8,485,376 bytes,
SHA-256 `7EB42CF1687D0DB1FF16721875C5F36B502FD96D1EB1DEC686C3AC4EEA85AB6B`.
The linked-resource extraction/hash/signature/read-execute self-test passes;
the physical missing-driver/UAC install was not run because this workstation
already has ViGEmBus.

The newest IROK evidence is now the highest-priority runtime blocker. In
`HallJoyStabilityTrace.previous (1).log` the SparkLink route remained perfect
(`279189/279189`, zero failures), but the ViGEm output thread handle produced
`WAIT_FAILED/ERROR_INVALID_HANDLE`; lifecycle became poisoned and all 161
watchdog recovery attempts were blocked. In the following 160-second run
SparkLink again remained perfect (`421858/421858`), while shutdown was poisoned
by an independent `sayo stop.lock_timeout`. The crash sidecar was synthetic,
not an unhandled exception. These facts are preserved without raw private logs
in `V14_RELEASE_READINESS_IROK_FREEZE_FORENSICS_2026-08-21.txt` and tracked as
`HJ-V14-P0-006` / `HJ-V14-P1-038`. The complete mandatory order is now
`RELEASE_READINESS_MEGA_AUDIT_2026-08-21.md`.

The 2026-08-22 corrected GravaStar V75 run then reopened P0-006 in the new F3
process route: `HallJoy (10).log` physically proves 76,773 error-free analogue
matrices at 308-342 Hz, smooth/released/two-key input, but the parent-owned
ViGEm child-process handle became invalid after 135.6 seconds and the old
watchdog made 218 blocked attempts. D-064 now kernel-protects the sole process
and job handle slots from foreign close, verifies PID identity and makes unsafe
recovery one-shot fail-closed. All local owner/self-host/real-child/stress/build
gates pass. Exact diagnostic `2C5A1E05...098B` still needs a long physical
output-continuity plus unplug/reconnect run; superseded `E71EDB6...C3D5` is DO
NOT DISTRIBUTE. See
`V14_23_VIGEM_PROTECTED_HANDLE_HARDENING_2026-08-22.txt`.

1. IROK ND75 work is paused. Do not resume it unless the owner explicitly
   changes priorities.
2. The first DrunkDeer G65 diagnostic and its user log are rejected because
   diagnostic schema 1 dropped 40-byte x64 RAWKEYBOARD packets and could leave
   a preallocated NUL tail without shutdown evidence. Turbo mode also disabled
   the keyboard's analog output. Physical schema-2 log SHA-256 `5F62728C...FFD2`
   then proved two independent facts: firmware 0012 returns one matrix frame
   per `B6 03 01`, and the common UAP map is wrong for at least Esc `(1,0)`,
   RightShift `(4,12)`, RightAlt `(5,9)`, Down `(5,13)`, Right
   `(5,14)->Left`, and Up `(4,13)->RightShift`; Left and Fn were missed by the
   one-frame-per-second v2 sampling. Therefore v2 EXE `D8026D...A4DC` is also
   rejected. The corrected diagnostic is ready at
   `build/HallJoy-DrunkDeer-Diagnostic.exe`: 2,361,856 bytes, SHA-256
   `AF602E45CFA14AB9905CBA76223D2F8D09C355DF56707E7A38C4D7DC1F8462D8`.
   It keeps one HID open and performs the proven UAP request+three-chunk
   transaction for every frame. Digital events never drive, select or learn
   analogue output at runtime; mapping analysis is a separate post-exit tool.
   PID 2382 now selects `g65_full_static_v2`: 45 positions are directly fixed by
   the physical log and the complete 68-position map follows the official ANSI
   G65 geometry. It contains 66 standard HID keys plus independent Soup/UAP
   extended codes Fn `0x409` and Menu/Fn2 `0x403`; the common UI, curve, binding,
   profile and overlay path supports the complete 16-bit domain. No digital event
   participates in analog identity or timing. The persistent transport retries a
   transient frame on the same handle, reopens only on definitive disconnect or
   after 12 consecutive transient failures, neutralizes state older than 350 ms,
   and logs retry/recovery/stale/reopen causes separately. Turbo must be off.
   Physical log `(4)`, SHA-256 `9BA60C...C21A`, completes that gate for the
   tested path: the user reported correct behavior; Fn `0x409`, Menu `0x403`
   and all four arrows independently reached raw 40; 19,051 frames completed
   with zero DrunkDeer failures/retries/reopens and a stable 249.4 Hz mean.
   Forty-seven of 68 mapped positions changed; Delete/End/PgUp/PgDn were not
   pressed in that session. No further matrix collection is required. The same
   log exposed an independent ViGEm output-worker join timeout during close:
   DrunkDeer stopped cleanly, but global shutdown was correctly marked poisoned
   and exited with code 2 after the three-second bound. This is not user error
   and remains a common output-lifecycle issue. See
   `DRUNKDEER_DIAGNOSTIC.md` and `DRUNKDEER_UAP_PROTOCOL_AUDIT.md`.
   Built-in keyboard-layout preset zero remains the historical `DrunkDeer A75
   Pro`. `DrunkDeer G65 ANSI` and the compact `WASD Only` layout are selectable
   alternatives. `WASD Only` contains exactly W/A/S/D and changes presentation
   only; it never limits the complete native analogue snapshot.
3. The complete cross-family UAP audit is open and must be revisited. It is not
   historical and is not closed by the previous release qualification. See
   `UAP_ALL_KEYBOARDS_AUDIT.md` and open risks `HJ-V14-P0-001` /
   `HJ-V14-P0-002` in `RISK_REGISTER.md`.
4. The complete production-native audit is also open. It covers all seven
   catalog routes and records independent critical defects: Sayo derives
   analog identity from later binary keyboard events and can fabricate depth;
   W669 can retain stale connected/nonzero state indefinitely; Hex80 can retain
   a permanently failed chunk; Addressed/SparkLink/fixed sibling layouts and
   the shared 8-bit key domain have additional P1 gaps. A later code pass also
   proved that SparkLink global success/freshness can hide one permanently
   failing row and its stale values (`HJ-V14-P1-039`). No implementation fix was
   made because representative family hardware is not currently available.
   See `NATIVE_ALL_KEYBOARDS_AUDIT.md` and risks `HJ-V14-P0-003` through
   `HJ-V14-P1-013`.
5. The complete common analog pipeline audit is open. It found independent P1
   defects after protocol parsing: digital fallback uses a global manual
   backend list instead of per-source/per-HID ownership; profile loads are
   unvalidated, piecemeal and unchecked by the global switch; axes/triggers,
   buttons, INI, UI, capture and overlay disagree on the key-identity domain.
   SOCD state and layout/UI snapshot issues are recorded as P2. See
   `COMMON_ANALOG_PIPELINE_AUDIT.md`, P1 risks `HJ-V14-P1-014` through `016`
   and P2 risks `HJ-V14-P2-010` / `011`.
6. The artificial-limits and performance audit is open. Native backends
   irreversibly quantize to 1001 levels before curves; generic UAP performs
   repeated 256-value copies/scans per key and device and propagates unchanged
   poll generations into realtime; independent UAP/XUSB 1 kHz ceilings are
   magic constants rather than capability-derived policy; the private UAP
   snapshot silently truncates at eight devices while the parent tracks 16.
   No source fix or build was made. See
   `ARTIFICIAL_LIMITS_PERFORMANCE_AUDIT.md`, P1 risks `HJ-V14-P1-017` through
   `020`, and P2 risks `HJ-V14-P2-012` through `015`.
7. The input-provider architecture audit is open. Shared memory itself remains
   the correct high-rate transport, but HallJoy currently consumes an already
   complete UAP snapshot through a serialized per-key Wooting compatibility
   facade; one fixed all-access object mixes data, control, telemetry and
   diagnostics; native protocol I/O lacks UAP's process containment. The
   required direction is a common `AnalogProviderV2`, one immutable snapshot
   acquisition per generation/tick, separated IPC planes, explicit capacity,
   common key/source/freshness semantics, then staged native isolation. Do not
   perform a big-bang parser rewrite. See
   `INPUT_PROVIDER_ARCHITECTURE_AUDIT.md`, P1 risks `HJ-V14-P1-021` through
   `023`, and roadmap package `V14-21`.
8. The test/evidence trust audit is open. The current baseline passes 59
   source-text audits and 34 portable C++ executables, but mandatory gates also
   require known-wrong disabled-hotplug, milli, 256-key and fixed-1-kHz
   contracts. Official build/CI does not execute sanitizer, production smoke,
   fault, soak or exact-artifact release qualification; merely naming a runner
   is sometimes counted as coverage. Before any P0/P1 fix, add an old-bug
   negative oracle that fails on the current defect. See
   `TEST_EVIDENCE_TRUST_AUDIT.md`, P1 risks `HJ-V14-P1-024` through `027`, P2
   risks `HJ-V14-P2-016` through `017`, and roadmap package `V14-22`.
9. The concurrency/lifecycle/ownership audit is open. It found a P0 close/use
   race on the ViGEm wake handle during watchdog recovery and a P1 data race in
   tracked-HID publication. Native registry lifecycle can remain `Running`
   after its worker exits; SparkLink/Sayo discovery, proof, Stop and join execute
   inside realtime; cancelled OVERLAPPED HID I/O and watchdog-allocation failure
   have no hard process liveness bound. Compound provider/config/telemetry state
   also lacks one generation. No implementation or test contract was changed.
   See `CONCURRENCY_LIFECYCLE_OWNERSHIP_AUDIT.md`, P0 risk
   `HJ-V14-P0-005`, P1 risks `HJ-V14-P1-028` through `032`, P2 risk
   `HJ-V14-P2-018`, and roadmap package `V14-23`.
   The 2026-08-21 physical IROK logs extend this audit with P0 invalid ViGEm
   thread-handle/recovery poison and P1 Sayo lifecycle-lock shutdown poison.
   The stable 1.4 EXE is an A/B reference, not a source fix: the available
   ViGEm output-worker section is line-for-line identical.
10. Pwnage Zenblade 65 V2 has been statically researched but is not supported.
   PID `3662:1002` uses `FF60:0061`, report ID 0 and a direct 64-byte
   VIA/QMK-like protocol with Pwnage Hall extensions. The older WASM `00 71`
   protocol is not the PID `1002` protocol. An official-representative-linked
   V0022 SOCD firmware was recovered for the old Zenblade only: its manifest
   proves runtime `3662:1001` and bootloader `3662:1002`. Never run or offer that
   updater to V2, because V2 uses `1002` as its normal runtime PID. Disassembly
   proves old wire command `71` is a persistent trigger/configuration getter,
   not live travel, and no old dispatcher handler exports the dynamic Hall
   arrays. No V2 image, current-travel command or passive depth parser is known.
   Do not build a passive listener: the next useful hardware step remains an
   active read-only sweep of undocumented parameter selectors inside the
   proven V2 Hall getter `20`, as specified in
   `PWNAGE_ZENBLADE_65_V2_PROTOCOL_RECON.md`.

## Code-change boundary

The UAP all-family review, native all-family review, common-pipeline review,
precision/performance review, input-provider architecture review,
test/evidence trust review, concurrency/lifecycle/ownership review and Pwnage
reconnaissance changed project documentation only. The evidence review ran
existing portable and sanitizer tests but did not change their contracts. The
concurrency review did not build an EXE or change production/test/build code. A
temporary official Pwnage Hub demo copy was inspected outside the project and
is not part of HallJoy. No UAP/native parser, IPC/provider architecture,
performance or concurrency fix, Pwnage PID admission, guessed top-level command
or production compatibility claim has been added. The release-readiness mega
audit and IROK freeze forensics also changed documentation only; no runtime fix
or new EXE was produced. The implementation after those audits remains
compile-time scoped to the DrunkDeer diagnostic build. It
now includes schema-2 protocol framing and durable logging, the PID-specific
complete G65 static map, an extended key-code domain for Fn/Menu across the
common analog UI/binding/profile path, and bounded same-handle transport
recovery. It does not change generic production UAP routing or admit another
DrunkDeer PID. G65 mapping, Fn/Menu, arrows and 249.4-Hz transport passed on
physical log `(4)`; the remaining DrunkDeer rerun is only against the eventual
exact final EXE after the common ViGEm/lifecycle blockers are fixed.

## 2026-08-23 continuation checkpoint

- The mandatory but non-release-blocking virtual firmware/HID testbed is now
  permanently tracked as `LAB-01..LAB-06` in
  `FIRMWARE_VIRTUAL_HID_TESTBED_ROADMAP_2026-08-23.md`. It must not disappear
  from planning, but it does not displace current release blockers.
- R2-B2i-a is complete: checked variable Provider V2 data-plane layout,
  double-slot commit contract and malformed/stale/greater-than-eight portable
  oracles pass. Full native checks and official Release x64 pass; exact local
  artifact SHA-256 is
  `920CBFB75229F9820FFB20C30CC11A3ED6940EEEA16D66E23E926099F087F5FE`.
- This checkpoint intentionally does not alter the production route. The live
  monolithic `SharedState` is still fixed at eight V2 devices and mapped with
  old rights. No user test is required for B2i-a.
- R2-B2i-b is complete. Its deterministic Windows process gates pass:
  explicit handle inheritance, parent-read-only/child-writable rights, forged
  handle rejection, odd-commit crash rejection, reader/resize exclusion, six
  topology generations and zero surviving child/writer are proved. Full native
  checks and MSVC Release x64 pass. The isolated exact physical zero-capacity
  negotiation/restart capture passes on the final packaged image, SHA-256
  `1EAAFAD31C19AE9C3DC75D37E6081BD0120CB156DCBA9C5C95719D6A6F0F4EFF`;
  the sequential production smoke closes cleanly with no survivor or log.
- Do not run the official build, private ABI runtime gate, exact UAP dual-capture
  smoke or ordinary production smoke while any HallJoy is active. Keychron's
  custom full-matrix response has four same-command packets without a visible
  transaction/part index; a second HID consumer can contaminate another open
  handle's response queue. This is a strong code/protocol inference matching the
  observed stable matrix shift, not a captured on-wire proof. Do not add a
  runtime digital-key correlation or silently reset/relearn the matrix.
- Next package is B2i-c: migrate the live shadow and,
  only after its gates pass, makes V2 the normal local engineering route. The
  dense implementation remains compiled/tested behind an explicit immutable
  legacy build property for a separately named emergency/user artifact; no
  runtime fallback or UI switch is allowed. B2j later decides user-release
  promotion/removal rather than performing the first real V2 output test.

## Required reading before related work

- `RELEASE_READINESS_MEGA_AUDIT_2026-08-21.md` first for the single mandatory
  order, release scope, hardware matrix and definition of done;
- `ENGINEERING_WORKING_METHOD.md` before planning or implementing any package;
- `README.md` in this directory for the authoritative v1.4 document index;
- `CORRECTNESS_RELEASE_BLOCKERS.md` before any release/version decision;
- `UAP_ALL_KEYBOARDS_AUDIT.md` before any UAP family fix;
- `NATIVE_ALL_KEYBOARDS_AUDIT.md` before any native family fix;
- `COMMON_ANALOG_PIPELINE_AUDIT.md` before curves, bindings, profiles, SOCD,
  key-domain, UI/overlay or ViGEm pipeline changes;
- `ARTIFICIAL_LIMITS_PERFORMANCE_AUDIT.md` before changing normalized value
  precision, polling/output rate, snapshots, device capacity, hot-path work,
  UI timers or overlay state publication;
- `INPUT_PROVIDER_ARCHITECTURE_AUDIT.md` before changing UAP/HallJoy IPC,
  Wooting compatibility usage, provider contracts, snapshot publication,
  native process placement or runtime configuration generations;
- `TEST_EVIDENCE_TRUST_AUDIT.md` before adding, changing or using a test/build/
  CI/release gate as evidence for any open P0/P1;
- `CONCURRENCY_LIFECYCLE_OWNERSHIP_AUDIT.md` before changing worker state,
  handles/events, hotplug/reconnect placement, HID cancellation, shutdown
  watchdogs or provider/config/telemetry publication;
- `DRUNKDEER_UAP_PROTOCOL_AUDIT.md` before DrunkDeer implementation;
- `PWNAGE_ZENBLADE_65_V2_PROTOCOL_RECON.md` before Pwnage diagnostics or code;
- `docs/development/ADDING_NATIVE_ANALOG_PROTOCOL.md` and
  `docs/development/PROTOCOL_REVIEW_CHECKLIST.md` before a new native backend.
