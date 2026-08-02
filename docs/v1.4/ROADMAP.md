# HallJoy v1.4 roadmap

## Release objective

v1.4 combines the mature user interface and self-contained dependency lessons
from v1.3 with the native protocol, isolation, scheduling, and stability work
from the imported advanced archive. The release must not require a system-wide
Wooting Analog SDK installation.

The word "final" is not used before every release gate in this roadmap passes.

## Package rules

- One package addresses one coherent risk area.
- Protocol bytes, mappings, timing, lifecycle, and UI changes are not mixed
  without characterization tests.
- Every package has a local rollback commit.
- Every package updates the authoritative v1.4 documentation.
- Static audits supplement behavioral tests; they do not replace them.
- No push, merge to `main`, tag, or GitHub Release occurs without explicit
  approval.

## Ordered packages

| Package | Scope | Status | Required completion gate |
|---|---|---|---|
| `V14-00` | Preserve v1.3 SDK work; import and verify the advanced archive | Verified | Provenance commits, clean tree, baseline build and test evidence |
| `V14-01` | Product identity, version resources, current README and documentation ownership | Verified | No active product surface reports 3.9.0; historical evidence remains intact |
| `V14-02` | Development-only deterministic analog simulator and scenario runner | Verified | Full common pipeline, ramps, SOCD, hotplug, disconnect and fault scenarios without production enablement |
| `V14-03` | Self-contained private UAP runtime and truthful dependency diagnostics | Verified | Works from writable and protected install locations; never recommends system SDK |
| `V14-04` | Reproducible dependencies, warning baseline, local/CI-equivalent build scripts | Verified | Pinned inputs, clean-room x64 Release, portable tests, documented warning policy |
| `V14-05` | Truthful lifecycle registry and generation-scoped stop contract | Verified | Failure-injected start/stop/restart tests; timeout/fault poisons the generation and blocks restart |
| `V14-06` | Cooperative lifecycle migration for realtime, logging and native protocol workers | Verified locally | Per-worker tests, no ordinary `TerminateThread`, plus unchanged protocol and mapping characterization |
| `V14-07` | Analog host and UAP ABI generation, exception, unload and restart safety | Verified | Partial-start, crash, hang, C ABI, null/state, unload and bounded restart gates passed |
| `V14-08` | Startup transaction, wake correctness and ViGEm output isolation | Verified | Reverse-order rollback, no lost wake, stalled-driver and report-equivalence tests |
| `V14-09` | Transactional persistence and writable state migration | Verified | Fault-injected atomic-save tests and safe `%LOCALAPPDATA%` migration |
| `V14-10` | IPC and overlay security/correctness | Verified | ACL, spoofing, framing, overflow, origin, concurrency and shutdown tests |
| `V14-11` | UAP pacing, identity, modularization and measured performance | Verified | D-022 production-code gates for pacing, identity, snapshot lifetime and exact interface ownership; no unsupported hardware claim |
| `V14-12` | Release qualification, Aula integration and hardware matrix | In progress | Clean package, 8-24h soak, reconnect cycles and required device-owner gates |

## Completed package: V14-00

Completed:

- v1.3 self-contained SDK work preserved on
  `checkpoint/v1.3-self-contained-sdk` at `b3fefce`.
- Advanced archive imported byte-for-byte on `v1.4-integration` at `f5e8c18`.
- Original ZIP SHA-256 recorded in the worklog.
- Imported x64 Release build reproduced successfully before integration.
- establish the authoritative v1.4 documentation set;
- x64 Release build reproduced from `v1.4-integration`;
- all supplied static audits passed;
- all portable C++20 tests passed with Clang 19.1.5;
- ViGEmBus 1.22.0 installed from the verified winget/GitHub release;
- UI launch, ViGEm initialization, main shutdown, and child shutdown passed.

The trace verdict is `WARN`, not `FAIL`, because this workstation has no
SparkLink device. It proves generic startup/shutdown and ViGEm lifecycle only.
GCC evidence is inherited from the byte-identical archive baseline and will be
rerun in CI before publication.

## Completed package: V14-01

- Central version macros define `1.4.0.0` and runtime build ID `HallJoy-v1.4`.
- The Windows resource exposes `FileVersion` and `ProductVersion` `1.4.0.0`.
- The About dialog and current README identify v1.4.
- Active runtime and build surfaces no longer identify `3.9.0`.
- Historical imported evidence and provenance filenames remain intact.
- A version identity static audit prevents regression.
- Static audits, portable C++20 tests, and the full MSVC x64 Release build pass.

## Completed package: V14-02

- Added a deterministic pure C++ analog model with ramps, holds, releases,
  opposing axes, diagonal input, disconnect, reconnect, fault, and recovery.
- Added a compile-time and command-line gated native backend.
- The simulator uses common curve, SOCD, report construction, ViGEm scheduling,
  telemetry, and shutdown paths.
