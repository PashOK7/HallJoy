# HallJoy — RM-00 current-state index

Recorded: 2026-09-06. Status: **RM-00 documentation baseline complete; this is
not a build, runtime, hardware, or release qualification.**

## Exact release artifact

The ordinary delivery artifact is `build/release/HallJoy.exe`.

| Field | Value |
|---|---|
| Current SHA-256 | `A4D8B0C52D826B7ED723819DD441A7DDD05501D84AEAF647541F0F6DDCEE6E37` |
| Historical 2026-09-05 SHA-256 | `44CFC0793A9CFD2A74B2B677AD8704708B1E16C6CD764EA8852C14962ECBE012` |
| Interpretation | Different artifact; the historical evidence remains tied to its own bytes. The current hash matches `build/release/SHA256SUMS.txt` and the 2026-09-06 structure handoff. |
| Version source | `HALLJOY_VERSION_STRING_FULL = 1.4.1.0` in `H/version.h` |
| Release manifest | `build/release/dependency-lock.json`, SHA-256 `580237C73B1DB6E3B260BB3521336E4A59A6F0EC9E081BF254139CEC40EE3AC9` |
| Toolchain contract | Visual Studio 2022, C++20, `v143`, Windows SDK `10.0`; locally observed MSBuild `17.14.14.31908` |

The intentional negative oracle is the SHA comparison above: an EXE with the
historical hash cannot inherit evidence recorded for the current release hash.

## Current source boundary

The ordinary build contract is `MAD68ProRNative`, `Release`, `x64`; its build
defines include `HALLJOY_MAD68PR_NATIVE;HALLJOY_PRODUCTION`. `tools/build.ps1`
has SHA-256 `B371AD2FEBE8726B2187F7293678B88F042E83B94C6AD9F06C120B856CF64E44`.

After the release artifact was created, the working tree received an unbuilt,
opt-in ROG Azoth 96 HE diagnostic scaffold. It is source-only, excluded unless
`HallJoyRogAzoth96HeDiagnostic=true`, and therefore is **not represented by the
release EXE hash**. Its detailed boundary is
[ROG test diagnostic plan](../research/ROG_AZOTH_96_HE_M901_TEST_DIAGNOSTIC_PLAN_2026-09-06.md).

| Current source input | SHA-256 |
|---|---|
| `H/HallJoy.vcxproj` | `A5AB69DC350E63A3B9A42C620EEC4AD54A11DA9A87FCF5CD2C29EF5E3F4D0650` |
| `H/native_analog_backends.def` | `ED4BA556AE65B27262989777BC6838BC677845D84F5CAD4E0E9742101F365E03` |
| `H/native_analog_routing.h` | `939A9B8C2EBC6F65A98935692AB433AFD3D593B2FDA638EAF949CFA59610771C` |
| `H/rog_azoth96he_diagnostic_backend.cpp` | `E59F0678CBFA44258889F8DC8BE5812C2AC072D5534A6B20C4FEFDF86C34408F` |
| `H/rog_azoth96he_diagnostic_backend.h` | `2C342237F60B0A7523D07AF2B2F3539369622A1EF5B20260F731714CAEA91F49` |

Dependency identity is pinned by the release lock: Sun
`83c195bd61314bdbfdccc161653dbb652e3b6678`, Soup
`b02796b0b20276277c8a4b4d3759643eeab43ff7`, ViGEmBus `1.22.0`, and
`ViGEmClient.lib` SHA-256
`800239E478698154F544AD81C4580A3E6A2A48358D1FF67D970C996B382640B4`.

## Historical P0/P1 reconciliation

This assigns every still-open integration blocker from the current
[risk register](RISK_REGISTER.md) to an execution owner. "In progress" means
there is partial evidence only; it is not `Verified`. P1-001 through P1-008
are retained historical records: their individual current statuses remain in
the risk register and their exact-artifact revalidation belongs to RM-34.

| Historical ID | Current code/evidence status | Old oracle retained | Owner |
|---|---|---|---|
| P0-001 | In progress: V2 export/capture exists; public surfaces pending | extended-key identity/collision | RM-07, RM-09, RM-12 |
| P0-002 | Open | UAP malformed-report/hotplug lifecycle | RM-13, RM-30 |
| P0-003 | Open; automatic letter learning is intentional, ambiguity and stale fallback remain separate | cross-report Sayo association; stale-depth behavior | RM-03 |
| P0-004 | Open | W669 lost-live/lost-release | RM-05, RM-30 |
| P0-005 | Implemented locally; hardware pending | close/reuse output wake handle | RM-15, RM-24, RM-34 |
| P0-006 | Reopened by physical evidence; correction local PASS, hardware pending | protected owner / blocked watchdog recovery | RM-15, RM-24, RM-34 |
| P1-001..003 | Verified historical records | original extraction/version oracles | RM-34 revalidation only |
| P1-004..007 | Verified historical records | original reconnect/time arithmetic oracles | RM-34 revalidation only |
| P1-008 | Partial; physical MAD68 retest pending | shutdown containment | RM-34, RM-35 |
| P1-009 | Open | Hex80 partial matrix freshness | RM-05, RM-30 |
| P1-010 | Open | Addressed sibling-layout admission | RM-30 |
| P1-011 | In progress: identity foundation, adapters pending | native 16-bit identity | RM-07, RM-09 |
| P1-012 | Open | Spark layout/scale/duplicate semantics | RM-04, RM-30 |
| P1-013 | Open | protocol proof without layout proof | RM-30 |
| P1-014 | Open | mixed fallback source arbitration | RM-11 |
| P1-015 | Open | profile atomicity/load failure | RM-06, RM-18, RM-19 |
| P1-016 | In progress: identity model, UI/profile surfaces pending | no-wrap end-to-end binding | RM-07, RM-20 |
| P1-017 | In progress: V2 precision foundation, route pending | adjacent sub-milli source values | RM-10 |
| P1-018..019 | Open | UAP snapshot cost / rate policy | RM-22, RM-33 |
| P1-020 | In progress: variable section; live fixed arrays remain | >8-device capacity | RM-09, RM-12 |
| P1-021..022 | In progress: IPC and common contract foundation | coherent read-only generation / cross-provider contract | RM-09, RM-12 |
| P1-023 | Open | native hang/fault containment | RM-14, RM-15 |
| P1-024..027 | Open | anti-fix, wiring, missing-gate, sanitizer health controls | RM-31, RM-32 |
| P1-028..032 | Open | snapshot/lifecycle/realtime/hang/shutdown interleavings | RM-15, RM-24 |
| P1-033 | Open | neutralize/release/resume and competing owner | RM-16, RM-17 |
| P1-034 | Open | arbitrary DLL / TOCTOU owner | RM-23 |
| P1-035 | Open | huge/malformed external layout | RM-20 |
| P1-036 | Open | diagnostic privacy/artifact scan | RM-28 |
| P1-037 | Open | unsigned/post-sign mutation | RM-32, RM-34 |
| P1-038 | Open; physical proof | unrelated Sayo stop poisoning shutdown | RM-15 |
| P1-039 | Open | permanently failing Spark row remains nonzero | RM-04, RM-05 |

## Controlled backup and next step

Before this index and its navigation updates, the three modified navigation
files were copied and hash-verified in
`.local/backups/rm00_baseline_20260906_133258/manifest.json`. No build,
application launch, HID interaction, keyboard injection, or gamepad output was
performed for RM-00.

RM-01 is next. It must classify all `tools/run_*.ps1` before any runner is
executed; the present user session prohibits output-capable input tests.
