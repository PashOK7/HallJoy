# Release-readiness audit — 2026-09-05

## Verdict

**Do not publish yet.** The local tree is a release candidate in active
development, not a qualifying release artifact. This verdict is based on the
current source and executable outputs, rather than on the historical 2026-08-21
audit alone.

The next release can be made publishable without reopening every historical
research task, but only after the gates below are closed against one exact,
versioned `HallJoy.exe`.

## Scope and evidence used

This audit inspected the working tree at
`W:\github\_halljoy_latest_20260817\HallJoy-main` on 2026-09-05.

- It is not a Git worktree. There is no local commit or tag that identifies
  the source snapshot.
- GitHub `main` is `d478300daf531e49c77275fccb91ad0faff9f565` (the v1.4.1
  release commit, 2026-08-11).
- After normalising CRLF/LF only, 90 tracked text files differ from that
  GitHub revision. The local tree also has 60 new top-level production source
  units, including Provider V2, the child-process ViGEm/XUSB output path, and
  diagnostic/experimental backends.
- The two available ordinary executables are both version `1.4.1.0`:
  the root `HallJoy.exe` is dated 2026-08-22 and
  `src/HallJoyProject/HallJoy/x64/Release/HallJoy.exe` is dated 2026-08-23.
  Their SHA-256 values are, respectively,
  `E71EDB6BA9734F6019F5C22EAF48ED6DD299EA91D1F00E4D564705D66312C3D5`
  and
  `0D665C524C89782F0F881A619BEA0AE1C33A253CFA16CB59AF43D3B1F3A32AB1`.
- The ordinary project and its inputs changed after those executables. In
  particular `app.cpp`, `HallJoy.vcxproj`,
  `aula_hero84he_backend.cpp`, and `titan68_turbo_diagnostic_backend.cpp`
  changed on 2026-09-05. Therefore neither existing EXE is evidence for the
  current source.

Historical files such as `RELEASE_READINESS_MEGA_AUDIT_2026-08-21.md` retain
their original findings. They are not silently treated as either closed or
still reproducible here: current code and a new exact artifact must decide
that.

## Current reproducible test result

On 2026-09-05, the following read-only command was run with Python bytecode
writes disabled:

```text
python tools/run_native_backend_checks.py --require-compiler
```

It stopped in the static-audit phase with this failure:

```text
native backend architecture audit: FAIL: catalog getter has no definition:
AulaHero84He_GetNativeBackendDescriptor
```

The implementation does exist in `aula_hero84he_backend.cpp`. The audit uses a
regular expression that accepts `NativeAnalogBackendDescriptor&` but rejects
the valid project style `NativeAnalogBackendDescriptor &`. Thus this is a
**false-red test defect**, not evidence that the descriptor is absent.

Nevertheless it is a release blocker: `tools/build.ps1` invokes this unified
runner before compiling, so an official build cannot pass until the audit is
corrected and the whole runner is rerun. Running all 79 `*audit.py` scripts
independently produced exactly this one failure; this does not substitute for a
passing unified runner or for a native build.

## Latest local evidence after pre-release corrections

On 2026-09-05, after the Provider V2 evidence alignment and simulator-runner
corrections, the complete required source gate was run again:

```text
python tools/run_native_backend_checks.py --require-compiler
```

It exited successfully: every static audit and every portable C++ test passed.
The migration suite also passed from a fresh simulator image: nested profiles,
Unicode/decomposed filename fixtures, replay, all five atomic failure stages,
and portable mode. The suite now uses a short temporary root so its own atomic
temporary-file suffix cannot manufacture a `MAX_PATH` failure, and it is
non-interactive for intentional startup failures.

This evidence proves source contracts and simulator behavior only. It does not
replace R-03's clean ordinary build, exact-artifact lifecycle qualification, or
physical device gates.

## Exact-artifact evidence — 2026-09-05

The backed-up ordinary output folders were rebuilt through `BUILD.cmd` while no
`HallJoy.exe` process was running. The resulting user-facing files are
byte-identical:

```text
build/output/HallJoy.exe  SHA-256 1DD724B6E2CFC47CD5CBA152AE59B9FC2926EE1A70EA2F65D04047B7F57C5961
build/release/HallJoy.exe SHA-256 1DD724B6E2CFC47CD5CBA152AE59B9FC2926EE1A70EA2F65D04047B7F57C5961
```

For that exact ordinary artifact, the following gates passed:

- UAP Provider V2 dual-capture smoke, bound to the hash above;
- production overlay/startup/shutdown smoke, including 2,000 concurrent HTTP
  fuzz cases and no continuous diagnostic or crash report;
- 25 isolated normal start/stop cycles (74–175 ms shutdown), with user state
  unchanged (110 files).

