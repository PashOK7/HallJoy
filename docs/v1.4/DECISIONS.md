# HallJoy v1.4 decision log

## 2026-09-06 — Unified build and archive layout (owner authorized)

Observed: generated x64 trees occupy source directories; root EXE is stale and
build/output duplicates build/release. Compared alternatives:
(1) cleanup only: cheaper but scripts recreate disorder;
(2) one build root, isolated variant/configuration/platform outputs, one release
package and indexed archives: fixes recurrence with bounded changes;
(3) move all source modules and rewrite the build system: broad include/test
churn without demonstrated runtime benefit. Adopt (2), retaining device semantics.

Contract: build/bin/<variant>/<configuration>/<platform>, build/obj with the
same suffix, build/release as sole ordinary distribution, build/packages for
named diagnostic distributions, build/evidence for runs. Dependency worktrees
use .cache; rollback snapshots use .local/backups. Old outputs and unique
evidence are archived with old/new paths and SHA-256; no age-based deletion.
Full pre-change snapshot: .local/backups/structure_20260906_122303 (4712 files,
all hashes verified). docs/current/PROJECT_LAYOUT.md will define current paths.
Validate clean production and diagnostic compilation, without keyboard/gamepad
runtime tests. Historical documents retain their dated evidence.


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

## D-016 - Writable state has one explicit root and one filename policy

Date: 2026-08-01

Production mutable state belongs under `%LOCALAPPDATA%\HallJoy`. EXE-local
portable state is opt-in through an ordinary, non-reparse `HallJoy.portable`
marker and is accepted only after a unique physical write/flush probe succeeds.
Simulator roots are explicit test-only overrides and never touch the user's
LocalAppData.

The first non-portable launch migrates supported legacy state from the EXE
directory without deleting or modifying the source. It transactionally creates
and byte-validates a source-specific backup, copies only missing destination
files, and commits a schema-validated source-specific marker last. A failed
stage blocks application startup; a completed marker makes replay idempotent.
The official package builder must preserve legacy mutable files so it cannot
erase migration input before the first launch.

Global profiles, layout presets and curve presets share one Windows filename
policy: NFC display normalization, invariant case-folded collision keys,
invalid/control replacement, trailing-dot/space removal, DOS-device avoidance,
an 80 UTF-16-code-unit stem limit and verified direct-child path construction.
Existing Unicode/case aliases remain readable; new collisions receive a
bounded suffix or are rejected where the UI requires a unique logical name.

## D-017 - Existing Mouse IPC mappings are peer-owned state

Date: 2026-08-01

The creation disposition of a file mapping belongs to the immediately
preceding `CreateFileMappingW` call and must be captured before mapping the view
or making any other Win32 call. Only a newly created Mouse IPC mapping may be
zero-initialized. A pre-existing mapping is external peer-owned state and must
be validated, never reset by the publisher.

The public v1 mapping remains `Local\HallJoy_MouseBridge_v1` and exactly 40
bytes. Its final former `reserved1` field now carries `structSize` without
moving any offset. Zero remains a compatible legacy value and is atomically
upgraded to 40; every other size, magic or version mismatch is rejected. Magic
is published last for a new mapping. Cross-process `LONG` fields that HallJoy
reads are accessed through interlocked operations rather than ordinary
volatile reads.

This decision closes Mouse IPC creation/schema and memory-order correctness
only. Analog-host authentication, ACL and precreation resistance remain the
separate V14-10B scope.

## D-018 - Analog-host IPC is an inherited kernel capability

Date: 2026-08-01

The isolated analog host must not discover transport objects through names.
The parent creates an unnamed shared mapping, unnamed stop/snapshot events and
a real handle to the parent process. `CreateProcessW` inherits only these four
capabilities through `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`; the child consumes
the inherited handle values directly. No analog-host mapping or event exists
in the named-object namespace, so another same-session process cannot win a
name-creation race or substitute an object under the expected name.

The internal shared schema is v10 and binds an owner PID plus a CSPRNG launch
token. Before publishing anything, the child validates all four handles, the
full schema tuple, token and `GetProcessId` result of the owner handle. The
parent separately compares the shared `hostPid` with the PID returned by its
own `CreateProcessW` before accepting Ready. A mismatch blocks restart.

This is a capability and accidental/spoofed named-object boundary, not an OS
sandbox. A process already permitted to modify HallJoy memory or duplicate its
private handles is outside this threat model. Child job containment and the
existing bounded generation lifecycle remain unchanged.

## D-019 - Overlay HTTP input is an explicitly bounded byte stream

Date: 2026-08-01

The loopback overlay server must never equate one `recv` result with one HTTP
request. Each connection owns an accumulator and an incremental HTTP/1.0/1.1
parser that waits for a complete header and exact `Content-Length` body,
consumes exactly one frame, and retains remaining pipelined bytes. Header,
body and request-target limits are 8 KiB, 4 KiB and 2 KiB respectively;
unsupported transfer coding, duplicate length fields and malformed framing are
rejected with a closing response.

Numeric query fields use complete `from_chars` conversion rather than a
wrapping decimal accumulator. Telemetry keys must match exactly, duplicates
and trailing junk are invalid, every metric is limited to one billion, and all
fields are validated before any aggregate counter changes. This decision owns
framing and numeric correctness only. Multi-client scheduling and browser
origin policy remain the separate V14-10D boundary.

## D-020 - Overlay clients are bounded owners with generation-scoped browser access

Date: 2026-08-01

The accept thread must not execute attacker-controlled request reads. It owns
only `accept`, completed-worker reaping and assignment into a fixed table of 16
client slots. Every assigned socket has one worker HANDLE and one exception
boundary. Saturation is rejected immediately; the `503` response is best effort
because Winsock may reset a newly accepted connection whose request remains
unread. Blocking the accept owner to guarantee delivery would reintroduce the
slow-client serialization this boundary removes.

Stop shuts down the listen socket and every owned client socket before joining
the accept generation. The accept owner does not complete until all client
workers have completed and their handles and sockets can be released. A failed
outer join retains the accept HANDLE, WSA and reachable client ownership and
poisons restart.

Every successful server generation creates an independent 128-bit token with
the Windows CSPRNG. Direct navigation to `/` publishes it as an `HttpOnly`,
`SameSite=Strict` session cookie. `/state` and `/client_perf` require the exact
cookie. A browser `Origin` header is accepted only when it exactly equals the
bound `http://127.0.0.1:<port>` origin; that value alone is echoed with
`Vary: Origin`. Missing Origin remains valid for direct same-host HTTP clients,
but does not bypass the session requirement. Wildcard CORS is forbidden.
The embedded page treats `401` as a generation change, fetches `/` once to
receive the new HttpOnly cookie and resumes its normal polling loop.

This is a loopback browser boundary, not authentication against another process
already able to read HallJoy's memory, browser cookie store or local traffic.

## D-021 - UAP vendor polling is deadline-paced by transport class

Date: 2026-08-01

Only devices for which Soup reports `isPoll()` may be paced by the private UAP
worker. Their production target is a 1000 us start-to-start deadline, not a
fixed sleep after every request and not a promise that every device samples at
1 kHz. A successful transaction waits only for the unconsumed deadline; an
already-slow transaction continues immediately. A tolerated Madlions report
failure waits with bounded exponential backoff from 2 through 64 ms, and the
next success resets the failure streak.

Wooting, Razer and NuPhy report-stream devices retain their existing blocking
read behavior. A separate telemetry flag identifies deadline-paced workers;
the old unthrottled flag is diagnostic-only. Portable timing arithmetic may
prove the scheduler contract and modeled busy-time reduction, but only an
actual UAP poll keyboard can verify CPU load, USB transaction rate and input
latency. Therefore the implementation and its hardware qualification have
separate statuses.

## D-022 - Unavailable UAP hardware is replaced only by production-code proofs

Date: 2026-08-01

The current owner has no UAP-routed keyboard and no access to two identical UAP
devices. V14-11 must therefore not remain permanently blocked on hardware that
cannot reasonably be obtained. A UAP code-level risk may be marked `Verified`
when the exact pure implementation included by production passes exhaustive or
high-volume deterministic properties, persisted-output golden vectors, GCC and
MSVC warning-clean builds, Clang ASan+UBSan, the complete official build, real
ABI load/unload and a production regression through the available native Irok
route.

This substitution verifies scheduling arithmetic, identity mapping, ordering,
fallback and integration behavior. It does not measure a physical USB bus,
device firmware latency or driver-specific path volatility, and documentation
must say so next to every result. A 64-bit ID collision cannot be disproved
mathematically; high-volume collision smoke and versioned golden vectors are
the practical regression gate. A serial-less device moved to another port is
defined to follow the new HID interface path because no software-only test can
recover unknowable physical identity.

## D-023 - Snapshot readers pin owners before leaving the device registry

Date: 2026-08-01

The private UAP device registry owns each `Device` through `shared_ptr`. A
snapshot or telemetry export may hold `devices_mtx` only while copying a
bounded, fixed-capacity list of owner pins. It must release that mutex before
waiting for `snapshot_mtx`, reading telemetry or copying the 256-value dense
body. Export-time pin capture performs no device-object allocation.

Removal keeps a pin while invoking the disconnected callback outside the
registry mutex. Bounded unload similarly pins every worker before cancellation
and join. Erasing a registry entry therefore cannot invalidate an export,
callback or unload operation already using that device, and a slow snapshot
reader cannot delay registry removal by retaining the global lock.

Automated tests may prove lock ordering, coherent copies and object lifetime.
Without a UAP-routed keyboard they do not establish a physical latency or USB
throughput number; D-022's evidence boundary still applies.

## D-024 - Native ownership is exact HID interface-path ownership

Date: 2026-08-01

VID/PID identifies a product family, not one openable HID interface. Native
arbitration therefore uses a compact fingerprint of the complete normalized
SetupAPI interface path. ASCII case and slash direction are normalized; the
shared token contains a 64-bit hash and normalized UTF-16 unit count. Token-list
membership is exact and delimited, never a path substring search.

