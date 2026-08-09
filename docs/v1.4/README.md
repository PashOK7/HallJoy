# HallJoy v1.4

This directory is the authoritative source for v1.4 scope, status, decisions,
risks, and validation. Documents under `docs/stability` preserve the imported
archive history and evidence, but they do not define the current product
version or release status.

## Current status

- Product version: `v1.4.0.0`
- Working branch: `v1.4-integration`
- Release qualification: passed
- UI gate: unified scroll architecture passed automated 6/6-tab stress; final
  scroll behavior and visual result accepted by the owner
- Aula physical gate: passed, including real analogue matrices, 10+ rollover,
  measured rate and three disconnect/reconnect recoveries
- Production diagnostics: no continuous telemetry or log writer; crash-only
  `HallJoyCrash.txt`
- Public release notes: [`RELEASE_NOTES_v1.4.md`](../../RELEASE_NOTES_v1.4.md)
- GitHub publication: [`v1.4.0`](https://github.com/PashOK7/HallJoy/releases/tag/v1.4.0)
  published as the latest stable release
- Final artifact: `build/release/HallJoy.exe`, 2,187,776 bytes, SHA-256
  `C03CB7A19DB73921D69904A7ABDAB954D54F3D5CE42899ADD9C24012D93D0402`

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
backend originally accepted only `1CA2:1902`, `FFA0:0001`, the 65-byte Windows
envelope and firmware `App V1.1.6 / Feb 4 2026`; V14-12V supersedes that
admission rule with a bounded brand-scoped 6x21 family proof while preserving
the exact physically verified profile. Parser, oracle,
end-to-end, poisoned-session and ambiguous-device tests pass GCC, MSVC
`/W4 /WX` and Clang ASan+UBSan. The official build, private ABI, overlay and
Irok regression pass with 65,379/65,379 SparkLink queries and unchanged user
state. At the V14-12A package stage this was firmware-proven support rather than
physical Aula validation. The later V14-12U.5/U.6 hardware runs below close
input, rate, rollover and reconnect for the stated firmware. Other identities
remain outside the physical claim even when accepted as protocol-compatible.
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
- [PRE_RELEASE_UI_AUDIT_2026-08-02.md](PRE_RELEASE_UI_AUDIT_2026-08-02.md) -
  independent code-and-log UI audit, unified scrolling architecture, and the
  owner's recorded final visual acceptance.

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

The clean post-handoff UI audit produced a 2,228,224-byte `HallJoy.exe`,
SHA-256 `6BCBA47D86448E7D262250AEAF7EAFD89FD448CA8A917B1C514711F31FCB6CC3`.
Static UI guards, instrumented Irok runtime, 3/3 production lifecycle cycles
and the 15-second simulator pass. UI-01 through UI-05 are implemented but remain
`In progress` until the owner completes visual acceptance; no earlier
`Verified` status is accepted as visual evidence.

The V14-12S root-corrected artifact is the 2,230,784-byte
`build/output/HallJoy.exe`, SHA-256
`1B5671F36EDE9CD2CB1153A2D02729387D0974D25FFB031D457A4FDC7AB523D1`.
It restores event-complete keyboard preview updates on every tab, moves
Tester gamepad motion to UI cadence, frame-coalesces the three problematic
scroll paths, atomically moves Configuration combos and defaults new overlay
smoothing to 15%. Full build, code-driven UI stress logs, simulator and final
3/3 lifecycle qualification pass. Visual status remains pending owner review.

After the owner accepted the visuals but reported low scroll FPS, V14-12S.1
removed `WM_TIMER` starvation from Remap/Configuration/Global scrolling and
introduced adaptive 8–16 ms input-driven frame deadlines. The stress rate rose
from about 29 to about 70 commits/s with zero active erase. The latest artifact
is 2,231,808 bytes, SHA-256
`F9A32FEB956E7ED38CE7CB75BBE8254D64B296259821B9922ECA8EFF9D5B040C`;
full build and final 3/3 qualification pass. Scroll visual recheck is pending.

V14-12S.1 was subsequently rejected by the owner because higher FPS did not
prevent elements from disappearing. V14-12T supersedes that approach with one
viewport architecture for all six keyboard pages: a shared controller,
content-coordinate hit testing, retained composition and one common scrollbar.
The final 2,232,832-byte `build/output/HallJoy.exe`, SHA-256
`851C84A63AB6A1532C21E4FE477A16F9D9248BF4AB76090E3DD9AEAA969C2509`,
passes the full build, 6/6 production scroll stress and 3/3 lifecycle
qualification with stable GUI resources and unchanged user state. This is
automated evidence only; visual acceptance remains pending owner review.

V14-12T.1 fixes the retained-control visual regressions without reverting the
accepted unified scroll. KSP, Spark and Global faces now reuse the canonical
PremiumCombo painter; vector power/save icons replace encoding-sensitive text,
the added inner focus outline is removed, and popup controllers hide on every
close path. The 2,233,344-byte artifact SHA-256 is
`76CB02D76131E72B78652969C3673F3A2E586BCB843FD4ED57B78F51F6FD4677`.
Full build, 6/6 scroll stress, three popup lifecycle cases and 3/3 qualification
pass. Visual status remains pending owner review.

V14-12T.2 restores interaction semantics after the retained migration: bounded
Remap hit domains make the gamepad power action clickable, a shared binding
transaction exposes global-profile dirty/save state, and Input Overlay
direction/depth/font are true PremiumCombos rather than cycling buttons. The
2,232,832-byte artifact SHA-256 is
`8BE26D58294AD7E02D38C0970E4A8F6BD321A976638DC0E809A0425020D714CA`.
Full build, 6/6 scroll stress and 6/6 popup lifecycle cases pass. Owner
interaction review remains pending.

V14-12T.3 fixes mouse-wheel routing for long PremiumCombo popups, including the
Input Overlay font list. Final artifact SHA-256:
`45AEDB2FA1952843B004FED3F80EAD7F18DC8357FAD5EF2D64BBE237A6AC221B`.
Full build and popup wheel/lifecycle stress pass.

V14-12T.4 corrects that first wheel implementation: wheel input now changes
only the viewport of an overflowing popup. Five fitting lists remain at
`scrollTop=0`; the font list scrolls while selection remains unchanged. Final
artifact SHA-256:
`ED0082DFDC24F8A4137B1559D1B43058186C19ED4B9142E7F9BEE107F65EB00D`.

V14-12U adds a physical Aula diagnostic package after a tester trace proved
that SparkLink was repeatedly opening and probing the Aula interface. Spark now
rejects the dedicated Aula identity before HID open. The separate
`build/aula-diagnostic/HallJoy.exe` traces every read-only Aula proof stage into
one automatic `HallJoy.log` beside itself while keeping strict
claim/publication gates. No helper scripts are delivered. See
`AULA_PHYSICAL_DIAGNOSTIC_2026-08-02.md`; this diagnostic EXE is not a release
candidate and physical Aula acceptance remains pending its returned trace.

The first two returned single-file logs refined that diagnosis. A transient
`ERROR_SHARING_VIOLATION` disappeared in the next run: exclusive open succeeded
62/62 times. The actual root was HallJoy's inferred 54-byte sync gate rejecting
the physical keyboard's stable, checksum-valid 60-byte response. V14-12U.1
allows that envelope only in the aggressive diagnostic build, continues later
read-only stages, and still blocks claim/publication through the firmware
mismatch mask. The replacement diagnostic EXE SHA-256 is
`4AC9B51E9EE1824E6050400FF09F94B763084EEEE7A816A6D8A4290E938D54CA`.

`HallJoy (3).log` then completed all 17 read-only proof transactions three
times on the physical keyboard. Precision `10/10/3400`, the 61-position map,
two stable active-map generations and both travel envelopes match. The last
mismatch was our incorrect string interpretation of a binary build descriptor.
V14-12U.2 replaces the inferred 54-byte oracle with the exact repeated 60-byte
physical descriptor in production. Runtime non-zero travel and reconnect are
the remaining physical acceptance gates.

The claim-capable single-file diagnostic for that gate is 2,245,120 bytes,
SHA-256 `23CEC8D7EF2479B353EABE7AAB8857CB04BDF3CC6BD0FE3D1FC89DAA1C02BB14`.

`HallJoy (4).log` and `HallJoy (5).log` close the physical runtime-input gate.
Both runs pass strict proof with zero mismatch, claim the dedicated Aula route,
connect and sustain polling for about 58 and 17.5 minutes; the tester confirms
that analogue key travel appeared in HallJoy. The raw trace cap retained only
the first zero-valued startup frames, so no non-zero byte capture is claimed.
The longer run proves disconnect clearing and bounded rediscovery, but the
keyboard was not reattached before exit. Return-after-disconnect reconnect is
the remaining Aula physical gate.

V14-12U.4 corrects the diagnostic blind spot before asking for one last hardware
run. The single-file schema-v2 EXE records exact 5-second polling frequency,
transaction latency buckets, 0/1/2-4/5-9/10+ active-key distributions,
release-to-zero transitions, event snapshots and final per-HID travel maxima.
It also rate-limits Spark skip evidence and treats shutdown cancellation as
normal lifecycle. Sanitizers are 6/6 and all native/production gates pass; the
production image contains no high-detail markers. Diagnostic SHA-256:
`F2727D0A7E901DF89D95B27B1D0CD86D7D2B9655B59EB4998F62594FCAF158C5`.

`HallJoy (7).log` validates the new measurements on hardware: 21,027 successful
matrices at 343.973 Hz, no failed update, all paired travel transactions below
4 ms, maximum 22 simultaneous keys, 2,654 frames at 10+, eight full releases
and 40-HID coverage up to 3,400 um. Shutdown is clean.

`HallJoy (8).log` closes the remaining physical reconnect gate. It records three
successful reconnects to the retained physical identity, each with a complete
strict proof. The first recovered session delivered 1,008 matrices at 341.463
Hz, including 329 non-zero frames over 12 HIDs, proving actual analogue recovery
rather than enumeration alone. Transient write/read failures coincide with the
tester's additional USB cycles; every cycle recovered and final shutdown joined
all workers with exit code 0. Aula physical release blockers are closed; the next
artifact can be the crash-only production release build.

The final production profile is now built at `build/release/HallJoy.exe`.
Ordinary debug and stability-trace calls are eliminated at their call sites, so
their arguments are not evaluated and no logger thread or normal log file exists
at runtime. The production image contains none of the stability, diagnostic or
Aula matrix telemetry markers. A lightweight unhandled-exception filter performs
no normal-operation I/O and creates `HallJoyCrash.txt` only after a process crash;
the silent native A9 emergency watchdog remains enabled. The isolated hidden
portable smoke started, accepted PID-owned `WM_CLOSE`, exited 0 and created zero
normal/crash logs. The Aula backend now admits brand-scoped compatible 6x21
profiles only after a complete structural read-only proof; the physical claim
remains limited to the tested WIN 60 HE MAX identity. Final artifact: 2,164,224
bytes, SHA-256
`2833DA24AF9D086A084B045FCEEA78F08883536FD96F48E1EFEEC938B652E1BB`.