- The Windows scenario proves neutral SOCD output, non-neutral diagonal output,
  disconnect/fault neutralization, accepted ViGEm changes, and clean shutdown.
- Simulator sources are excluded from the production compile.
- Evidence is always labelled simulated and never closes hardware gates.

## Completed package: V14-03

- Portable installs retain exact-resource verification beside the EXE.
- Protected installs fall back without elevation to a versioned per-user
  runtime under `%LOCALAPPDATA%`.
- Writes are atomic, flushed, and verified byte-for-byte after replacement.
- A stale or corrupted per-user DLL is self-repaired from the embedded resource.
- The verified absolute DLL path is quoted and passed explicitly to the child.
- System Wooting SDK and global UAP download/install recovery was removed.
- ViGEmBus remains the only offered external runtime dependency.
- Static, portable, forced-fallback, corruption-repair, production build, and
  runtime shutdown gates pass.

## Completed package: V14-04

Implemented locally:

- one machine-readable lock owns Sun, Soup, ViGEm, Action commits, runner
  labels, and toolchain families;
- Sun and Soup bootstrap by immutable commit, ignore arbitrary tools from
  `PATH`, and apply a five-file hash-locked Soup overlay;
- local and CI portable gates both require a C++20 compiler;
- the official Windows build fails on every warning outside the documented
  `LNK4099` ViGEm PDB allowlist;
- workflow Actions use full commit SHAs and fixed runner labels;
- a static audit prevents mutable dependency and gate regressions.

The corrected independent clone completed fresh dependency bootstrap, portable
C++20 tests, the full Windows build, and a production runtime smoke without
using repository build caches. GitHub Actions remains an optional
post-publication check.

## Completed package: V14-05

- Replaced the lossy global `g_started` array with a fixed-capacity,
  generation-aware lifecycle registry protected by a mutex and owner-thread
  contract.
- Changed the internal native descriptor ABI to return a generation-scoped
  `StopResult`.
- A timed-out, faulted, or malformed stop result leaves the entry `Poisoned`;
  reset cannot erase that state and restart is rejected.
- Exact lifecycle diagnostics are available through a registry snapshot and
  critical stability trace.
- SparkLink and Sayo no longer report forced termination as a successful
  cooperative join.
- Failure-injected portable tests cover wrong-thread access, failed start,
  normal join/restart, timeout poisoning, and stale-generation poisoning.
- Portable/static gates, full MSVC x64 Release build, and the deterministic
  analog simulator runtime scenario pass.

`TerminateThread` removal and bounded cooperative shutdown remain open and
begin in V14-06; V14-05 makes those failures truthful instead of masking them.

## Completed package: V14-06

Migrate workers one ownership boundary at a time to bounded cooperative
shutdown, beginning with the realtime loop. Protocol bytes, mappings, and
polling behavior remain characterization-locked during each migration.

Progress:

- `V14-06A` realtime loop: Verified locally. Stop wakes the address wait,
  returns a generation-scoped result, and closes the thread HANDLE only after
  confirmed completion. Timeout/failure retains ownership, poisons restart,
  guards backend teardown, and uses process-level exit without CRT destruction
  if a potentially live realtime worker survives final shutdown.
- `V14-06B` diagnostic logger: Verified locally. Shutdown closes its producer
  gate, wakes and drains the writer, and releases HANDLE/file/event ownership
  only after a confirmed join. Timeout retains all reachable resources,
  poisons restart and uses process-level exit without CRT destruction.
- `V14-06C` overlay server: Verified locally. Stop wakes both `accept` and
  client `recv`, and releases the worker HANDLE and WSA ownership only after a
  confirmed join. Timeout retains reachable ownership, poisons restart and
  prevents dependent application teardown. Loopback `/state` and forced-timeout
  simulator scenarios pass.
- `V14-06C.1` overlay responsiveness hotfix: Verified locally. The periodic
  `/client_perf` response and all close-framed error responses now close their
  connection immediately, so the single HTTP worker cannot wait for the
  five-second receive timeout before returning to `/state`. A socket-level
  regression gate measured the next state response at 0.3 ms in simulation and
  0.4 ms in production. The canonical production artifact is `HallJoy.exe`.
- `V14-06D` SparkLink: Verified locally. The hotplug worker has its own
  serialized generation inside the registry generation; stop signals its event,
  cancels HID I/O and releases thread/HID/event HANDLEs only after confirmed
  join. Join or lifecycle-lock timeout poisons restart and prevents dependent
  teardown. The outer generation represents the long-lived hotplug service, so
  a worker connected after initial device absence is still stopped at shutdown.
  Protocol discovery, claim policy, commands and polling are unchanged.
- `V14-06D.1` SparkLink service-stop hotfix: Verified on the Irok MG75 Max.
  Final service stop closes a dedicated outer
  start/reconnect gate before joining the active poller. The production trace
  has three balanced worker generations, two unplug/reconnect cycles, analog
  input before and after reconnect, and no reconnect/device-open/connect after
  `service.stop.begin`.
