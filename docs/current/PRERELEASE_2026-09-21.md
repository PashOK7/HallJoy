# HallJoy 1.6.0 prerelease - 2026-09-21

## Delivery

- Ordinary Windows x64: `build/bin/Release/x64/HallJoy.exe`.
- File/product version: 1.6.0.0; application version: 1.6.0.
- Size: 9466368 bytes.
- SHA256: `9d42bc7cc087404d29fcc8a7459b3c923ad16ba431e11764563be3b966e15358`.
- Owner approved stable v1.6.0 publication; final Linux and Windows CI passed.

## Changes and owner decision

The owner approved ordinary ATTACK SHARK X65 Pro support following tester
confirmation of analogue input, browser gamepad output with Block Bound Keys,
and subsequently working gameplay. IROK NA87 and AULA MINI 60 HE Pro retain
previously approved ordinary support, prepared locally but not yet published.
User-facing support text uses model names, without internal identity numbers.

keyboard_ui.cpp now excludes the verified X65 Pro layout group from the family
testing notice. The native backend publishes that token only for a connected,
identified session. Other family models and unknown tokens retain the notice.
No protocol, scaling, blocking policy or gamepad-output behavior was changed.
The group covers the existing admitted X65 Pro profiles; it does not assert that
every hardware revision was individually tested.

README, compatibility table, release notes and current documentation were
updated. Code/public-doc backup: `.local/backups/x65-prerelease-20260921-171719.zip`.
The replacement helper also preserves the previous executable in
`build/obj/replacement-backups/`.

## Build and verification

Built the ordinary Release x64 native target through MSBuild with all discovered
project diagnostic, simulator, UI-audit, qualification and experimental override
properties explicitly false. Required ordinary native backends remain enabled.
The existing staged candidate/replacement workflow was used. No keyboard-specific
self-test or physical-device test was run, as requested; diagnostic source tools
remain available separately for future investigations.

- Source encoding build gate: PASS (631 files).
- Support logging static audit, release isolation and crash-memory privacy: PASS.
- Actual compiler commands: production defined; forced-log and diagnostic flags absent.
- Binary checks: focused input-path and X65 diagnostic title/build markers absent.
- Embedded ViGEm installer resource check: PASS; no driver installed.
- Delivered bytes match the validated candidate; version and hash read back.
- No visual launch/test, game run or firmware test performed.

Build log: `.local/prerelease-20260921-171738.log`.
Warnings remain: C4267 in existing sayo_layout_state.h size conversion and LNK4099
for the dependency's unavailable ViGEmClient PDB. Compilation/linking succeeded.
Optional logging follows the saved user preference and is off by default; no
preference was changed. Mandatory crash/missing-keyboard/failure reports remain.
AJAZZ-specific diagnostic code and automatic input-path logging are disabled.

## Remaining scope

The tester's rapid Forza/Roblox keyboard/controller switching stopped reproducing
without a HallJoy fix. Its cause remains unknown, separately from X65 Pro support.
Other model validation limits and factory-assignment/wired transport constraints
remain in SUPPORTED_HARDWARE.md. No new universal compatibility claim is made.
Owner evaluates the delivered UI before subsequent GitHub publication.

## AppData runtime follow-up

Owner requested removing the UAP DLL beside the executable. The private plugin
now always uses `%LOCALAPPDATA%/HallJoy/Runtime/v1.6.0/`, including portable mode.
Removed the executable-directory destination and obsolete force-fallback branch;
atomic extraction, exact embedded-byte validation and the child load lease remain.
No fallback writes a DLL alongside the executable if AppData is unavailable.
The old simulator flag is harmless; its existing location=user expectation remains.

Rebuilt ordinary Release and passed private-runtime static checks, support-log
isolation/privacy checks, and the embedded installer resource check. Two headless
legacy-helper invocations verified extraction and reuse in AppData with exact
resource bytes and no sibling DLL. No keyboard-specific or visual test was run.
Build log: `.local/runtime-appdata-build.log`; only existing LNK4099 warning.
Delivered hash/size above updated to this build. Previous EXE is backed up by the
replacement helper. Source backup: `.local/backups/runtime-appdata-20260921-172636.zip`.
The old local delivery DLL was byte-verified, backed up and removed; the application
does not delete arbitrary old DLLs in other installation directories.

Normal settings/profiles and HallJoy.log use AppData; portable mode keeps those
user data beside the executable. An unhandled crash can still create
HallJoyCrash.txt beside the EXE. Existing logs and build-generated PDB/MAP files
were preserved; they are not DLL extraction or normal end-user runtime output.

## Dual optional logs - final 2026-09-21 follow-up

Supersedes the previous crash-location note. Enable logging now writes HallJoy.log
to both the data root and the executable directory. The mirror is enabled only
for continuous logging (including its final OFF transition). Automatic incidents
with logging OFF stay in the data root. Portable equal paths use a single writer.
Each destination keeps independent size, error and five-second retry state;
failure of either does not suppress the other. Failed reset temporary files are
removed. Existing EXE-side logs remain when logging is turned off.
Production crash destination is precomputed in the data root before installing
the handler: HallJoyCrash.txt no longer appears beside a normal installation.
Portable data storage and AppData-only UAP runtime are retained.

Verification: actual Win32 writer suite PASS for OFF/no file, automatic incidents,
dual ON/OFF, matching final copies, bounded logs, failed primary/recovery, locked
mirror with successful primary/recovery, and equal-path single session output.
Static logging/isolation/privacy/crash-destination checks PASS. Release build and
embedded installer resource check PASS. No visual, hardware, keyboard-specific
or induced-crash test. Build log: `.local/dual-log-build.log`; existing C4267 and
ViGEm LNK4099 warnings only. Delivered candidate bytes verified; latest size/hash
above. Backup: `.local/backups/dual-log-20260921-173252.zip`.

## Publication preparation

GitHub inspection confirmed NA87/MINI60 were already published in v1.5.3 on
2026-09-19; earlier local-only statements were stale. Public 1.6.0 notes corrected.
First CI run 35614588675 passed Linux and all Windows functional/profile tests,
then rejected compiler warning C4267 in the three-key O3C binding loop. Added an
explicit uint16_t conversion for codes 0x470..0x472; no mapping/policy change.
Rebuilt locally with only the allowed ViGEm PDB warning. Final candidate hash/size
above supersede the earlier dual-log binary. Resource check and exact GitHub
roundtrip download PASS. Final source CI: 35615925037: Linux and Windows PASS.
Three stale documentation audits were updated for the current output path and
current hardware-test guidance; their checks remain enabled. All 114 static
audits pass after these corrections. No runtime logging preference was changed.

Final code revision: `eda5f3e5f2cf57929bb5c27924a09cf606bc8a1a`.
CI: https://github.com/PashOK7/HallJoy/actions/runs/35615925037.
Release uses the locally built, hash-verified EXE, not a replacement CI artifact.
