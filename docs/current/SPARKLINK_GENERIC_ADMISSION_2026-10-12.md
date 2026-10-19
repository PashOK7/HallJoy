# Generic SparkLink V2 admission (2026-10-12)

Owner rule (permanent, also in `AGENTS.md`): if a protocol explicitly identifies itself and
HallJoy can be certain that a keyboard speaking an implemented protocol is connected, the
keyboard works and shows the yellow notice "Keyboard not in the list: hardware testing
incomplete" (protocol match, not tested). If certainty is not possible, admission stays an
exact list of Supported or implemented-untested models.

Context: [SPARKLINK_V2_PLATFORM_SURVEY_2026-10-12.md](SPARKLINK_V2_PLATFORM_SURVEY_2026-10-12.md)
(552 official identities, 26 named, IPI/AULA/Everglide and many other brands on one platform).

> Release status: implemented and software-tested, but switched off by `kGenericAdmission=false`
> (`sparklink_model_profiles.h`) in the 1.6.9.1 hotfix, which must contain fixes only. With the switch
> off, `ProbeIdentity` equals the exact model list and `GenericOnly` is always false (behavior of
> 1.6.9). Flip the switch in the next feature release after a decision.
>
> 2026-10-19 OWNER decision: enabled (`kGenericAdmission=true`) for the 1.6.10 feature release.

## Why this is "certain" for SparkLink V2

| Condition | How it is met |
| --- | --- |
| Safe identity gate before any command | The VID:PID must be in the SparkLink PlayJoy vendor's official keyboard list for usage page `0xFFB0`, usage 1 (551 identities, `sparklink_official_identities.h`, generated from `docs/research/sparklink-v2-official-identities-20261012.json`). These are exactly the devices the official drivers already send `01 02` to. A bare vendor page is never enough: `01 02` is a reset on SteelSeries Apex Pro (VID 1038, not in the list). |
| Protocol reply proof | `01 02` device-info reply with status `0x01` (existing proof), then layout rows `03 01`. |
| Analog proof | One live travel read `04 03 01 00` must succeed (three attempts) before the keyboard is published; a non-analog board of the same platform never becomes "connected" and never claims its keys. |
| No identity shared with another protocol | VID `0x3151` (RongYuan / Attack Shark, different protocol on shared IDs) is excluded; a test asserts no overlap with the AULA 6x21 identities. |

## Behavior

- `ExactModel` (MG75 Max `0529`, Supported SU75 Pro `3002`, the experimental named identities)
  is unchanged: same admission, same notices, same layouts.
- `GenericOnly` = official-list identity that is not an exact model: key positions and layout
  rows come from the keyboard, manual/default layout, no layout token, yellow generic notice
  (`GenericProtocol`, telemetry `genericProtocol`) shown only after the first successful
  travel read. Not in the Sheet-synchronized model catalog (same as BY/Logitech generic).
- Failure memory: a generic device that fails the protocol probe, the travel probe, has no
  layout rows or never reads travel is counted per interface path; after 3 failures it is
  rejected and not probed again until it has left the bus. A success clears the count.
  Log: `[spark] generic.failure reason=... failures=N rejected=0|1`,
  `[spark] generic.admitted vid pid official_list=1 device_info=1 travel=1`.
- Exact-model behavior (including reconnect every 2 s) is untouched.

## Files

`sparklink_official_identities.h` (generated), `sparklink_model_profiles.h` (`ExactModel`,
`OfficialListed`, `GenericOnly`, `ProbeIdentity`), `backend_sparklink.inc` (candidate filter,
failure memory, synchronous travel proof, session handling), `backend.cpp` (telemetry),
`tools/build_sparklink_official_identities.py` (+ `--check` in `run_native_backend_checks.py`),
`sparklink_model_profiles_test.cpp` (551 identities sorted/unique, ProbeInterface, exact vs
generic, 3151 and AULA 6x21 exclusions, SteelSeries not admitted), project files.
Backups: `.local/backups/sparklink-generic-20261012/`.

## Validation

- `run_native_backend_checks.py --require-compiler`: PASS (all static audits and portable C++
  tests; log `.local/sparklink-generic-checks-20261012b.log`).
- `build_release.ps1`: PASS, all gates (full catalog image check, support diagnostics,
  profile transactions, startup recovery, layout catalog, supported layouts). Local EXE
  SHA-256 `e11f3605ba2b726dac7c7081d82adfe4f86591833b0ee7d42fbe7a9c30069548`
  (`.local/sparklink-generic-build-20261012b.log`). Not published.
- Environment note: with the Git `mingw64` runtime first on `PATH`, `support_log_windows` and
  `input_path_log_windows` crash (access violation inside `libstdc++-6.dll`, a toolchain
  mismatch with the WinLibs g++ 15.2 that `run_native_backend_checks.py` selects). Put the WinLibs
  `mingw64\bin` first on `PATH` before running the checks. Unrelated to this change.
- No device test: the generic path has never run against hardware. Every named SparkLink model
  was validated only through the same backend; the new parts (candidate filter, failure memory,
  synchronous travel proof, telemetry gating) are covered by compilation and the identity test,
  not by a live keyboard.

## Open points

- Which listed boards are Hall-effect is unknown; non-analog ones are rejected by the travel
  proof after at most 3 attempts + 3 reads each (about 9 requests), then left alone.
- A tester log of any newly detected board (`hid.candidate name=`, `[spark] generic.admitted`)
  is what turns a generic board into a named model.