The separately compiled Provider V2 qualification executable also passed its
exact-artifact smoke. Its three-second no-device run correctly reported
`verdict=INCOMPLETE`, not a fabricated analogue `PASS`; it verified the
authoritative V2 route and that legacy dense data is not submitted to ViGEm.
The qualification executable is evidence for its diagnostic contract, not the
ordinary release binary.

## Required release gates

| ID | Gate | Current state | Required closure |
|---|---|---|---|
| R-01 | Unified source gate | **PASS locally, 2026-09-05** | The complete `run_native_backend_checks.py --require-compiler` gate passed again after all pre-release corrections, including Provider V2 evidence and storage-runner repairs. Re-run it from the clean release build tree in R-03. |
| R-02 | Release scope for AULA Hero84 HE | **Decision required** | The normal catalog currently includes the experimental Hero84 HE backend, while `SUPPORTED_HARDWARE.md` has no row or physical evidence for it. Before shipping, either compile it out of the ordinary release and keep it diagnostic-only, or document it explicitly as experimental and obtain the required physical safety/input evidence. Do not let an unlisted automatic HID claimant ship by accident. |
| R-02A | UAP unplug/reconnect | **PASS, owner physical test 2026-09-05** | Exact production EXE `08D6A79F41CD8B72E0B2BCBC5A418694E2712A87907827EDAFBC9A5DDD63582C` recovered analogue input after physical unplug/replug without restarting HallJoy. `UAP_DISABLE_HOTPLUG=1` remains in force: the rejected periodic broad scan was not restored. Broader UAP regression remains part of R-05. |
| R-02B | Provider V2 route promotion | **Automated qualification PASS; physical gates pending** | The controller now selects the same-transaction V2 snapshot as its authoritative source. Legacy dense data is a reverse comparison shadow and compatibility fallback only when the V2 plane is unavailable. The dedicated exact-artifact qualification build and smoke passed, including the no-false-pass idle oracle. Fresh representative UAP hardware equality/topology evidence remains required. |
| R-03 | Clean ordinary build | **PASS locally, 2026-09-05** | `BUILD.cmd` rebuilt the backed-up output folders successfully. `build/output/HallJoy.exe` and the clean `build/release/HallJoy.exe` are byte-identical at SHA-256 `1DD724B6E2CFC47CD5CBA152AE59B9FC2926EE1A70EA2F65D04047B7F57C5961`. |
| R-04 | Exact-artifact runtime qualification | **Partial PASS** | The exact ordinary EXE passed Provider V2 dual-capture smoke, overlay/startup/shutdown smoke, and 25/25 normal start/stop cycles with preserved user state. The one-hour long-soak gate is still unrun, so this row is deliberately not marked complete. |
| R-05 | Representative physical regression | **Not run for the new output architecture** | The child-process ViGEm/XUSB route changed after v1.4.1. Exercise the exact fresh EXE with at least one proven native keyboard and one UAP keyboard, including analogue motion, release-to-zero, virtual-controller continuity, unplug/reconnect, and clean shutdown. Claims for a newly enabled family require that family's own physical evidence. |
| R-06 | Version, notes, and support boundary | **Not started** | Choose the new version, update `version.h`, create release notes for that version, and make `SUPPORTED_HARDWARE.md` match what the ordinary package actually contains. Diagnostic-only MCHOSE Ace68 and Titan68 Turbo builds, plus IROK ND75's gated owner build, must not be described as normal support without their evidence. |
| R-07 | Git provenance and publication preparation | **Not started** | Put this source tree into a proper Git repository/branch, record the clean source commit, review the diff against `d478300`, tag the exact release commit, and publish only the package hash produced by R-03/R-04. |

## What is deliberately not a gate for this ordinary release

The MCHOSE Ace68 and MADLIONS Titan68 Turbo deliverables are diagnostic-only;
there is no received hardware trace that promotes either to normal analogue
support. IROK ND75 is behind its experimental build property. They should stay
out of the ordinary release scope unless their separate hardware gates are
closed.

The 2026-08-21 P0/P1 findings remain valuable regression oracles, especially
for output continuity and shutdown. They must be exercised by R-04/R-05, but
this audit does not make a false claim that every historical item is still an
unfixed source defect after the post-August architecture changes.

## Order of work

1. Make the R-02 shipping-scope decision before a normal build can silently
   include Hero84 HE.
2. Complete R-02B Provider V2 production-source promotion and its exact-artifact
   equality/restart/topology gates.
3. Close R-02A with exact-artifact unplug/replug evidence; this must precede
   the UAP portion of R-05.
4. Create a clean source commit/branch and version boundary (R-06/R-07).
5. Back up the tree, run the clean official build (R-03), and record the
   generated EXE hash.
6. Qualify that same EXE with automated start/stop and soak evidence (R-04),
   then physical native and UAP regression evidence (R-05).
7. Recheck package contents, notes, supported-hardware claims, and hashes;
   only then create the GitHub tag and release.

No GitHub mutation was performed while creating this audit.