The first protocol that completes its capability proof owns that exact path.
Same-VID/PID sibling interfaces remain unclaimed unless independently proved.
Every native enumerator must reject a foreign exact claim after obtaining the
SetupAPI path and before any HID open, then claim the exact path it proved.
Reconnect may reopen only that protocol's prior path once routing is published.

HallJoy publishes the exact token list before starting the isolated UAP. Soup
calls a plugin-owned shared hook before `CreateFileW`; the later discovery guard
checks the actual UTF-8 Soup path through the same algorithm. The Soup patch may
not carry a duplicate hashing or substring-matching implementation.

Under D-022, deterministic production-code properties, three compiler modes,
sanitizers, ABI/build and available native Irok integration are sufficient to
verify this code-level risk. This does not prove physical multi-UAP coexistence
or mathematically rule out every collision in a finite 64-bit fingerprint.

## D-025 - Aula support may ship only as explicitly firmware-proven

Date: 2026-08-01

The reproducible Aula WIN 60 HE MAX archive is accepted as protocol evidence
because its firmware verifier and official npm-source oracle were independently
reproduced, and the production implementation passes parser, end-to-end,
session-policy, ambiguity, three-toolchain, sanitizer, build, ABI and available
Irok regression gates. This is sufficient to implement the exact read-only
backend without waiting indefinitely for unavailable hardware.

It is not sufficient to label Aula hardware-tested. Production accepts only
the exact `1CA2:1902`, `FFA0:0001`, 65-byte envelope and `App V1.1.6 / Feb 4
2026` proof. The backend claims no interface before all 17 read transactions
complete on the same exclusive handle, never reserves a VID/PID family, and
destroys a session after any transaction uncertainty. Multiple candidates fail
closed. Physical input, held-key reconnect, multi-device coexistence and other
firmware versions remain separate release-matrix gates.

## D-026 - Hex80 active HID ownership remains worker-local

Date: 2026-08-01

The Hex80 owner may publish stop, signal the wake event and request `CancelIoEx`,
but it may never close the active HID handle. The worker's `Session` owns that
handle through terminal reap. Its active-registration scope unwinds before the
session destructor closes the handle, and completed requests re-check stop before
decode or publication.

The worker is represented by a waitable `_beginthreadex` generation. Stop has
one 3000 ms deadline and releases thread/event ownership only after confirmed
completion. Timeout retains every reachable generation resource, returns a
truthful registry result, blocks restart and requires process containment. This
package must not change the Hex80 protocol, command bytes or GET-only proof.

## D-027 - MAD68 owner cancellation must preserve final A9

Date: 2026-08-01

Because A8 may place the keyboard in a temporary service mode, shutdown must
leave the worker able to send its idempotent final A9. The owner cancels only the
persistent overlapped read and never closes session handles. The worker withdraws
active-read registration before its own cancellation reap and read/write/control
handle close. New A8 commands are rejected once stop is published; late reads
cannot reach decode or publication.

MAD68 uses one waitable `_beginthreadex` generation and a 3000 ms join deadline.
Unconfirmed completion retains the full session stack and kernel ownership,
poisons restart and requires process containment. Protocol builders, write
transports, A8/A9 strategy, restore and decoder must remain unchanged in this
lifecycle package.

## D-028 - Remove privileged dependency installation from HallJoy

Superseded for current production UX by D-049. Its rejection of mutable
runtime downloads, predictable temporary paths and unbounded waits remains
binding historical rationale.

HallJoy must not download or elevate executable content. Missing ViGEmBus is a
recoverable dependency diagnostic, not authority for the application to resolve
a mutable `latest` asset, write it to a shared predictable temp location and
run it with administrator rights.

The only accepted recovery path is manual guidance to the exact official
ViGEmBus 1.22.0 release page. Version, URL and `manual-only` policy are pinned in
`tools/dependency-lock.json` and checked against the immutable production
constant. HallJoy never reports `Installed`; it stays in degraded mode until the
user installs the dependency and restarts. This structurally removes both TOCTOU
and unbounded installer-process waiting from the application.

## D-029 - Proportionate v1.4 long-run qualification

Date: 2026-08-01

The v1.4 release gate is one hour of continuous production operation with
overlay probes, resource sampling, complete trace analysis and unchanged user
state, combined with 1000 independent production start/graceful-stop cycles.
Longer observation increases confidence but cannot prove the absence of a fault
at an arbitrary later hour, so 8-24 hours is no longer a mandatory v1.4 claim.

If the one-hour run finds a narrow defect, the corrected artifact does not need
to repeat both expensive gates when the change is isolated by a deterministic
regression, the full automated/build gate passes, and a targeted production
runtime verifies the affected path. The evidence must explicitly preserve the
original finding, the exact old/new executable hashes and the proportional
requalification decision. Device-owner and external Aula gates are unaffected.

## D-030 - Fault-contained service workers are owner-reaped and supervised

Date: 2026-08-01

An exception barrier prevents a C++ exception from escaping a Windows thread,
but containment alone is not service recovery. A completed overlay or ViGEm
output generation must publish its stopped state and remain owned until the UI
owner observes the signaled thread handle, requests stop, confirms the join and
releases that generation's resources. Only then may a replacement generation
start. A timeout, retained client owner or poisoned lifecycle fails closed and
must never create overlapping generations.

For ViGEm, recovery recreates the driver transport only after the old output
worker is confirmed joined, then starts a new output owner and requests a fresh
realtime report. Overlay command-line/autostart intent is supervised and is not
erased merely because its worker faulted; an explicit user stop still clears
that preference. Healthy supervision runs every 30 UI timer ticks and performs
only atomic reads plus a zero-time handle query, outside the realtime path.

Recovery must be proven by one-shot production C++ fault injections that show
the failed generation reaped, a new generation started, normal work resumed and
final shutdown balanced. Fault-injection arguments remain simulator-only and
must not appear in release qualification commands.

## D-031 - UAP and Soup are pinned layers, not alternative runtimes

Date: 2026-08-01

HallJoy's universal route loads a locally modified Universal Analog Plugin in a
private child process. Soup is the plugin's lower-level HID/device library; it
is not an alternative selected instead of UAP. Sun is used only to build the
plugin. The exact Sun and Soup commits and every reviewed Soup overlay are
immutable inputs in `tools/dependency-lock.json`.

The pinned UAP, Soup and Sun sources are MIT-licensed. Distribution must retain
their copyright and permission notices, so the official build must place
`THIRD_PARTY_NOTICES.md` beside `HallJoy.exe`. An upstream release never changes
HallJoy automatically. Adoption requires an explicit lock update, source and
license review, overlay-hash update, clean rebuild, automated gates and relevant
hardware requalification. HallJoy's own license obligations remain separate.

## D-032 - Explicit shutdown is bounded through final trace teardown

Date: 2026-08-01

Subsystems keep their narrower cooperative deadlines and ownership rules. For
the private UAP path, a child that has not exited 2.5 seconds after stop is
terminated as a disposable process, confirmed reaped and never restarted.
Shutdown heartbeat loss must not be classified as a runtime crash.

The main process also arms one independent 12-second Win32 watchdog before the
first app cleanup/log call. The deadline exceeds the analog-host 6-second
graceful plus 4-second child-job containment budget, so it cannot pre-empt the
normal ownership policy. It remains armed through GDI+, debug-log and
stability-trace shutdown and is released only after all explicit teardown is
complete. If it fires, it uses no logger or CRT and terminates the process with
exit code 4. This last resort prevents a faulty driver or arbitrary teardown
lock from requiring Task Manager; it does not convert a forced exit into a
clean hardware qualification result.

## D-033 - Every production keyboard route requires deterministic containment

Date: 2026-08-02

Each unique native catalog route must have a simulator-only permanent-stop
process scenario that exercises its real production stop implementation,
requires the route-specific incomplete-stop trace, verifies the application's
poisoned exit and rejects a surviving process. A shared plugin-backed family is
tested once at its isolated UAP/Soup child boundary because all such keyboards
use that same owner and shutdown path. The independent process watchdog is one
additional route-agnostic proof, and a normal common-pipeline run is mandatory
as the non-faulted control.

The matrix preserves and hashes separate traces and labels its summary
`hardware_verified=false`. Passing it proves bounded code-level containment;
it must never be used to claim physical protocol, analogue input, hotplug,
multi-device or firmware compatibility.

## D-034 - Production load claims include every process and stop idle drawing

Date: 2026-08-02

Input-to-overlay qualification must measure the complete HallJoy process tree
and the complete browser tree separately. Main-process worker TIDs provide
named persistent stages; the difference between main-process CPU and those TIDs
is reported as UI/short-lived-worker CPU so ephemeral HTTP threads cannot vanish
from the accounting. Physical route counts and transaction time, server JSON/
send timing, browser fetch/render timing, resources, state preservation and
survivor checks are all mandatory. System-busy CPU is context, not HallJoy cost.

The browser canvas is retained. It may redraw only after resize, layout/style or
visible-depth invalidation; smoothing must converge to an exact idle value.
Sprite and label caches remain bounded at 512 and 256 entries and use
constant-time insertion-order LRU. A fresh profile defaults to 8 ms polling to
avoid spending browser CPU on imperceptible duplicate fetches. The 1 ms setting
remains an explicit maximum-load option, and upgrades never rewrite an existing
choice.

Headless Chrome qualification disables background throttling for repeatability.
Its CPU is an upper-pressure comparison, not a claim about exact OBS usage on
another PC. Physical Irok proves SparkLink plus the shared downstream path;
other keyboard transports retain their independent physical gates.

## D-035 - Factory reset is a recoverable restart-time transaction

Date: 2026-08-02

A UI reset request must never delete mutable state in the live process. HallJoy
first persists a validated atomic request, then uses its ordinary graceful
shutdown path. A replacement process may start only after worker, GDI+, logger,
stability-trace and watchdog teardown is complete, and it applies the request
before loading any settings.