- `V14-06E` Sayo shutdown: Implemented and verified locally without hardware.
  All readers share one generation and one three-second group deadline. Stop
  signals the shared event, cancels every HID operation and releases reader,
  HID and event HANDLEs only after the whole group joins. Timeout retains the
  group, poisons restart and prevents dependent teardown. The old worst case of
  sequential per-reader waits followed by `TerminateThread` is removed. Stop
  publishes neutral analog input before cancellation and again after join.
- `V14-06F` Sayo exception and completion boundary: Verified locally without
  hardware. Every reader enters through the common allocation-free C++ barrier
  inside a separate SEH wrapper. Faults retain a fixed per-reader diagnostic,
  neutralize published input, stop the complete reader group and publish
  completion. Startup rejects an already exited or faulted reader generation;
  loss of the final live reader clears connected state.
- The simulator C++-fault injection, the earlier blocked-reader timeout
  injection, the normal common-pipeline scenario, all static/portable tests and
  the official production MSVC build pass. Production contains no ordinary
  `TerminateThread` call.

Sayo protocol/device compatibility is not inferred from simulation. It remains
an explicit device-owner gate in `V14-12`, together with the post-change
SparkLink regression and long-run qualification.

## Completed package: V14-07

Harden the isolated analog host and private UAP boundary: generation ownership,
partial-start rollback, bounded restart/unload and exception-safe C ABI exports.
This package must preserve the self-contained runtime and exact ABI behavior
verified in `V14-03`/`V14-04`.

Progress:

- `V14-07A` analog-host parent generation: Verified locally. The bridge and
  supervisor are one lifecycle generation; a confirmed group join is required
  before their HANDLEs, IPC mapping/events or child job are released.
- Supervisor partial-start failure now requests stop and joins the already
  created bridge before rollback. A failed join retains all reachable resources,
  poisons the generation and rejects restart.
- Final shutdown has bounded graceful and child-job containment phases. If a
  parent worker still does not join, backend shutdown reports failure and the
  application selects process-level containment without dependent teardown.
- Simulator-only bridge-timeout and supervisor-start-failure scenarios pass,
  as do the normal simulator, all static/portable gates and the production MSVC
  build. These tests prove lifecycle containment, not keyboard compatibility.
- `V14-07B` analog-host worker and child exit safety: Verified locally. Both
  parent Win32 workers and the isolated child entry have C++ and SEH barriers
  with neutral fault publication. A child HANDLE is retained until confirmed
  process completion; job-assignment or reap failure blocks replacement.
  Parent-fault, child-fault/restart and child-reap-timeout simulator gates pass.

- `V14-07C` private UAP safety: Verified locally. Every throwing C export is
  contained by a common exception barrier, all Soup mutex ownership is RAII,
  initialization state and null arguments are truthful, and device workers are
  cancelled and bounded-joined without holding the global devices mutex.
- The child host uses the optional private bounded-unload export. If plugin
  worker completion cannot be confirmed, it does not unload the DLL or run CRT
  teardown beneath a live worker; the disposable child process terminates and
  the existing parent/job containment owns recovery.
- The official build now runs a real ABI1 load/init/null/unload state gate after
  compiling the private plugin. Static audits, portable exception/RAII tests,
  production MSVC build and a process-clean smoke test all pass.

The Irok MG75 Max proved native SparkLink identification, successful vendor
polling and analog-row changes. `V14-06D.1` now suppresses the shutdown-time
reconnect and passed its held-key unplug/reconnect and balanced-shutdown hardware
gate. `HJ-V14-P1-004`, V14-07 and V14-08 are Verified; V14-09 is next.

## Completed package: V14-08

Split the package at the ownership boundary so startup/wake publication changes
do not obscure the later ViGEm worker transition.

Progress:

- `V14-08A` startup transaction and wake correctness: Verified locally. Backend
  dependents start as one transaction and publish readiness only after realtime,
  required native phases and Raw Input prerequisites succeed. Optional absent
  protocol families remain valid; a present backend failure or rejected
  lifecycle ownership aborts startup.
- Rollback follows reverse acquisition order. It stops at the first unconfirmed
  join, retains dependent ownership and selects process-level containment rather
  than tearing backend state down beneath a live worker.
- Realtime input notifications now use a process-lifetime monotonic sequence.
  Notifications before worker start, during restart, or between the final check
  and `WaitOnAddress` remain observable; the address wake is only a latency hint.
- Curve invalidation now release-publishes an ordered generation and every
  thread-local curve cache acquire-observes it before reading atomic settings.
- Portable concurrency tests, static audit, two startup fault injections, the
  normal simulator, the official production build and an Irok MG75 Max startup/
  shutdown smoke all pass.
