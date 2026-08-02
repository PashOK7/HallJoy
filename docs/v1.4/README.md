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
- Completed subpackage: `V14-10B` analog-host inherited-handle IPC,
  Verified locally
- Completed subpackage: `V14-10C` bounded overlay HTTP framing and telemetry
  parsing, Verified locally
- Completed subpackage: `V14-10D` bounded overlay concurrency, strict origin
  policy and cooperative client shutdown, Verified locally
- Completed package: `V14-10` IPC and overlay security/correctness
- Verified by automated gates: `V14-11A` deadline pacing for UAP poll
  transports; no physical UAP performance claim is made
- Verified by automated gates: `V14-11B` stable identity for identical UAP
  devices across enumeration reorder and reconnect generations
- Verified by automated gates: `V14-11C` bounded device-owner capture; snapshot
  and telemetry exports no longer wait on per-device locks under the registry
- Verified by automated gates: `V14-11D` exact HID interface-path ownership;
  sibling interfaces with the same VID/PID are no longer hidden as one device
- Completed package: `V14-11` UAP pacing, identity, snapshot contention and
  exact interface ownership
- Implemented subpackage: `V14-12A` Aula WIN 60 HE MAX native support,
  firmware-proven and implementation-tested; physical Aula hardware remains
  unvalidated
- Implemented subpackage: `V14-12B/S06` Addressed overlapped-I/O ownership and
  bounded stop; deterministic timeout containment and available Irok regression
  pass, while physical Addressed hardware remains unvalidated
- Completed pre-qualification package: `V14-12G/S20` current build/test docs,
  catalog-driven Addressed audit, x64-only project configurations and MSVC
  Warning Level 4 with zero unexpected production warnings
- Verified package: `V14-12L/S21` saturating runtime arithmetic, recoverable
  overlay/ViGEm worker containment and rare clipboard/UAP ownership cleanup;
  full compiler, sanitizer, locked-plugin build and Irok qualification pass
- Verified package: `V14-12N/S21` deterministic shutdown containment for every
  production keyboard route; 9/9 process scenarios pass with zero survivors
- Verified package: `V14-12O/S21` production input-to-overlay load profiling
  and retained browser rendering; physical Irok, full HallJoy/browser process
  trees and the 1 ms/8 ms refresh comparison pass without state mutation
- Verified package: `V14-12P` recoverable factory reset; styled Global-settings
  action, atomic request, exact-target backup, reverse rollback injection,
  official build and physical Irok lifecycle regression pass
- Reopened after manual review: `V14-12Q` automated gates passed, but physical
  Irok review found input-driven UI flicker, stale Configuration telemetry,
  fake poll comboboxes, duplicated diagnostics and rejected reset-button states;
  see `RELEASE_UI_AUDIT_HANDOFF_2026-08-02_RU.md`
- Next work: `V14-12/S21` release qualification and hardware matrix. The Aula
  physical result may arrive asynchronously but remains mandatory before release
