# HallJoy v1.4 decision log

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

Gamepad Tester remains intentionally scrollbar-free because its card grid is
computed from the available viewport height rather than from an unbounded
content height. If it later gains fixed-height content that can overflow, it
must adopt the common scrolling contract instead of clipping controls.

Destructive actions must communicate danger through their resting fill as well
as border and text. Hover-only or border-only danger styling is insufficient
because the idle state is the primary state users see.