- `V14-08B` ViGEm output isolation: Verified locally. Realtime now publishes
  complete newest-state report batches through a non-blocking latest-value
  mailbox. A dedicated output worker exclusively owns runtime driver updates,
  reconnect and teardown after initial startup creation.
- Pending multi-pad masks are merged while every report payload is refreshed
  from the newest complete snapshot. Portable equivalence tests cover coalesced
  pads and reject stale intermediate payloads.
- Output stop has one three-second bound. If a driver call does not return,
  handles and driver ownership remain retained, dependent backend cleanup is
  skipped, and final shutdown selects process containment instead of destroying
  state beneath the live worker.
- The simulator-only 60-second driver stall left realtime responsive through
  the complete input scenario and produced the expected bounded timeout/exit 2.
  Normal simulation, the official production build and an Irok MG75 Max
  startup/shutdown smoke all pass with balanced output-worker ownership.
- `HJ-AUD-P1-010` is Verified.

V14-08 is complete. The next implementation package is V14-09 transactional
persistence and writable state migration.

## Completed package: V14-09

The package is split so the transactional write primitive is verified before
it is used for migration or every remaining data format.

Progress:

- `V14-09A` settings, overlay and bindings persistence: Verified locally.
  Every save uses a collision-resistant temporary file in the destination
  directory, checks writes and stream state, physically flushes, parses schema
  markers back, and commits only through a write-through atomic replace.
- Save failures now publish a stage/error/path trace and reach a production UI
  error even when an autosave caller cannot consume the return value. Profile
  switch/create/manual-save paths additionally retain the old active/dirty
  state when persistence fails.
- Portable stage-failure tests and Windows simulator injections cover prepare,
  write, flush, validation and replace. For settings, bindings and overlay
  probes, every injected failure preserved the known-good SHA-256 and left no
  transaction temp file.
- The official production build and Irok MG75 Max startup/shutdown smoke pass;
  user settings, bindings and presets were restored hash-identically.
- `V14-09B` layout, curve preset and curve-state persistence: Verified locally.
  These formats now use the same checked transaction and schema readback.
  Layout candidates and curve active-state metadata do not commit in memory
  after a failed save, and partial active-preset rename outcomes are reported.
- All five Windows failure stages preserve known-good hashes for settings,
  bindings, overlay, layout, curve preset and curve state, with zero temporary
  files left behind. The production build also reads the user's pre-transaction
  layout/state files and shuts down without a persistence error.

`V14-09C` writable-state migration and filename hardening: Verified locally.
Production now defaults to `%LOCALAPPDATA%\HallJoy`; an explicit
`HallJoy.portable` marker selects EXE-local state only after a physical write
probe succeeds. The one-time source-specific migration retains the legacy
files, creates a byte-validated transactional backup, preserves existing target
files and commits a validated replay marker last. All five injected transaction
failures block startup without changing the source or leaving a destination or
temporary file. Profile, layout and curve names share NFC normalization,
invariant case keys, DOS-device avoidance, trailing-dot/space cleanup, an
80-code-unit bound and direct-child path validation. Simulator policy tests,
real production migration/replay and Irok balanced shutdown all pass.

V14-09 is complete. The next implementation package is V14-10 IPC and overlay
security/correctness.

## Completed package: V14-10

Split IPC ownership/authentication work from overlay protocol work so every
change has a narrow behavioral gate.

Progress:

- `V14-10A` Mouse IPC creation and atomic-read correctness: Verified locally.
  The `CreateFileMappingW` result is captured before `MapViewOfFile`, a new
  mapping publishes its schema marker last, and an existing mapping is never
  cleared. Existing mappings must match the stable 40-byte v1 schema; the old
  zero `reserved1` slot is accepted and upgraded in place without moving any
  field or changing the public mapping name.
- Peer-owned attach and heartbeat fields are acquire-like interlocked reads.
  The simulator policy gate proves preservation of existing payload, legacy
  size-slot upgrade, rejection without overwrite of an invalid schema, and
  atomic reads. Static audit, storage/failure regression suites, overlay
  lifecycle, production build and Irok route/balanced-shutdown smoke pass.
- `HJ-AUD-P1-011` and `HJ-AUD-P2-005` are Verified. No external ASI binary was
  available for an attach test, so the compatibility claim is limited to the
  unchanged name, offsets, version and simulator-emulated legacy slot.
- `V14-10B` analog-host IPC capability transport: Verified locally. Mapping,
  stop/snapshot events and the owner-process reference are unnamed kernel
  objects passed only through `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`; the child no
  longer discovers any transport object by name. A CSPRNG token and owner PID
  bind shared schema v10, the child verifies every inherited handle/schema/
  owner tuple, and the parent accepts Ready only from the PID it created.
- The invalid-handle simulator gate proved exit 31 before shared-state use,
  followed by one bounded restart and normal pipeline completion. All five
  prior analog-host generation/fault scenarios, static/portable gates, the
  production build and Irok route/balanced shutdown also pass.