Reset scope is the exact set `settings.ini`, `bindings.ini`, `GlobalProfiles`,
`Layouts` and `CurvePresets`. Each existing ordinary target is moved with
write-through semantics into a unique `FactoryResetBackups/reset-*` directory;
it is not deleted. Migration-completion markers, logs, runtime dependencies and
unrelated files remain in place. This prevents a reset from replaying one-time
legacy migration or turning diagnostics into user-state loss.

The request marker is removed only after fresh state directories exist. Any
earlier failure removes newly created directories and restores moved targets in
reverse order. A complete rollback permits a normal retry; an incomplete
rollback stops startup and reports the recovery directory truthfully. No code
path may describe an unverified rollback as successful.

## D-036 - Long settings pages share conditional themed scrolling

Date: 2026-08-02

Every settings page whose content can exceed its viewport must use the common
`CustomPageSurface` track/thumb geometry and interaction contract. The
scrollbar is conditional: no track or thumb is drawn when the entire content
fits. Remap, Configuration, Global settings, Input Overlay and Mouse settings
are overflow-capable and follow this rule.

V14-12R made Gamepad Tester the canonical route-complete diagnostics surface.
Its diagnostic line count is therefore unbounded by the adaptive gamepad-card
grid, so Tester now also adopts the common conditional wheel/track/thumb-drag
scrolling contract instead of clipping a multi-route snapshot.

Destructive actions must communicate danger through their resting fill as well
as border and text. Hover-only or border-only danger styling is insufficient
because the idle state is the primary state users see.

## D-037 - Live UI publication is visible-tab, changed-snapshot and region gated

Date: 2026-08-02

Backend dirty-key publication was initially limited to active Remap children;
this part is superseded by D-038 because the shared keyboard preview is visible
above every tab. Configuration and Gamepad Tester may sample live telemetry
only while one of those pages is visible, at a bounded cadence, and may post a
paint request only when their stable telemetry/report hash changes. Hidden
pages do not poll or repaint for presentation.

Configuration keeps static settings in `CustomPageSurface`; its brief live
source status is composed after the cache and invalidates only its own rect.
All route-complete backend diagnostics have one builder and one consumer,
Gamepad Tester. Configuration must not grow a second detailed diagnostics card.

Controls which present a list choice must be real themed child controls with
mouse, keyboard, focus and popup semantics. A painted rectangle that cycles a
value is not an acceptable combobox. Release-only code must not contain paint
telemetry; `HALLJOY_UI_AUDIT_TRACE` is a separate diagnostic target property.

## D-038 - Shared preview is event-complete; scroll layout is frame-committed

Date: 2026-08-02

The keyboard preview is owned by the main keyboard page, not by Remap, and is
visible above every sub-page. Backend dirty bits must therefore invalidate its
key HWND on every active tab before the bits are discarded. Page visibility
may gate page-specific telemetry, but never a shared visible consumer.

Continuous scroll input is a latest-value stream. Remap, Configuration and
Global settings store the newest thumb target and commit it at most once per
16-ms frame. Every affected child group is positioned with one deferred,
no-redraw batch; the parent then schedules one no-erase redraw. Synchronous
`RDW_UPDATENOW` and one-layout-per-`WM_MOUSEMOVE` are forbidden in these hot
paths. Slow diagnostics may use 100 ms sampling; gamepad motion UI samples its
report hash at normal UI cadence.

## D-039 - Scroll deadlines must not depend on WM_TIMER delivery

Date: 2026-08-02

`WM_TIMER` is a fallback drain mechanism, not the primary animation scheduler
under continuous pointer input. Each scroll mouse event compares the monotonic
clock with the next frame deadline and immediately commits the latest target
when due. A timer is armed only to deliver a pending final frame if input stops.

The scroll frame interval follows the configured UI cadence but is clamped to
8–16 ms. A 1 ms deadline tolerance compensates for integer clock granularity
and prevents a 15–16 ms input stream from degrading to every second frame.

## D-040 - One viewport architecture owns every scrollable keyboard page

Date: 2026-08-02

V14-12S.1 is superseded. Raising commit cadence while moving independent child
windows improved measured FPS but allowed visual elements to disappear. The
release architecture therefore treats mixed parent/child composition during
scroll as the root defect, not as a scheduling problem.

Remap, Configuration, Gamepad Tester, Global settings, Input Overlay and Mouse
settings must use `CustomPageScrollController`, content coordinates and the
common scrollbar. Retained pages present through `CustomPageSurface_Present`;
scroll-only updates change the viewport offset and do not move visible child
HWNDs. Dynamic Tester content may render live but may not introduce another
scroll state machine.

Native HWNDs are permitted only as transient popup/keyboard owners or hidden
compatibility action controllers. They are not visual scroll-layout elements.
Static architecture guards and the six-page production stress runner are
release gates. Automated PASS does not replace owner visual acceptance.

## D-041 - Retained controls reuse canonical painters and explicit popup lifecycle

Date: 2026-08-02

A retained page may own geometry and input routing, but it may not reproduce an
existing custom control with a generic button or local drawing approximation.
Closed PremiumCombo faces must use `PremiumCombo::PaintRetainedFace`, including
the canonical font, alignment, arrow section, border and extra icon.

Popup controller HWND visibility is a state machine: visible only while the
popup is logically open, hidden after selection, Escape or outside-click close.
`MsgDropStateChanged` is the mandatory owner notification. Text glyphs are not
accepted as icon substitutes where a vector renderer exists.

## D-042 - Retained migration preserves control semantics through explicit transactions

Date: 2026-08-02

Hit classification uses disjoint, bounded ID domains; numeric ordering is not a
type system. A binding edit is one transaction: persist active bindings, mark
the active global profile dirty, notify its retained surface and request normal
application persistence.

A finite named choice set is a PremiumCombo, not a button that cycles captions.
Retained pages render its canonical face and reveal the controller only for
popup and keyboard behavior.

## D-043 - Popup input is routed to the controller-owned combo state

Date: 2026-08-02

`PremiumCombo_Popup` is a separate top-level window and receives pointer wheel
messages directly. It must synchronously route `WM_MOUSEWHEEL` to its owning
controller; `hotIndex`, `scrollTop`, visibility and invalidation remain owned by
one implementation. The popup must not create a second scroll state machine.

## D-044 - Combo wheel moves the viewport only when the popup overflows

Date: 2026-08-02

Routing wheel input is insufficient if it invokes option navigation. While a
popup is open, wheel input may change only `scrollTop`, and only when
`items > visibleRows`. It must not change `curSel` or `hotIndex`. A fitting
popup ignores wheel input. Keyboard arrows remain the explicit option-navigation
mechanism.

## D-045 - Diagnostic continuation cannot weaken native ownership

Date: 2026-08-02

An aggressive hardware diagnostic may continue after semantic mismatches so
later read-only protocol stages are observable. It may not claim the HID path or
publish analogue values unless the complete strict proof has mismatch mask zero.
Transport timeout, malformed/correlation failure or exclusive-session loss
still poisons the session and requires reopen because Aula has no transaction ID.

Backends must also reject immutable identities dedicated to another native
family before `CreateFileW`. This prevents a broad protocol scanner such as
Spark from repeatedly probing Aula, without reserving an Aula-rejected path away
from UAP.

## D-046 - Physical envelopes may extend diagnostic parsing, not production trust

Date: 2026-08-02

A structurally valid response observed repeatedly on the exact physical HID
interface supersedes an inferred oracle for diagnostic traversal. The observed
60-byte Aula sync envelope may be accepted only under
`HALLJOY_AULA_AGGRESSIVE_TRACE` until its extended fields are authenticated.
The normal build keeps the pinned 54-byte contract.

Accepting an envelope is not firmware authentication. Any decoded identity
mismatch remains recorded and forbids route claim and analogue publication.
An intermittent sharing violation is not grounds for shared HID ownership when
a later trace proves exclusive open succeeds reliably.

## D-047 - Physical sync evidence supersedes the inferred Aula oracle

Date: 2026-08-03

The production Aula identity is the exact repeated 60-byte physical sync
descriptor, not the former 54-byte fixture generated without hardware. Three
16-byte blocks are length-delimited by `0x10`; immutable prefix, app/build
descriptors and `0xFF` trailer are compared exactly. Only the serial block is
device-specific and excluded from firmware equality while remaining reconnect
evidence.

Binary build-descriptor bytes must not be presented as a decoded build-date
string. Legacy 54-byte sync is rejected rather than retained as a compatibility
path, because there is no physical evidence for it.

## D-048 - Aula compatibility is a bounded protocol family, not a PID table

Date: 2026-08-05

The physically verified WIN 60 HE MAX identity remains an exact profile, but
production discovery may admit a sibling PID or firmware after a complete live
proof of the same 6x21 read-only protocol. Discovery is first restricted to
Aula VID `1CA2` or an Aula/SparkPlayJoy SetupAPI brand identity, then requires
`FFA0:0001`, exact 65-byte HID reports, a structural 60-byte sync descriptor,
plausible precision/travel, a unique dynamic default map, two identical Fn0
generations and both valid travel halves.

The active-map request count is derived from the proven physical positions and
bounded to nine batches per generation; the complete proof is bounded to 25
transactions. Arbitrary HID interfaces are never metadata-opened or probed.
Only the exact successfully proven interface path is claimed. A sibling passing
this gate is protocol-compatible, not physically validated, until independent
hardware evidence exists.

## D-049 - Restore one-click ViGEm installation without runtime downloading

Date: 2026-08-21

Downloading was not itself the defect in the old installer. The unsafe parts
were resolving a mutable `latest` executable at runtime, using a predictable
temporary path across an elevation boundary, and waiting indefinitely on the
UI thread. Replacing all installation with a copyable error message removed
those risks but produced an unacceptable first-run experience.