- GitHub publication: not started
- Release status: not release-ready
- Current UI gate: rejected; a clean paint/control/diagnostics audit is required

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
V14-10B removes the analog-host named-object attack surface: its mapping,
events and owner-process capability are unnamed handles inherited only by the
created child through an explicit handle list. The v10 shared schema binds the
owner PID and CSPRNG launch token, and both sides validate child identity.
V14-10C replaces recv-bound request handling with an incremental HTTP/1.0/1.1
frame parser. It consumes exact `Content-Length` bodies, supports fragmented
and pipelined requests, rejects transfer coding, and enforces 8 KiB header,
4 KiB body and 2 KiB target limits. Client metrics use exact-key
`from_chars` conversion with a one-billion upper bound, so malformed,
duplicate and overflowing telemetry is rejected before counters change.
V14-10D delegates accepted clients to a fixed 16-worker ownership table, so
slow or fragmented clients cannot serialize the live state stream. Stop wakes
and joins every client before WSA cleanup; a simulator gate held eight partial
requests through application shutdown. Each server generation also creates a
128-bit CSPRNG session cookie, protects state and telemetry with that cookie,
accepts only the exact `http://127.0.0.1:<port>` browser origin and emits no
wildcard CORS policy. An already open overlay page automatically refreshes a
cookie from the new generation after an overlay restart. Simulator and
production socket gates, timeout
containment, the full build and the Irok route/balanced-shutdown smoke pass.
V14-11A replaces the zero-delay UAP vendor-request loop with a 1 ms
start-to-start deadline. Fast poll transports are capped at 1 kHz, a slow USB
transaction receives no extra delay, and transient Madlions failures back off
from 2 to 64 ms. Report-stream devices remain on their blocking path. The
portable rate model, static audit, rebuilt ABI identity/unload gate, full build
and production Irok regression pass. The Irok is a native SparkLink device, so
V14-11A is accepted by deterministic production-code tests, GCC/MSVC, and
Clang ASan+UBSan because no UAP poll keyboard is available; this verifies the
scheduler but makes no physical USB/latency claim. V14-11B now hashes the
normalized Soup HID interface path and descriptor, so identical devices keep
their own IDs regardless of enumeration order. The exact production function
passes all 40,320 permutations of eight devices, 100,000 reconnect generations,
250,000 synthetic paths without a collision, 1,024 pathless fallbacks and
persisted-ID golden vectors under the same three compiler/toolchain gates.
V14-11C replaces exclusive registry ownership with pinned `shared_ptr` owners.
Both snapshot exports copy at most eight owners under `devices_mtx`, release it,
and only then lock each device and copy telemetry or 256 values. Deterministic
blocked-reader removal, 100,000 lifetime cycles, 50,000 coherent snapshot
reads, three toolchains, sanitizers, the ABI gate, full build and Irok
regression pass. This proves lock/lifetime behavior, not physical UAP latency.
V14-11D replaces coarse VID/PID arbitration with first-proof-wins ownership of
the exact normalized SetupAPI interface path. All five native families reject a
foreign claim before opening HID, Soup calls the same shared fingerprint helper
before `CreateFileW`, and its redundant post-open guard checks the actual path.
The portable gate separates same-VID/PID siblings through 10,000 reorder/
reconnect generations and 300,000 synthetic paths; GCC, MSVC `/W4 /WX`, Clang
ASan+UBSan, ABI, the official build and native Irok regression pass. Under
D-022 this verifies the code-level ownership contract, not physical multi-UAP
hardware or an impossible mathematical proof against every 64-bit collision.
V14-12A independently reproduced the supplied Aula WIN 60 HE MAX protocol
evidence against exact npm sources and the supplied firmware. The native
backend accepts only `1CA2:1902`, `FFA0:0001`, the 65-byte Windows envelope and
firmware `App V1.1.6 / Feb 4 2026`; it runs a 17-transaction read-only proof on
one exclusive session before claiming the exact interface path. Parser, oracle,
end-to-end, poisoned-session and ambiguous-device tests pass GCC, MSVC
`/W4 /WX` and Clang ASan+UBSan. The official build, private ABI, overlay and
Irok regression pass with 65,379/65,379 SparkLink queries and unchanged user
state. This is firmware-proven support, not physical Aula validation; input,
hotplug, multiple devices and other firmware versions remain open.
V14-12G closes the remaining S20 build/documentation risks. Unsupported
Win32/x86 configurations were removed, both supported x64 configurations use
Warning Level 4 with a narrow documented legacy baseline, and the official
build rejects every warning except the external ViGEm PDB diagnostic. Current
project guides name the real unified runner and `HallJoy.exe`; the old Addressed
validator now checks the central catalog and bounded lifecycle, while the v3.9
validation record is explicitly historical. Automated gates and a normal Irok
3-cycle regression pass. This completes pre-qualification work, not S21.

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

## Latest qualification status

V14-12I completed 1000/1000 ordinary production start/graceful-stop cycles.
The one-hour V14-12K soak then found and isolated a SparkLink timestamp-underflow
restart; the corrected artifact passed its deterministic/full-build/targeted
runtime gates. V14-12M additionally contains a reported MAD68 HE/UAP shutdown
stall at both boundaries: permanent child unload stalls are disposed after the
bounded child deadline, and a 12-second process watchdog covers every explicit
shutdown stage through final logging. The exact production artifact passed 5/5
Irok cycles with no survivor. V14-12N adds an independently trace-verified
permanent-stop scenario for each of the six native routes, the shared UAP/Soup
child route, the global watchdog and a normal control; all 9/9 pass with zero
survivors. This is code/simulator containment evidence, not physical protocol
validation. Physical MAD68 HE retest, manual device-matrix items and external
Aula hardware remain release-blocking.

V14-12O profiles the production chain from physical Irok/SparkLink HID through
realtime, ViGEm publication, overlay JSON/send and a real headless Chrome page.
On the final artifact, repeated 1 ms real-overlay runs used 0.809-0.929% of the
12-logical-processor machine for the complete HallJoy tree and 5.451-6.120% for
Chrome. A final exact 8 ms comparison used 0.721% and 3.747% respectively.
The retained canvas now stops redrawing after visual convergence, sprite/label
caches are bounded at 512/256 entries with constant-time LRU eviction, and new
profiles default to 8 ms while the existing 1 ms opt-in remains supported.
Physical route accounting reached 217,628/217,628 in the 8 ms comparison and
277,055/277,055 in the final full-stage run; both preserved user state and left
zero HallJoy processes. The exact 2,212,352-byte `HallJoy.exe` SHA-256 is
`06CF73B59827E957DDF9644AC2557C601F5602FD454AC7B025FB6533C041A462`.
This is physical Irok and shared-downstream evidence, not physical proof for
unavailable Aula, MAD68 HE/UAP or other keyboard protocols.

The latest post-V14-12Q production artifact is the 2,228,224-byte
`HallJoy.exe`, SHA-256
`6DFC616422D89783A846F7EE8CAEA64AD7D951591092576DEFB717320543DF96`.
Its one-cycle physical Irok regression passed with unchanged user state and a
193 ms graceful shutdown. This UI correction does not change the outstanding
physical MAD68 HE/UAP or Aula release gates.