- `HJ-AUD-P1-012` is Verified. This removes named precreation/spoofing; it is
  not a sandbox against a process already authorized to tamper with HallJoy's
  process handles or memory.
- `V14-10C` overlay HTTP framing and bounded parsing: Verified locally. The
  loopback server now accumulates fragmented input, consumes one exact request
  frame at a time and serves pipelined frames without treating a `recv` chunk
  as a message. Strict `Content-Length` parsing, transfer-coding rejection and
  8 KiB header, 4 KiB body and 2 KiB target limits produce closing
  `400`/`413`/`414`/`431` responses.
- Telemetry fields are exact query keys converted with `from_chars`, reject
  duplicates, junk and overflow, and are capped at one billion before any
  counter update. Simulator and production socket regressions, the complete
  static/portable gate, timeout containment, official build and Irok route/
  balanced shutdown pass. `HJ-AUD-P2-002` and `HJ-AUD-P2-003` are Verified.
- `V14-10D` overlay concurrency, origin policy and shutdown: Verified locally.
  Accepted sockets run in a fixed table of 16 independent workers; saturation
  is rejected promptly, while eight slow clients and eight concurrent state
  requests remain responsive. Stop shuts down every client and joins all
  client workers before releasing WSA ownership.
- Each overlay generation has a new 128-bit CSPRNG session cookie. State and
  telemetry require that cookie; browser requests accept only the exact bound
  loopback origin, echo it with `Vary: Origin`, and never publish wildcard
  CORS. Hostile and `null` origins are rejected without issuing a cookie. An
  open overlay page refreshes the root once after a stale-generation `401`, so
  restarting the server does not require a manual browser refresh.
- Static and socket gates, active-client shutdown, prior forced-timeout
  containment, official production build and Irok route/balanced shutdown all
  pass. `HJ-AUD-P2-001` and `HJ-AUD-P2-004` are Verified.

V14-10 is complete. The next implementation package is V14-11 UAP pacing,
identity, modularization and measured performance.

## Completed package: V14-11

- `V14-11A` UAP poll pacing: Verified by automated gates. All six private
  plugin targets use a 1000 us start-to-start
  deadline for poll transports. Transactions that already consume the
  deadline receive no additive delay; transient Madlions failures use bounded
  2..64 ms exponential backoff. Wooting, Razer and NuPhy report-stream paths
  are unchanged. Static/portable gates, the rebuilt private ABI1 identity and
  unload gate, the official production build and a native Irok regression all
  pass. An actual UAP poll keyboard is not available, so deterministic timing,
  GCC/MSVC, Clang ASan+UBSan and production integration are the acceptance
  gate; no physical CPU/USB/latency claim is inferred.
- `V14-11B` UAP device identity: Verified by automated gates. The exact
  production function derives a versioned 64-bit ID from the normalized Soup
  HID interface path plus descriptor. IDs are occurrence-independent when a
  path exists; the pathless metadata fallback remains unique but is explicitly
  not marked duplicate-safe. All 40,320 permutations of eight identical
  devices, 100,000 reconnect/subset generations, 250,000 synthetic paths,
  1,024 fallback occurrences, normalization cases and persisted-ID golden
  vectors pass GCC, MSVC and Clang ASan+UBSan. This proves enumeration stability,
  not physical identity after moving a serial-less keyboard to another port.
- `V14-11C` snapshot export contention: Verified by automated gates. The device
  registry now owns `shared_ptr` objects, and both telemetry and dense exports
  copy at most eight owner pins under the global mutex before any per-device
  lock or 256-value copy. Hotplug removal and bounded unload retain matching
  pins outside the registry lock. A deterministic blocked-reader/removal test,
  100,000 lifetime cycles, 50,000 coherent copies, GCC/MSVC, ASan+UBSan, ABI,
  official build and native Irok regression pass. No physical UAP latency claim
  is inferred.
- `V14-11D` exact HID interface ownership: Verified by automated gates. Native
  routing now claims a normalized full interface-path fingerprint after protocol
  proof, not a VID/PID pair. Same-VID/PID sibling interfaces remain independent,
  catalog priority is first-claim-wins per exact path, and every native enumerator
  rejects foreign ownership before any HID open. Soup's pre-open hook and the
  redundant plugin guard call the same shared implementation. Same-pair sibling,
  exact-token, 10,000 reorder/reconnect and 300,000-path collision tests pass GCC,
  MSVC `/W4 /WX`, Clang ASan+UBSan, ABI, official build and native Irok regression.
  This proves code-level routing under D-022, not physical multi-UAP hardware.

V14-11 is complete. The next implementation package is V14-12 release
qualification and the hardware matrix.

## In-progress package: V14-12