The accepted production path embeds the exact official ViGEmBus 1.22.0
installer in `HallJoy.exe`; end users perform no network download. Source and
build policy pin 6,278,576 bytes and SHA-256
`89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A`.
The build also requires a valid Nefarius Authenticode signer and executes a
non-elevating self-test against the linked resource.

At runtime HallJoy checks the resource before extraction, creates a CSPRNG-named
directory and `CREATE_NEW` file, flushes it, transitions from the writer through
a path-retaining bridge to a read-only lock, re-hashes the locked file, verifies
Authenticode through that same handle, and probes read/execute compatibility.
The file is re-hashed once more immediately before `runas`; write/delete sharing
remains denied until setup exits. Elevation occurs only after the user chooses
the explicit Install command. Waiting is message-pumped and limited to 20
minutes; timeout never force-kills the installer and schedules cleanup at
reboot. Successful setup retries ViGEm initialization in the same HallJoy
process; restart-required and failure states are explicit. A fixed official
release-page button remains a fallback. Runtime `latest` resolution and network
download primitives remain forbidden.

## D-050 - One release-readiness plan owns ordering and scope exclusion

Date: 2026-08-21

The existing family, pipeline, performance, architecture, evidence,
concurrency, pause and security audits remain the detailed technical sources.
`RELEASE_READINESS_MEGA_AUDIT_2026-08-21.md` is the single execution order and
definition of done that joins them. It must not duplicate or silently redefine
risk status; `RISK_REGISTER.md` remains the status ledger and
`VALIDATION_MATRIX.md` remains the evidence ledger.

An unverified production route can avoid blocking the general release only if
compile/catalog/binary evidence proves that the route is absent and public docs
make no support claim. A label such as `experimental` or `hardware pending`
does not waive a reachable P0/P1. UI and device ledgers with stale pending
labels must be reconciled against one exact final candidate instead of
repeating broad audits already accepted by the owner.

## D-051 - ViGEm output recovery owns resources by generation

Date: 2026-08-21

The physical IROK freeze proves that `alive`, a non-null stored `HANDLE` and a
fault flag are not a coherent output-worker health contract. Thread and wake
resources must be owned by one generation (or deliberately for process life),
with ready, progress, terminal completion and one-owner reap. Publishers must
pin that ownership before use; watchdog recovery must neutralize and either
rebuild within a hard bound or terminate an isolated boundary. A poisoned
generation may not make every future recovery impossible.

The working v1.4 EXE remains a required A/B runtime oracle, but not an
architectural rollback target: the available stable/current output-worker
source section is line-for-line identical. Root-cause work must therefore trace
handle provenance and forced interleavings rather than assume a SparkLink
protocol regression.

## D-052 - Architecture alternatives are evaluated before implementation

Date: 2026-08-21

Every release-roadmap implementation package follows
`ENGINEERING_WORKING_METHOD.md`. Before changing production code, the package
must define evidence, affected invariants and an old-bug oracle, then compare
at least a local root correction, a staged architectural migration and a clean
redesign/rewrite when those alternatives technically exist.

The chosen option is the one with the strongest correctness, latency,
liveness, resource ownership, data contract, testability, security and
migration properties. A smaller diff, retained legacy implementation or faster
short-term build is not preferred merely for being easier. Conversely, a
big-bang rewrite is not preferred merely for being new: staged replacement is
better when it reaches the same correct architecture with lower migration risk
and preserves characterization evidence.

Production work stops and returns to architecture comparison when a proposed
fix needs another special case, cannot name one owner, waits for a digital edge
to identify analog input, guesses protocol/layout/scale, hides stale state with
retry/timeout, or cannot be proven through the production path. Deeper redesign
is accepted when necessary; no artificial release deadline overrides this
contract.

## D-053 - ViGEm output moves to a supervised self-hosted process

Date: 2026-08-21

The local in-process repair, an in-process generation owner and a self-hosted
output process were compared under D-052. A process-lifetime wake event and RAII
would fix the known wake race, and an in-process generation object would make
health/ownership coherent, but neither can safely terminate a synchronous
`vigem_target_x360_update` that never returns. Killing such a thread would leave
live stack/library/request state and is forbidden.

The selected end state is the same `HallJoy.exe` in an internal ViGEm-output
child mode. Realtime publishes one newest complete four-pad XUSB snapshot to a
versioned bounded shared mapping and signals an event that remains alive across
child generations. One parent supervisor owns all process/thread/job handles,
deadlines and reap. The child exclusively owns ViGEm client, targets, updates,
reconnect and destruction. A stalled child can be terminated and reaped as a
whole process before exactly one replacement generation starts.

The implementation is staged behind production-linked fake-transport and
process-fault gates, but there may never be two simultaneous production ViGEm
owners. The legacy thread remains the only production owner until the atomic
routing switch, then is removed completely. Full contract, rejected options,
old-bug oracles and rollback boundary are in
`VIGEM_OUTPUT_PROCESS_ARCHITECTURE_2026-08-21.md`.

## D-054 - Prove the Windows IPC contract and generic supervisor separately

Date: 2026-08-22

The first claimed-slot channel package was reopened before building a child.
Portable `std::atomic_ref` stress alone is not the final Windows cross-process
contract. Shared control and slot transitions now use documented Win32
Interlocked operations on Windows, an in-flight publisher lease closes the
disable-to-publication race, and child-written health is explicitly telemetry;
restart count and authoritative lifecycle remain private to the parent.

A real Windows parent/child executable now rejects a wrong nonce and exchanges
100,000 complete four-pad snapshots through the production channel. This proves
the data-plane primitive, but it intentionally does not prove production
process ownership: its broad inheritance is test-only. The next package must
first implement a reusable fake-child supervisor with an explicit inherited
handle list, job containment, bounded startup/progress/stop, exact one-owner
reap, no generation overlap and zero survivors. ViGEm create/update/destroy is
added only after that lifecycle passes. This prevents driver/library behavior
from obscuring defects in the more fundamental process-generation owner.

## D-055 - Child containment is established before its first instruction

Date: 2026-08-22

F1 compares direct reuse of the analog-host supervisor, immediate extraction
and migration of analog-host, and a new common process-generation primitive.
Direct reuse is rejected because the existing loop is UAP-specific and starts
the child before job assignment. Immediate analog-host migration is rejected
for this package because it combines a qualified UAP route migration with an
unproved ViGEm foundation.

The selected implementation is a reusable owner and state machine proved first
with a fake child. Every generation gets a private kill-on-close job. The child
is created suspended with an explicit inherited-handle list, assigned to the
job, and resumed only after containment succeeds. One owner retains process and
job handles until confirmed reap; a reap failure blocks replacement. Generic
startup, progress, planned-stop and hard-reap deadlines execute outside
realtime. F1 contains no ViGEm calls and changes no production route.

The complete comparison, negative fixtures, rollback boundary and acceptance
gates are in `PROCESS_GENERATION_SUPERVISOR_DESIGN_2026-08-22.md`.

## D-056 - Exact HallJoy self-host uses a reusable output session and qualified stop

Date: 2026-08-22

F1.1 compares a separate helper executable, grafting output service behavior
onto the UAP analogue host, direct test-only wiring around the F1/R1.1
primitives, and an early same-image child plus a reusable output-process
session. A helper creates package/signature/version skew, the UAP graft mixes
unrelated service ownership, and direct test wiring would be replaced rather
than extended in F2.

The selected implementation dispatches `--halljoy-vigem-output-host` at the
first point in `wWinMain`. A narrow output session owns the process-lifetime
unnamed mapping/wake/stop/owner objects, uses the F1 supervisor as the sole
process/job owner, and adds output-specific protocol admission and completion
above that generic lifecycle. Simulator-only fake transport and fault modes
exercise this exact HallJoy image; `backend.cpp` remains on the legacy owner.

A generic `PlannedStop` result is not an output success. The matching child
generation must also acknowledge neutral application, target removal and
`completedStopGeneration`; otherwise the output adapter reports an incomplete
stop. The shared child-health plane gains an explicit generation and a
transactional telemetry sequence within previously reserved space, preserving
the 640-byte ABI while preventing stale or torn child observations from
admitting a replacement.

The complete alternatives, invariants, rollback boundary and exact-EXE O3/O4
gates are in
`VIGEM_OUTPUT_SELF_HOST_FAKE_TRANSPORT_DESIGN_2026-08-22.md`. No ViGEm call may
move into the child until this fake-transport composition passes.

## D-057 - Real ViGEm child uses one exhaustive RAII transport

Date: 2026-08-22

F2 compares inline SDK calls in the command host, transplanting the complete
legacy worker, immediately sharing a newly extracted transport with both legacy
and child routes, and a child-only RAII transport behind the proved F1.1
session. Inline calls mix trust parsing with lifecycle, the transplant imports
the obsolete wake/thread/poison model, and changing the legacy route now would
break the atomic rollback and failure-attribution boundary.

The selected child transport owns one ViGEm client and a fixed four-target
array. Its planned stop attempts neutral on every added target, then removal
and free on every target regardless of an earlier failure, and preserves the
first exact SDK error and phase. Only complete neutral plus complete removal
can acknowledge a clean stop. A default immutable API table calls the real SDK;
deterministic tests inject a fake table solely to exhaust every failure edge.

Pad count is immutable for a child generation. The parent writes it with a
matching configuration generation into reserved shared-control capacity before
launch; `Ready` acknowledges that configuration only after every target was
added. The ABI remains 640 bytes and the command-line/handle contract does not
grow. `backend.cpp` and production routing remain unchanged until the real-child
gate passes and a later package performs one atomic owner switch.

The complete comparison, invariants, gates and rollback boundary are in
`VIGEM_OUTPUT_REAL_CHILD_TRANSPORT_DESIGN_2026-08-22.md`.

## D-058 - F3 atomically replaces the in-process ViGEm owner

Date: 2026-08-22