- `V14-12A` Aula WIN 60 HE MAX support: Implemented at firmware/code level.
  The supplied archive passed path, manifest and complete Git-bundle checks.
  Exact firmware verification passed 57 checks; ten oracle source files matched
  independently fetched fixed npm packages, and the official oracle reproduced
  the archived JSON hash.
- Production accepts only `1CA2:1902`, `FFA0:0001`, exact 65-byte input/output,
  `App V1.1.6 / Feb 4 2026`, canonical precision and layout. A full 17-read
  capability proof occurs on one exclusive session before exact-path ownership.
  No coarse VID/PID reservation or mutating command is present.
- Protocol, oracle, end-to-end and session-policy suites pass GCC 15.2, MSVC
  `/W4 /WX` and Clang 21 ASan+UBSan. All 39 repository audits and 26 portable
  tests, official build, private UAP ABI and overlay gates pass.
- Available-hardware regression: Irok MG75 Max completed 65,379/65,379
  SparkLink route queries with zero failures, balanced shutdown and unchanged
  user state. This proves absence of regression on the available native route.
- Aula remains `hardware-unvalidated`: no physical input, held-key reconnect,
  multi-device or alternate-firmware claim is made. This gate stays open in the
  V14-12 hardware matrix.
- `V14-12B/S06` removes Addressed cross-thread HID handle close. The reader owns
  handle, buffer, event and `OVERLAPPED` through terminal reap; the outer native
  worker has a three-second truthful join and retains all generation resources
  on timeout.
- Addressed timeout containment passes a deterministic simulator process gate;
  packet constructors and session polling core are token-identical. The 15-second
  Irok regression completed 57,161 successful routes, balanced shutdown and no
  trace ERROR. Physical Addressed qualification remains open.
- `V14-12C/S07` migrates Hex80 to a waitable native generation with cooperative
  HID cancellation and one truthful three-second join. Timeout retains the
  thread, wake event and any active worker-owned HID session, blocks restart and
  selects process containment.
- Hex80 timeout containment passes its deterministic simulator gate; protocol
  sources and GET-only routing remain unchanged. The 15-second Irok regression
  completed 57,276/57,276 successful routes with zero trace ERROR and unchanged
  user state. Physical Hex80 qualification remains open; MAD68 is the next and
  last backend in S07.
- `V14-12D/S07` migrates MAD68 to a waitable native generation. Owner stop
  cancels only the persistent overlapped read; the worker retains session HANDLE
  ownership, rejects any new A8 after stop and still performs final A9 recovery.
- MAD68 timeout containment, 41 audits, 26 portable tests, `/W4 /WX`, official
  build and Irok regression pass. Protocol, transports, A8/A9 strategy, restore
  and decoder remain unchanged. S07 is code-complete; physical MAD68, Hex80 and
  Addressed qualification remains open.
- `V14-12E` adds a normal-operation release qualification runner. The post-build
  gate completed 25/25 production start/WM_CLOSE cycles with exit code zero,
  complete error-free shutdown traces, no remaining HallJoy process and 11/11
  unchanged LocalAppData files. Per-cycle peak HANDLE count returned to 218
  after one transient 225 sample; shutdown took 138-326 ms.
- V14-12E is a verified harness/pilot, not the final S21 gate. The required
  1000-cycle run, 8-24 hour soak, reconnect/input exercise and unavailable
  device-owner matrix remain open.
- `V14-12F/S18` removes the embedded dependency installer instead of trying to
  contain an unsafe privileged path. HallJoy no longer resolves `latest`,
  downloads to temp, verifies then executes a mutable file, invokes `runas` or
  `msiexec`, or waits for an installer on the startup/UI thread.
- Missing ViGEm now produces truthful manual guidance for the exact official
  ViGEmBus 1.22.0 release page. Version, URL and `manual-only` policy are pinned
  in the central dependency lock and matched to the production constant by
  static gates. S18 is Verified; no physical-device claim is inferred.
- `V14-12G/S20` completes the pre-qualification build/documentation package.
  Current project guides use only shipped commands and the canonical
  `build/output/HallJoy.exe`; the stale Addressed validator now checks the
  catalog-driven lifecycle and runs in the unified gate. The old v3.9 validation
  package is prominently historical.
- Unsupported Win32/x86 configurations are removed because only x64 ViGEm/UAP
  artifacts ship. Debug and Release x64 now use W4 with the documented
  `4100/4127/4324/4505` legacy baseline; the official build reports zero
  unexpected warnings. Full automated gates and a normal Irok 3-cycle regression
  pass. S20 is complete; S21 qualification is next, while Aula testing continues
  asynchronously and remains release-blocking.

## Release definition

v1.4 can be tagged only when:

1. all P1 risks in the v1.4 register are `Verified` or explicitly excluded with
   a release-blocking decision;