F3 re-compared patching the legacy worker, introducing a temporary selectable
second runtime, and routing production directly through the proved process
session. Patching cannot bound a driver call blocked inside HallJoy. A dual
runtime introduces an overlap state and duplicated lifecycle authority. The
selected implementation removes the old owner in the same package that adds
one production `OutputRuntime`; no configuration can reactivate both.

The runtime owns one process-lifetime mapping/wake/stop session, one parent
supervisor thread and immutable desired-configuration revisions. Realtime has
only a bounded complete-snapshot publication entry. Every planned stop,
configuration replacement, child exit and progress timeout disables parent
publication before stopping/reaping the exact child, then drains already-
admitted publisher leases before mapping reuse or replacement. Recovery uses a
bounded backoff and cannot create a second owner from the UI watchdog.

Publication admission and child lifecycle identity are deliberately distinct.
Closing admission before stop must not revoke the already-contained child's
right to report neutral application, target removal and terminal state. The
immutable child generation/PID remains authoritative until reap; a successor
cannot exist during that interval.

Local process, real ViGEmBus, routed normal, child-exit and stalled-child gates
pass with strict non-overlap and zero survivor. This completes the F3 code
route, not the release risk. Exact local O5 is recorded by D-059; long active-
input and exact final-artifact IROK/DrunkDeer qualification remain mandatory.

## D-059 - ViGEm generation topology and stop/resource boundaries are distinct

Date: 2026-08-22

O5 compared pausing the whole analogue producer around every configuration,
accepting arbitrary snapshot sizes and relying on the child to reject them, and
binding publication shape to the exact active output generation. Pausing would
add lifecycle latency to input; child rejection turns a normal settings race
into a transport fault. The selected channel admits only the pad count committed
to the active generation. A producer that crosses a count change returns
immediately for retry; it never waits and never submits the wrong topology.

The same stress separated two stop conditions that F3 had initially combined.
Before child stop/force, the parent must close generation admission. After the
child is reaped, it must drain any producer already inside its bounded lease
before reusing or closing shared resources. Waiting for that harmless producer
inside the child's neutral/remove deadline can falsely poison a fully clean
stop; omitting the post-reap drain can close memory under a producer. The
selected two-boundary ordering satisfies both without adding work to realtime.

The exact same-image runtime gate publishes at least 100,000 changing snapshots
across 101 generations, 50 pad-topology changes and 10 disable/enable cycles,
then proves newest sequence/checkpoint equivalence and zero survivor. Twenty
independent repetitions passed with no unsafe generation or session rebuild.
This closes local O5 architecture qualification only; long-duration physical
IROK/DrunkDeer qualification remains release-blocking.

## D-060 - AnalogProviderV2 uses a versioned contract and staged migration

Date: 2026-08-22

R2 compares enlarging the current numeric arrays, replacing every input and
profile surface in one big-bang rewrite, introducing a strict contract beside
verified compatibility adapters, and using string/UUID identities in realtime.
Array growth preserves collisions and milli quantization. A big bang loses
failure attribution and hardware-safe rollback. Strings/UUIDs move allocation
and registry ambiguity into the hot path.

The selected staged route first introduces production-compiled POD types and a
portable validator. `KeyIdentityV1` separates USB usage-page identity, Soup/UAP
extended controls and HallJoy semantic inputs. `AnalogSnapshotV2` carries exact
source ownership, independent provider/sample/value/ownership generations,
freshness, explicit complete/truncated capacity, normalized float and optional
raw numerator/domain. A complete zero sample is an authoritative release;
changed-only lists are acceleration only.

No provider or profile is switched in the foundation package. UAP and native
read-only adapters come next and must pass old/new XUSB equivalence before a
production route changes. Legacy milli is permitted only as an explicitly
marked compatibility input with a removal gate; new providers may not publish
through it. The full comparison and rollback boundary are in
`ANALOG_PROVIDER_V2_FOUNDATION_DESIGN_2026-08-22.md`.

## D-061 - UAP must export a full V2 snapshot before HallJoy can adapt it

Date: 2026-08-22

R2-B2 found that the existing UAP dense table is not merely awkward to adapt:
both the plugin worker and isolated host discard historical codes above 255.
Consequently Fn, DrunkDeer OEM1/Menu/Fn2 and consumer media values cannot reach
an adapter built only in the parent process.

The compared alternatives were enlarging the ambiguous numeric table, adding
an extended side list, re-reading or digitally correlating special keys in the
parent, and exporting the versioned provider contract directly from UAP.
Numeric growth and a side list preserve two legacy domains; re-read duplicates
hardware work; digital correlation adds activation-depth latency and cannot
identify Fn reliably.

The selected design retains values by Soup key before compatibility projection
and exports one immutable per-device `AnalogProviderV2` snapshot. USB keyboard
and consumer keys receive their real usage pages; Fn/OEM controls use the UAP
namespace. The old 256-key ABI remains temporarily as a projection of the same
worker acquisition. Layout proof stays false unless a model-specific capability
map exists, so a provider-owned zero cell is not misrepresented as a printed
physical key.

R2-B2 adds only a read-only parallel path and comparison gates. `Backend_Tick`
must remain on the qualified route until deterministic old/new output
equivalence passes. Full alternatives and invariants are recorded in
`UAP_PROVIDER_V2_SNAPSHOT_DESIGN_2026-08-22.md`.

## D-062 - GravaStar V75 extends the proved 6x21 family by exact identity

Date: 2026-08-22

The V75 firmware audit compared five routes: copying the Aula backend under a
GravaStar name, sending V75 to legacy SparkLink, broad GravaStar brand probing,
hard-coding a recovered or digitally learned layout, and extending the existing
SparkPlayJoy 6x21 engine with a bounded exact USB/board registry. A copy would
duplicate transport and lifecycle ownership; legacy SparkLink is a different
wire protocol; broad probing enlarges the HID attack/failure surface; a guessed
or digital-event map either mislabels keys or adds activation-depth latency.

The selected design adds only the three firmware-proven identities to the same
registry used by 6x21 discovery and legacy pre-open exclusion. Exact identity
permits a read-only probe but never a claim. The candidate must still prove
usage/report shape, frame integrity, live scale, unique default map, two stable
complete 16-bit active maps, plausible travel and its exact board ID before the
interface path is claimed.

The protocol's `F001` function maps directly to the already established HallJoy
analogue Fn semantic `0x409`; USB Menu remains `0065`; all unknown vendor
functions fail closed. This is matrix publication from the keyboard's own live
map, not runtime learning and never digital correlation. The file/module name
is left unchanged in this functional package to avoid a broad cosmetic rename;
the user-facing descriptor identifies the SparkPlayJoy 6x21 Aula/GravaStar
family. Physical support remains pending one bounded diagnostic run.

The distributable diagnostic is also subject to a final-sink privacy gate,
independent of which producer emitted a line. Private roots and raw SetupAPI or
Raw Input device paths are redacted, and the one-file V75 bridge excludes
unrelated subsystem inventories. Acceptance runs the exact packaged EXE in an
isolated directory and requires a complete `session.end`, clean `WM_CLOSE`,
unchanged hash and no surviving process before the artifact may be sent.

The physical tester must also end with a machine-checkable answer. Offline
inference from a long log was rejected because a missing early branch could
again make the run useless; ad-hoc event expansion was rejected because it
cannot prove completeness. The diagnostic build therefore owns monotonic
progress and first-material-failure state and emits exactly one
`diagnostic.verdict` on every handled exit. It reports either a received matrix
stream or an explicit request to return the log, including the last identity,
open error and protocol stage needed to make the next correction.

This does not relax admission. Diagnostic enumeration may safely observe a
different `1CA2`/`1CA5` VID/PID or GravaStar SetupAPI brand so the log can reveal
a firmware identity change, but it never opens such an unknown interface. The
ordinary production build retains the exact-profile and full read-only proof
requirements and contains none of this continuous diagnostic telemetry.

## D-063 - Exact known 6x21 boards and unknown family candidates use different compatibility policies

Date: 2026-08-22

The first physical V75 trace proved every live protocol stage but exposed an
invalid assumption in D-062's implementation: the family firmware predicate
treated Aula model bytes `C0/01/00` as universal. The V75 sync is structurally
valid and reports its exact expected board `16052201`, but its corresponding
bytes are `00/04/00`. Requiring both identities made the firmware proof reject
the keyboard after already proving the stronger model-specific fact.

Four corrections were compared: accept any structured 6x21 sync, special-case
the three V75 bytes, remove the firmware predicate, or make the trust policy
explicit. The explicit policy is selected. A VID/PID already present in the
firmware-derived exact registry carries its exact expected nonzero board ID;
the sync must match that board and the structured descriptor envelope, followed
by the unchanged full framing/correlation/scale/map/stability/travel proof.
An unknown family candidate has no such independent model correlation and must
retain the narrower Aula `C0/01/00` signature. Thus physical firmware variation
is admitted only where an equal or stronger exact identity exists.

A semantic contradiction cannot become true merely because one second passed.
After deterministic firmware, precision, map or travel rejection, the worker
therefore waits for a real device-change notification or shutdown. Timed retry
remains only for transient transport and claim failures. This prevents needless
HID traffic, log growth and reconnect-like churn without hiding recoverable I/O
failure or learning anything from Windows digital input.

## D-064 - Critical child/job handles use kernel-enforced ownership

Date: 2026-08-22

The corrected physical V75 trace proved that moving ViGEm into a child removed
the legacy output-thread handle but did not remove the underlying close/ownership
class. After 135.6 seconds the supervisor's stored child-process handle returned
`ERROR_INVALID_HANDLE`; the analogue backend continued normally while virtual
output froze and the UI attempted 218 impossible session rebuilds.

Four responses were compared. Keeping raw private handles and relying on code
review cannot defend against an accidental numeric `CloseHandle`. Duplicating
the process handle leaves two closeable slots and still cannot make ownership a
kernel invariant. Reopening by PID after failure risks PID reuse, weaker access
and an unprovable gap. Immediately restarting all of HallJoy contains damage but
does not prevent the cause and loses user state.