2. all automated Windows and portable CI gates pass from a clean checkout;
3. the application starts without a system Wooting SDK;
4. dependency failures produce truthful, actionable diagnostics;
5. settings and profiles survive injected write failures;
6. the required device matrix and long-run qualification are complete;
7. the release archive, executable, sources, notices, and evidence have recorded
   SHA-256 values.

### V14-12H / S21 qualification automation

- Added crash-persistent progress/evidence to the normal production cycle
  runner, including before/after state manifests and SparkLink route counters.
- Added an eight-hour-by-default production soak runner with periodic process,
  HANDLE, GUI-object, memory and CPU samples; overlay responsiveness probes;
  fixed post-warm-up leak gates; bounded graceful shutdown; and trace analysis.
- Static contracts, 45 audits, 27 portable tests, the official Release x64 build
  and a post-build 3/3 Irok regression pass. A one-minute overlay soak pilot
  stabilized at 210 HANDLEs and completed 234,846/234,846 Spark routes.
- This package makes the final S21 runs reproducible; it does not itself close
  the pending 1000-cycle, 8-24-hour, manual reconnect/input or Aula hardware
  gates.

### V14-12I / S21 1000-cycle qualification

- Final production lifecycle gate completed 1000/1000 PASS on the exact
  `HallJoy.exe` SHA-256
  `6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`.
- Every cycle used normal startup, one second of operation and bounded graceful
  `WM_CLOSE`: exit zero, complete trace, no ERROR/capping, no surviving process
  and unchanged 11-file LocalAppData state.
- Shutdown p50/p95/p99 was 250/430/1315 ms, maximum 2662 ms against the 15 s
  limit; maximum observed HANDLE count was 217 without accumulation.
- All 1000 trace SHA-256 values reverified. Spark accounting was exact:
  1,598,879 queries = 1,598,454 successful + 425 single shutdown-window
  cancellations.
- The 1000-cycle gate is closed. Remaining S21 work is the long soak, manual
  Irok input/reconnect check, unavailable device matrix and external Aula result.

### V14-12K / S21 one-hour soak finding and correction

- The user-approved proportional gate is one hour continuous production plus
  the already verified 1000 lifecycle cycles; 8-24 hours is not required.
- The hour run completed 703 samples, 12/12 overlay probes, stable resources,
  exit zero and unchanged state, but trace review correctly rejected a clean
  qualification claim: two `hotplug.stale` ages underflowed to near `UINT64_MAX`
  and caused two false SparkLink reconnects.
- Root cause: `Backend_Tick` could observe `nowMs` just before the worker
  published a newer `lastPacketMs`; unsigned subtraction treated the future
  packet as enormous silence. Production now saturates that age to zero.
- A pure C++ regression covers the exact high-bit shape. Current unified gate is
  46 static audits + 28 portable tests; official build has zero unexpected
  warnings. New `HallJoy.exe` SHA-256 is
  `81609DC44D12F7DF44C2A7D801D8992CBFDCB45221F07AE041ED4711F4EB840C`.
- Two-minute targeted production regression: 464,905/464,905 routes, one worker
  generation, zero stale/reconnect, 2/2 overlay probes, stable resources, exit
  zero and unchanged state. Per D-029, the hour/1000-cycle gates are not repeated.
- Raw evidence now defaults to `build/evidence`, outside destructive
  `build/output` cleanup. Earlier raw hour/1000 files were deleted by the next
  official rebuild; their independently verified committed summaries remain.

### V14-12L / S21 second-pass stability audit

- Shared monotonic arithmetic now saturates clock-age and remaining-deadline
  calculations. Sayo cannot mistake a newer worker timestamp for centuries of
  silence, and an expired Addressed deadline cannot become an infinite HID wait.
- Raw Mouse accumulation widens before clamping, avoiding undefined signed
  overflow at extreme injected deltas.
- Overlay and ViGEm output services are supervised under D-030. A contained
  worker exception is owner-reaped and restarted only after confirmed join;
  poison or retained ownership still fails closed.
- Clipboard and UAP early-error ownership is explicit: failed clipboard
  transfer frees its allocation, SetupAPI lists are released on every return,
  and vendor-state clearing uses the correct object extent.
- Deterministic arithmetic, 48 static audits, 29 portable tests, four Aula
  sanitizer suites, locked fresh-plugin build and both one-shot worker-fault
  tests pass. Official Release x64 has zero compiler warnings, and the resulting
  production artifact passes 3/3 Irok cycles with unchanged state. V14-12L is
  Verified; external Aula hardware remains the release blocker.

### V14-12M / S21 UAP shutdown containment and dependency provenance

- A tester report on MAD68 HE is explicitly routed through the modified private
  UAP and its internal Soup transport. MAD68 Pro R is a different keyboard on
  HallJoy's native A0 backend; no root cause is transferred between them.
- Shutdown heartbeat monitoring is disabled only after shutdown begins, so a
  stuck child unload is classified as `child.stop_timeout`, not as a runtime
  crash. The supervisor terminates the disposable child after its 2.5-second
  deadline, confirms exit and releases IPC without restarting it.