The selected design gives the sole generation owner a non-copyable protected
handle type. Adoption sets `HANDLE_FLAG_PROTECT_FROM_CLOSE` on both child process
and Job Object; every accepted generation records that protection and the
process PID still match. Only the owner removes protection, and only when it is
ready to close after a confirmed terminal state. Windows therefore rejects an
accidental foreign close before the handle-table slot can be freed or rebound.

Failure containment remains independent of prevention. If ownership, reap or
session teardown is nevertheless unprovable, no replacement generation may
overlap it. The UI emits one complete recovery-blocked event and requires a
HallJoy restart instead of retrying forever. This fail-closed path is not the
normal recovery mechanism; it is the truthful terminal fallback for a violated
lifecycle invariant.

## D-065 - UAP V2 capacity describes the captured generation, not caller memory

Date: 2026-08-22

The first R2-B2 implementation exposed a second truncation boundary. The UAP
registry is dynamic, but the lock-safe pinned-owner transport currently retains
at most eight device owners. Merely passing a larger destination buffer cannot
make the ninth owner part of the captured generation. Reporting that caller
buffer as snapshot capacity would therefore permit a truncated internal view to
look complete or internally invalid.

Three responses were compared: silently keep the first eight, dynamically
allocate while holding the registry, or preserve the registry demand and report
the effective immutable capture window. Silent truncation repeats the original
bug. Allocation inside this ABI/lifecycle package enlarges exception and lock
behavior before negotiated IPC exists. The selected staged response records the
source registry's `required_count`, bounds copied devices/samples by the pinned
generation, and reports that effective capacity even when the caller supplied a
larger buffer. Validation then requires the `Truncated` flag exactly when demand
exceeds captured capacity.

This is an honest fail-closed bridge, not the final variable-capacity IPC. The
read-only parent capture may observe and diagnose it, but production output
remains on the legacy route. A later negotiated data-plane package removes the
fixed window; before that switch, a same-generation shadow must prove the real
configured XUSB reports equal field-for-field.

## D-066 - UAP compatibility and V2 views come from one pinned acquisition

Date: 2026-08-22

R2-B2a left the legacy dense and V2 views as two child export calls. Retrying
until counters match was rejected because it cannot prove one pinned owner set
and can hide an ABA publication. Deriving legacy only from V2 and switching the
route now was rejected because it removes the qualified compatibility fallback
before configured output equivalence. Keeping separate calls was rejected as an
invalid oracle.

The selected private export pins device owners once, locks each device once and
builds both views from that captured generation. It returns the namespaced V2
samples and the ordinary-HID dense projection together. The exact DLL gate
independently reconstructs all 256 dense cells from V2 for every captured device
and requires equality, valid generations and finite normalized values.

IPC V12 labels a provider snapshot coherent only after the child validates both
views and their projection. The parent refuses a V2 capture without that label.
A dual-contract failure invalidates only the new read-only path and increments
telemetry; the child may use the old export solely to preserve the still-
qualified production route. Digital input is never used for correlation,
learning or fallback.

The exact production image must prove a non-empty parent capture through the
real isolated child. This closes acquisition/transport coherence only.
`Backend_Tick` stays on the legacy route until the next package compares actual
configured bindings, curves and every XUSB report field in read-only shadow.

## D-067 - Configured XUSB comparison uses one explicit-state production builder

Date: 2026-08-22

Calling the old report builder twice was rejected because it mutated global
Snappy Joystick/Last Key Priority and mouse state; the shadow call could affect
the game or compare against a different state transition. Comparing only raw
values was rejected because it omits bindings, curves, thresholds and report
conversion. Copying the builder was rejected because two implementations can
drift before a route switch.

The selected design extracts one production-linked builder. It accepts an
immutable per-tick binding/settings snapshot, already filtered input values and
an already acquired mouse contribution. Stateful conflict behavior is explicit
caller-owned data. Qualified and V2 shadow routes use distinct state objects but
the same component and configuration value.

The qualified route is migrated first and independently compiled/smoked before
V2 shadow integration. The shadow report must never reach ViGEm. Missing,
truncated or incoherent V2 evidence skips/fails the comparison and cannot alter
qualified output. Full design and remaining gate are in
`CONFIGURED_XUSB_SHADOW_DESIGN_2026-08-22.md`.

## D-068 - Mapping emits a neutral controller frame; XUSB is an output adapter

Date: 2026-08-22

Keeping `XUSB_REPORT` as HallJoy's canonical mapped state was rejected because
Xbox masks and naming would leak into every future output, including planned
DualShock 4 support. Immediately introducing a universal graph for touch,
motion, arbitrary axes and controller-specific extensions was also rejected:
there is no current consumer or compatibility proof for that larger contract,
and it would broaden the live Provider V2 migration unnecessarily.

The selected boundary is a versioned internal `VirtualControllerFrameV1` for
the standard controls HallJoy already produces: semantic buttons, two triggers
and four sticks. The configured mapping builder returns that neutral value.
`xusb_output_adapter` alone owns Xbox masks and conversion to the existing
`XUSB_REPORT`; ViGEm publication remains unchanged and XUSB-only.

V1 is not an IPC or persistence ABI and makes no DS4 support claim. A future
DS4 adapter can map the shared standard controls without changing input mapping;
touch, motion or output-specific extensions require an explicit later frame
version. Exact adapter regressions must prove all current XUSB fields before and
after this boundary. Provider V2 shadowing and any production route switch stay
separate later gates.

## D-069 - One parent transaction owns every UAP value used by a tick

Date: 2026-08-22

The production path could previously call the legacy per-key UAP reader for
each binding. Every call was individually coherent, but the child could publish
between calls, allowing one controller frame to mix adjacent generations.
Reading dense and V2 separately then comparing counters was rejected because it
retains an ABA window. Capturing V2 beside unchanged per-key qualified reads was
rejected because it would create a false equivalence oracle. Switching output
directly to V2 was rejected because configured equivalence is not yet proven.

The selected API copies publication metadata, aggregate dense values,
per-device dense values and optional Provider V2 beneath one shared sequence.
The qualified UAP route consumes the captured dense compatibility plane for the
whole tick. V2 remains read-only and unconsumed by mapping. A stable dense
capture remains valid when V2 is absent; if the complete parent transaction is
unavailable, the old analogue read is the availability fallback.

The child and parent use one production validator for per-device dense/V2
projection, and the parent additionally verifies the aggregate max merge and
active counts. No digital event participates in correlation, learning or this
fallback. Live shadow mapping, mismatch telemetry and route switching remain
separate gates.

## D-070 - Provider V2 is qualified by a live full-frame shadow, never by raw-only comparison

Date: 2026-08-23

Four implementations were reconsidered after the one-parent capture landed.
Switching production directly to V2 was rejected because it removes the dense
rollback oracle before real configured equality is observed. Comparing only raw
or curved keys was rejected because it omits bindings, thresholds, SOCD/Last Key
Priority, mouse merge and output fields. Copying the report builder was rejected
because two implementations could drift. Learning V2 identities from later
Windows keydown was rejected absolutely: it adds activation-depth latency and
cannot represent Fn/Menu keys that may not emit an ordinary digital event.

The selected route projects explicit V2 identities from the same immutable
parent tick, merges the exact cached native ownership, applies one shared curve,
configuration and mouse sample, and invokes the same neutral-frame builder with
separate state. Owned zero is distinct from absence. Consumer/semantic
namespaces are not aliased. A missing/incoherent V2 capture, digital fallback or
curve-generation mutation makes the sample ineligible and resynchronizes shadow
history from qualified history.

All seven controller fields are compared. Bounded in-memory counters retain
matches, mismatches, per-field totals and skip causes without continuous
production logging. Only the qualified dense result crosses the XUSB adapter
and ViGEm boundary. Implementation/local gates do not authorize route promotion:
representative physical zero-mismatch evidence and a separate explicit switch
decision remain required.

## D-071 - Provider V2 promotion evidence is one fail-closed ordinary-lifecycle report

Date: 2026-08-23

Continuous production logging was rejected because it adds permanent I/O and
does not define a trustworthy session verdict. UI-only counters were rejected
because a screenshot has no durable session identity or finalization. A hidden
partial-backend command was rejected because it would not exercise the normal
HallJoy lifecycle and configured game path. Direct promotion from local shadow
counters was rejected because idle ticks, repeated sample generations and a
backend reset could otherwise look successful.

The selected implementation is an opt-in ordinary HallJoy build. At startup it
atomically replaces any old evidence with a flushed `INCOMPLETE` report. Only a
normal application shutdown may finalize it. A crash or poisoned immediate exit
therefore cannot leave a current-looking `PASS`. Process evidence is monotonic,
requires exactly one backend generation, 1,000 eligible ticks/reports, 100
unique UAP sample generations, bounded unavailability, no digital fallback/
curve mutation and zero field mismatch. Elapsed duration is recorded but is not
a verdict gate. The original 60-second threshold was removed: an arbitrary
59.9/60.0-second cliff cannot prove either equality or stability. Stability is
qualified separately by purpose-built soak, reconnect and fault tests, and no
finite runtime is treated as proof against every later failure.

Coverage is not inferred from final controller output alone: mouse or native
input could activate that output without exercising UAP. Activation requires
both Provider V2 raw ownership with at least half travel and a non-neutral mapped
shadow field. Release is latched after activation and is accepted only when both
the Provider V2 raw bindings and mapped field are neutral; partial return cannot
count as release. The 28 pad/field slots remain independent. Windows digital
input is absent from this path.
Mismatch yields `FAIL`; missing proof yields `INCOMPLETE`; only complete evidence
yields `PASS`.

The dedicated image writes one bounded transactional text file and performs no
file I/O from `Backend_Tick`. The ordinary release contains no report marker.
Both images continue publishing only dense compatibility output; a physical
qualification `PASS` permits a separate promotion decision but never switches
the route automatically.

Two finalized physical reports from the exact same schema-1 qualification EXE
may be aggregated because each contains independent monotonic frame/generation
history and fixed pad/field masks; this is offline evidence composition, not
runtime learning. Their union covers every configured field (`0x7E`) through
activation and release across 43,879 zero-mismatch frames and 10,777 unique
generations. R2-B2g physical configured-output equality is therefore complete;
route promotion remains a separate decision.

## D-072 - Provider V2 promotion waits for a genuinely negotiated producer

Date: 2026-08-23

Physical configured-output equality closes mapping correctness, but it does not
make the current eight-owner UAP capture complete. Four next steps were compared:
immediate frame selection, enlarging the fixed constant, one big-bang producer/
IPC/route rewrite, and a staged negotiated producer followed by a split data
plane. Immediate selection preserves a known capacity defect. A larger constant
only moves it. The big-bang destination is sound but combines too many ownership
and rollback boundaries without an independently qualified producer.

The selected R2-B2h package makes the existing zero-capacity demand plus exact-
capacity request contract real. Reusable storage grows before registry/device
locks; one complete owner generation is pinned, device locks are released in
reverse order and every shared owner reference is cleared on all exits. Topology
movement retries only within a small bound and otherwise fails closed. Caller
capacity controls copied output but can no longer restrict which registry owners
the producer is capable of capturing.

This package deliberately does not promote output and does not claim the current
monolithic eight-slot IPC is fixed. R2-B2i must negotiate a separately owned,
parent-read-only Provider V2 data plane and prove its generation/resize/fault
lifecycle. R2-B2j may then select Provider V2 and remove the dense runtime
fallback. Full analysis and gates are in
`UAP_PROVIDER_V2_NEGOTIATED_CAPACITY_DESIGN_2026-08-23.md`.

## D-073 - The firmware testbed is layered, hash-pinned and outside production HallJoy

Date: 2026-08-23

Building one ND75-only scripted protocol mock was rejected because it would
duplicate assumptions and would not extend to another firmware. Starting with a
complete M484 board emulator was rejected because clocks, NVIC, USB, DMA,
GPIO/ADC, flash and scheduler modelling would delay the HallJoy correctness
roadmap before proving that those components answer a current question. Loading
an arbitrary updater and guessing its behavior was rejected because a green test
could then be weaker than the firmware evidence it claims to represent.

The selected design separates scenario generation, synthetic Hall inputs,
firmware execution, family/peripheral models, virtual HID transport and HallJoy
result oracles. Every firmware profile pins the exact bytes and declares one
evidence level: replay, protocol model, selected original machine code, or full
reset-to-main emulation. Unknown instructions, memory, peripherals, descriptors,
identity or protocol behavior fail closed. Digital Windows key events never
select, learn or delay analogue identity.

Windows VHF is a thin bounded transport only; firmware execution and test logic
remain in user mode. HallJoy is tested unmodified through its ordinary HID path,
and the testbed cannot relax production device admission. The architecture grows
incrementally from the existing M484 instruction-level work to an exact ND75
vertical slice, multi-firmware family packs and, only when justified, full-board
emulation without discarding earlier profiles or scenarios.

This program is mandatory long-term engineering work but is not a blocker for
the next stable HallJoy release. Virtual evidence cannot replace final physical
hardware qualification or establish unmodelled sensor, USB timing or board
electrical behavior. The authoritative stages and gates are in
`FIRMWARE_VIRTUAL_HID_TESTBED_ROADMAP_2026-08-23.md`.

## D-074 - Provider V2 uses a separate parent-read-only double-buffered data plane

Date: 2026-08-23

R2-B2h proved that the producer can discover and capture the complete owner set,
but the isolated-host `SharedState` still embeds eight dense devices and a fixed
V2 payload. Increasing those constants was rejected because it only moves the
capacity cliff. Replacing the whole control/dense/V2 mapping with one variable
mapping was rejected because control and payload would still share all-access
ownership and every topology resize would disturb unrelated lifecycle state. A
pipe/RPC frame broker was rejected for the realtime payload because it adds
another copy, queue/backpressure policy and wake boundary to every generation.

The selected design keeps the bounded legacy control/dense mapping only through
the migration and adds a separate variable-size Provider V2 section. The parent
creates and owns the section, maps its payload `FILE_MAP_READ`, and passes the
handle to the isolated child, which owns the writable view. Two fixed-capacity
slots are laid out from overflow-checked negotiated device/sample counts. The
child writes an inactive slot, validates the complete snapshot, then publishes a
generation-bound commit. The parent copies only a matching stable slot into
pre-sized caller-owned storage and rechecks both slot and dense transaction
tokens. No parent write is required on the realtime read path.

Initial zero-capacity discovery and later topology growth use the existing small
control plane only to report required counts. The old child is stopped and reaped
before the parent replaces the section and launches a new plane generation.
Unknown sizes, overflow, defensive-ceiling excess, partial snapshots, generation
mismatch or an unstable slot fail closed. Old payload bytes remain unreachable
after invalidation because no current control commit names them; clearing a
million-sample mapping in realtime is neither required nor allowed.

Implementation is staged: B2i-a proves the portable layout, arithmetic and slot
contract; B2i-b adds inherited-handle creation, read-only parent mapping,
negotiation and restart/resize fault gates; B2i-c moves live shadow capture to the
new plane and removes fixed Provider V2 payload arrays from `SharedState` and the
fixed parent snapshot. Dense production output remains the rollback route for
all B2i stages. Only the later explicit R2-B2j decision may select Provider V2
and remove dense runtime fallback. Full invariants and gates are in
`UAP_PROVIDER_V2_SPLIT_DATA_PLANE_DESIGN_2026-08-23.md`.

## D-075 - Provider V2 becomes the local engineering route after B2i-c, before release promotion

Date: 2026-08-23

Keeping dense output selected until the release-promotion decision was
reconsidered after B2i-a. It protects users, but it would also leave the new live
transport and output route largely unexercised by ordinary local use. Selecting
the current shadow immediately was rejected: today it still obtains Provider V2
through the old fixed `SharedState`, so that would test the semantic builder but
not the split mapping, rights, capacity or restart lifecycle that B2i is meant to
replace. Waiting until B2j for the first actual output was also rejected because
release qualification would then discover ordinary-use defects too late.

After every B2i-c live-plane gate passes, the normal local engineering build
selects Provider V2 output for the entire process lifetime. It never silently
substitutes dense input when the V2 generation is missing or invalid: the
affected Provider V2 source fails closed to neutral and exposes a bounded status
reason. Independently valid native/mouse inputs remain governed by the common
builder rather than being discarded merely because UAP is unavailable.

The legacy dense route remains implemented, compiled and directly regression-
tested, but is selected only by an explicit immutable build property for a
clearly named emergency/user-test artifact. There is no UI switch, persisted
setting or automatic runtime fallback that could hide V2 failures. Both routes
must compile in ordinary checks, and exact route-identity tests must prove which
one a produced artifact selected. The selector cannot change after startup, so
stateful SOCD/LKP history is never transferred between routes at runtime.

R2-B2j therefore becomes the decision to promote the already-used V2 route to a
user release and retire dense runtime compatibility, not the first time V2 sends
controller output. Until B2i-c is complete, ordinary and user builds continue to
select dense output because the Windows plane is not yet the live shadow route.

## D-076 - Physical UAP tests require process isolation; product HID policy is unchanged

Date: 2026-08-23

Running a second UAP/HallJoy instance beside an active HallJoy was previously
allowed by several test paths. A named per-family transaction mutex prevents two
writes from overlapping, but it does not make independently opened HID input
queues transaction-private. Keychron custom full-matrix `A9 31` returns four
packets carrying the same command/subcommand and no visible request token or
part index. `discardStaleReports()` only establishes a pending overlapped read;
it does not prove a quiet, fully drained transaction boundary. A second owner,
especially one terminated during the response, can therefore leave another
open handle able to assemble packets from different requests. This is the
strongest code/protocol explanation for the observed stable down/right matrix
shift; it is not promoted to captured on-wire proof without a trace.

Three broader reactions were rejected. Restarting/relearning from Windows
digital keys would introduce the forbidden activation-depth delay and hide a
protocol error. Making every product HID open exclusive could break vendor
configuration applications and needs separate compatibility evidence. Adding a
global HallJoy singleton would change ordinary multi-instance product behavior
to solve a test-runner problem.

The selected immediate rule is fail-closed physical-test isolation. The leaf
private-UAP ABI runtime check, official build runtime gate, exact dual-capture
runner and ordinary production smoke enumerate HallJoy processes before any UAP
hardware open and refuse to run if one exists; enumeration failure is also a
failure. Static/native compilation and synthetic process tests remain usable in
parallel. No running user process is terminated. Future firmware/protocol work
may add a response transaction/part identity or prove an exclusive-open policy,
but neither is silently inferred by the current checkpoint.

## D-077 - Bridge-owned preallocated broker is the Provider V2 realtime boundary

Date: 2026-08-23

B2i-c must expose variable-capacity Provider V2 data to controller construction
without allocating, waiting or retaining a mutable mapping pointer in
`Backend_Tick`. Three smaller designs were rejected. A realtime SRW/vector copy
can block or allocate during hotplug. A direct mapping view can be overwritten
when the child reuses its inactive slot. Defensive-maximum fixed arrays waste a
large permanent working set and merely replace one compile-time cliff with
another.

The selected boundary is a three-slot parent snapshot broker written only by the
existing snapshot bridge. Every slot owns vectors sized outside realtime before
the corresponding child generation becomes Ready. The bridge copies and fully
validates one mapping commit, rechecks its transaction, then atomically publishes
the completed broker slot. Realtime acquires a bounded nonblocking read lease;
the writer never touches the published slot or a leased slot. If all alternate
slots are busy, an intermediate generation is dropped rather than delaying the
consumer.