- A last-resort 12-second watchdog is armed before app cleanup and remains live
  through debug-log and stability-trace shutdown. A permanent owner-thread
  stall exits code 4 and leaves no HallJoy process.
- Sun, Soup and the modified UAP are MIT-licensed and exact-commit pinned. Their
  complete required notices now ship as `THIRD_PARTY_NOTICES.md`; upstream
  changes cannot enter a build until the lock and reviewed overlays are
  deliberately updated and the full qualification is rerun.
- The unified gate, permanent UAP-child stall, permanent process stall, normal
  simulator, clean locked UAP build and official Release x64 build pass. The
  2,210,304-byte `HallJoy.exe` SHA-256 is
  `C06AD4C7257244E4370738465BBADF815DBC41A081065CA30B0BCFF8059FA1A3`.
- Production Irok regression is 5/5, shutdown 122-226 ms, maximum 209 HANDLEs,
  33,461/33,462 Spark routes, zero survivor and unchanged 11-file user state.
  V14-12M code containment is Verified, but `HJ-V14-P1-008` remains Partial
  until the actual MAD68 HE tester repeats close on this EXE. Aula physical
  validation also remains release-blocking.

### V14-12N / S21 all-keyboard shutdown containment matrix

- The production catalog has six unique native routes: MAD68 Pro R, Hex80,
  Addressed, Aula WIN60HE, SparkLink/Irok and Sayo. Every route now has a
  simulator-only permanent worker-stop injection that must end in a bounded,
  truthful poisoned exit instead of an indefinitely resident process.
- The shared private UAP/Soup route has its separate permanent child-unload
  stall, and the independent 12-second application watchdog is exercised as
  the final containment layer. A normal common-pipeline scenario is the control.
- `tools/run_keyboard_shutdown_matrix.ps1` ran all 9/9 scenarios, preserved and
  hashed one trace per scenario, verified the expected exit/evidence and found
  zero surviving HallJoy processes. The unified static/portable gate and clean
  locked production build also pass.
- The exact 2,210,304-byte production `HallJoy.exe` has SHA-256
  `E12080E95DD394462FC36C842517F168F6CE4423CE9357B89D2320A20A962BB8`.
  Physical Irok regression is 5/5, 140-234 ms shutdown, maximum 209 HANDLEs,
  zero survivor and unchanged user state.
- V14-12N closes the code-level shutdown-matrix gap only. It does not claim
  physical protocol/input/hotplug proof for unavailable keyboards; physical
  MAD68 HE and Aula acceptance remain release-blocking.

### V14-12O / S21 input-to-overlay load qualification

- Added a production-only profiler that measures the complete HallJoy process
  tree, persistent worker TIDs, residual UI/short-lived workers, physical
  SparkLink HID transaction time, overlay build/send telemetry and a complete
  real Chrome process tree across idle, real-overlay and animated 32-key phases.
- Found the browser renderer redrawing the full canvas on every animation frame
  even after analogue values settled. Retained rendering now invalidates only
  on resize, layout/style or visible-depth change and converges smoothing to an
  exact idle value.
- Sprite/label caches are bounded at 512/256 entries and use constant-time
  insertion-order LRU. Fresh profiles default to an 8 ms polling interval;
  1 ms remains an explicit high-load option and existing settings are preserved.
- On the final production artifact, 1 ms real-overlay runs use 0.809-0.929% of
  the 12-thread machine for HallJoy and 5.451-6.120% for Chrome. An exact 8 ms
  run uses 0.721% and 3.747%. Physical SparkLink accounting is exact, user state
  is unchanged and no HallJoy process survives.
- The official build and five post-profile Irok cycles pass. V14-12O is
  Verified for the physically available Irok and shared downstream path;
  unavailable keyboard protocols retain their separate hardware gates.

### V14-12P / recoverable factory reset

- Global settings now exposes a styled owner-draw `Reset All Settings` action
  with a complete scope warning and `No` as the default confirmation choice.
- A confirmed reset writes a validated atomic request, performs the ordinary
  graceful shutdown, and relaunches only after all workers, logs, trace and the
  shutdown watchdog have completed teardown.
- Before any settings load, the new process moves `settings.ini`,
  `bindings.ini`, `GlobalProfiles`, `Layouts` and `CurvePresets` into a unique
  `FactoryResetBackups/reset-*` directory. Migration markers, logs and unrelated
  files are deliberately preserved.
- A partial move or fresh-directory failure rolls the exact targets back in
  reverse order. Incomplete rollback is reported truthfully with its recovery
  path instead of claiming that files were restored.
- Static contracts, a simulator failure after three real moves, byte-exact
  rollback, successful retry/backup verification, the official build and an
  unchanged-state Irok production cycle pass. V14-12P is Verified.