Resize first revokes broker publication, drains its writer and readers outside
realtime, then replaces mapping and broker capacity before child launch. A drain
or allocation failure fails V2 closed and follows the existing retained-resource
poison policy. The child must also replace its separate fixed and dynamic V2
queries with one dynamic dual capture after negotiation, placing the plane token
inside the same legacy dense seqlock. This makes coherence structural rather
than inferred from adjacent call timing.

## D-078 - UAP hotplug restarts only the isolated child on a real device-change event

Date: 2026-09-05

The private UAP configurations deliberately define `UAP_DISABLE_HOTPLUG=1`.
The disabled implementation is a repeated broad `discover_devices(false)` scan;
restoring it would reintroduce unsolicited HID enumeration, racing device I/O and
unbounded work independently of a real topology change. It must not be enabled.

Three alternatives were considered for reconnect after `WM_DEVICECHANGE`:

1. Enable the old periodic UAP scan. Rejected: it is polling, not an event
   boundary, and restores the reason the flag was introduced.
2. Add a mutable plugin control command which re-enumerates from inside a live
   child. Rejected for this package: it extends the C ABI/control surface and
   would make device discovery race the plugin's polling/unload ownership.
3. Coalesce actual Windows device-change notifications into one request consumed
   by the existing parent supervisor. Selected: the supervisor invalidates the
   shared snapshot, terminates only its already job-contained child, waits for
   confirmed reap, then launches a fresh child through the normal bounded
   generation and negotiated-plane path.

The request is a level-triggered atomic flag. It is accepted only while the
client is live, is never observed by realtime, and has no timer or retry loop.
The request is consumed by the supervisor rather than the UI `WM_DEVICECHANGE`
handler; a storm coalesces to one restart per active child. During replacement
the existing `Status_Restarting` neutral/fail-closed path remains authoritative.
A reaped child reruns the plugin's ordinary startup enumeration, so an unplugged
device disappears and a replugged device becomes visible without restarting
HallJoy. Reap, job-assignment, plane-retire, or identity failures retain their
existing poison/block semantics and never turn into an unbounded hotplug retry.

## D-079 — Audited profile persistence and runtime commit (2026-09-05)

Problem/evidence: independent audit F-01..F-08; production-linked loader probe
accepts empty input, wraps 65543 to 7 and truncates 1033 assignments to 539.
Invariants: malformed input never mutates runtime; settings and bindings have
one durable commit; a controller tick cannot observe a partially applied profile.

Option A: validate booleans and rollback two files. Rejected: crash between
renames still mixes generations and does not solve concurrent tick reads.
Option B: staged migration to one authoritative settings INI containing bindings,
retaining legacy pair reads until first successful save; prepare all load data
before a short runtime commit guard. Selected. File replacement remains the
existing flushed/validated atomic adapter. Realtime only tries a read lease;
it never waits on parsing, disk I/O or a writer. The UI drains existing tick
readers before applying prepared memory state. A contended tick publishes no
partial report. This preserves current getters while closing the whole-profile
transaction boundary; all ordinary saves must include both halves.
Option C: rewrite all settings/bindings/curve getters around immutable global
snapshots and replace the file format. Rejected for this package: it changes
every individual editor action and curve cache without improving disk atomicity
over B; the staged read lease gives the required tick isolation with less
compatibility risk. It remains a possible later simplification.

Ownership/security: only UI commits profiles; parse budgets apply before
allocation; no new device/network access. Prepare happens before runtime guard,
including key-settings allocations. Readers cannot retain state across a commit.
Legacy pair files are preserved as migration evidence, but once a bundle marker
exists its bindings are authoritative; no silent fallback to an older sibling.
Older executables require restoring the pre-change data backup when rolling back.
Wrong/incomplete bundles fail closed. Window/layout/overlay remain global.
Tests: old-bug oracles, malformed/wrong-kind/overflow/full-domain load tests,
bundle single-replace failure stages, concurrent tick/commit test, unified
source gate, MSVC build and isolated synthetic smoke. Physical claims unchanged.
Rollback: hash-verified .analysis/backups/profile_audit_fixes_20260905_175549.
Update owning audit, risk register, validation matrix, roadmap, worklog/handoff.

D-079 validation follow-up: the migration script failed neutral/opposing oracles
while the real UAP source continued publishing W near 1.0. This is source
contamination, not a file-loader failure (all scripted phases ran). Alternatives:
ask the owner to disconnect hardware; suppress neutral assertions; or explicitly
isolate synthetic ownership in the simulator. Select the last, behind both
HALLJOY_ANALOG_SIMULATOR and an opt-in test argument. WASD native synthetic values,
including owned zero on disconnect, feed the unchanged curve/builder/output
pipeline; physical UAP/native values do not compete for those four test keys.
Normal builds and the default mixed-input simulator route remain unchanged.
No assertion is weakened. Migration tests opt into the isolated source.

D-079 completion note: startup autosave is disabled until profile preparation
succeeds, including main's unconditional final shutdown. Production-linked
file-only tests verify rejected-startup file hashes and exit before backend
creation. A clean portable-marker file-only startup also passes. Full portable
controller simulation remains pending: user requested no gameplay input while
playing CS. The opt-in synthetic isolation mode still emits controller input
and must not be used during this restriction.

## D-080 - Preserve mouse IPC v1 and add an opt-in coherent capture boundary

Date: 2026-09-06

The named mouse bridge is a public ABI consumed by an external ASI, so resizing
it or repurposing fields would require a coordinated release and would not
repair old deployed helpers. The UI is its only state writer. We retain the
40-byte v1 schema and make its existing publisher heartbeat an odd/even commit
marker around the four related policy fields. A new reader can obtain a stable
image by accepting only equal even values on both sides of its scalar reads;
an old reader retains the same monotonic heartbeat it already understood.

Replacing the mapping with a new named v2 object was rejected because it leaves
old and new ASI sessions with split ownership during migration. A lock across
the public mapping was rejected because the external reader cannot be forced
to participate and producer latency must remain bounded. This marker is only a
capture protocol; it does not assert that an unmodified ASI already uses it.

## D-081 - Bound raw-input allocation before data retrieval

Date: 2026-09-06

`WM_INPUT` receives an untrusted size before the application has a typed
mouse/keyboard payload. Allocating the thread-local receive vector immediately
from that value makes malformed or unexpectedly large input an unbounded UI
allocation point. The selected correction rejects a size above the Windows
raw-input envelope cap before resize, then retains the existing second-call
typed-size validation. The cap is deliberately 64 KiB: it is comfortably above
the registered mouse/keyboard payloads while preventing a pathological request
from retaining arbitrary memory in the UI thread.

Rejecting all packets larger than `sizeof(RAWINPUT)` was rejected because raw
input packet sizes differ by architecture/type and the existing typed-size
validator intentionally supports valid compact keyboard packets. Moving raw
input parsing to a worker was rejected because it changes message ordering,
adds lifetime/queue ownership and does not improve the simple pre-allocation
boundary. The hook/pass-through and backend-admission gates remain unchanged.

## D-082 - Bound cached GDI glyph surfaces across DPI/style changes

Date: 2026-09-06

Keyboard-preview and Remap glyph caches own DC/bitmap pairs keyed by rendered
size and style. Their old maps freed an entry on explicit panel cleanup but had
no insertion bound, so repeated DPI/size/style changes could retain GDI objects
for the life of a UI session. The selected correction keeps the fast cache but
caps each independently owned cache at 256 entries, freeing one complete,
deselected DC/bitmap pair before inserting a new key at capacity.

Disabling the cache was rejected because repeated glyph rasterisation is a
measured paint-path cost. A global cache manager was rejected because these two
surfaces have independent lifecycle and render semantics. Eviction is safe
because callers consume the returned cache entry before requesting another;
the implementation never stores a cache pointer across a later lookup.

## D-083 - Classify trace failure evidence without changing producer timing

Date: 2026-09-06

The stability analyzer already detects trace errors, sequence gaps and worker
leaks, but reported only raw event names. Add deterministic post-capture
categories for stale input, stalled producer, output failure, storage failure
and incomplete shutdown. Classification uses the existing component/event names
and does not add trace writes, polling or filesystem work to realtime code.

Changing every producer to emit a second generic error event was rejected: it
would obscure the first native failure and increase diagnostic traffic. A
best-effort pattern matcher that suppresses unknown errors was rejected: unknown
critical events must remain visible as raw failures. Categories are additive,
deduplicated evidence labels, not a replacement for the original event.

## D-084 - Freeze AULA HERO84 HE outside ordinary builds until a real tester returns

Date: 2026-09-06

The owner has confirmed that the only available AULA HERO84 HE user stopped
responding, so the experimental implementation has no real-device evidence.
Keep its code and its exact admission checks, but remove it from every ordinary
catalog and ordinary image. The code may be enabled only by the named
`HallJoyAulaHero84HeExperimental` build property, which produces a separately
named trace-capable test executable for a future consenting owner.

Leaving the descriptor in an ordinary image was rejected: an untested keyboard
could be claimed automatically merely because it was connected. Deleting the
implementation was rejected because it would discard audited protocol work and
make a future evidence-led test needlessly start over. This is a release-scope
freeze, not a statement that the device is unsupported forever; physical input,
release, hotplug and log evidence remain required before a new scope decision.

## D-085 - Preserve the tested Addressed Analog session command

Date: 2026-09-06

The owner confirmed that Addressed Analog is physically tested and working on
real keyboards. The single `09 98 02` command at session start is established
protocol behavior: it disables the legacy last-key diagnostic mode before the
normal addressed `09 94 02` polling loop. Retain it unchanged.

Removing it merely because its purpose was not repeated next to the call site
would silently alter a working route and was rejected. The command stays behind
the existing exact HID fingerprint and successful checksum/correlation proof;
it is not generalized to devices that fail admission. Future code review must
treat the historical protocol contract and this owner confirmation as evidence,
not as permission to send additional commands.
