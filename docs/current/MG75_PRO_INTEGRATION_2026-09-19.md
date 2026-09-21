# MG75 Pro native integration — 2026-09-19

Owner approved enabling MG75 Pro without a hardware tester, with an orange experimental-support notice and the existing Discord invitation/QR. This supersedes the pending-integration status in the firmware review. MG75 V2 remains outside this change.

## Implemented

- Ordinary native backend `irok-mg75-pro`, exact VID/PID 1CA5:0807, product `IROK MG75 PRO`, usage FFA0:0001, unnumbered 64-byte vendor reports. Shared IDs alone do not admit plain MG75.
- SparkLink V1 command 12/type 2 polls two 63-slot halves, three reports per half. All 81 populated physical positions, including Fn at compact index 116, feed real analog values to the existing gamepad path. Digital keyboard events do not select, gate or synthesize depths.
- Fixed 0–3500 micrometre normalization. Official pinned IROK JS groups `IROKMg75PRO` in `JingTaiMg75`; `Fn=cs(Je.JingTaiMg75)` maps to `Gkt` with unit 1000/maxTravel 3500. No observed-maximum scaling and no calibration command.
- Three read-only factory-row queries (2B), then six batched base-assignment queries (23). A key review correction: firmware always returns fourteen records (61 total bytes), even for a single requested key. Actual firmware fixtures cover all 81 keys, not a guessed nine-byte reply.
- Automatic layout uses the verified Pro identity and live base assignments; manual layout uses factory positions. Duplicate assigned codes merge by maximum independent physical depth; releasing one alias cannot release another held alias. Physical Fn maps F001 to HallJoy 0409. Unsupported non-keyboard actions remain unassigned; firmware macros/media and dynamic Fn-layer actions are not converted into analog keyboard keys.
- Partial/error/timeout responses terminate the current session, clear input and reopen with a flushed queue. Untagged half replies are never pipelined or guessed. Values older than 150 ms become neutral. Stop cancels outstanding I/O; subsequent starts reset cancellation state.
- Exclusive access applies only to the vendor HID interface, leaving ordinary typing intact. Close the web configurator while using HallJoy; its concurrent ownership can prevent admission. No firmware/settings writes, calibration, device reset or digital input emulation.
- Existing orange banner shows `IROK MG75 Pro: testing incomplete`, with the instability/non-operation warning and Discord/QR. Exact USB metadata and native telemetry both enable it.
- Existing Enable logging controls ordinary-build logging. Added phase failures and aggregate protocol-health counters; no per-key press sequence or raw travel dump is added.

## Evidence

Pinned firmware and SDK identities are in [the firmware review](MG75_PRO_V2_REVIEW_2026-09-19.md).

- `tools/review_mg75_firmware.py`: executes actual firmware serializer 0804951C, factory serializer 08048D10 and read branch 0805540A through 080565C6. Three depth matrices, six factory rows, all 81 assignments, remapped W, unassigned A and Fn pass. Live map memory remains unchanged. This is emulation of firmware code with seeded RAM, not a sensor/USB test.
- `mg75_pro_protocol_test.cpp`: independently generated actual-firmware byte fixtures, all halves/map rows, padding, partial/malformed reports, checksum, wrong echoes, exact identity and Fn.
- Production-linked profile regression: independent halves, remap aliases, release, manual/automatic semantics and stale neutralization. Existing layout/catalog/profile/hidden-controls tests also pass.
- `tools/run_native_backend_checks.py --require-compiler`: all static audits and portable/Windows C++ tests pass, including the new firmware-fixture protocol test. Evidence: `.local/mg75-pro-native-checks.txt`.
- Latest production-linked profile run passes, including `mg75_pro_depth_remap_alias_fn_release_stale=PASS`; evidence `.local/mg75-pro-native-profiles.txt`. Firmware execution evidence: `.local/mg75-pro-firmware-checks.txt`.

## Hardware limits

No MG75 Pro has been connected locally. Firmware 1.1.0 and the official SDK establish the read mechanism; USB behaviour, sustained polling performance, other firmware revisions and actual gameplay still need user testing. Two half reads are sequential, not an atomic full-keyboard snapshot. Base assignments are read on connection; reconnect HallJoy after changing keyboard profiles/settings. The experimental notice must remain until sufficient real-device evidence is available.

## Delivery

Ordinary EXE installed by `tools/build_release.ps1`: `build/bin/Release/x64/HallJoy.exe` (9353216 bytes).

SHA-256: `e48a9399d5dae356c0d02d2e1cc668618aa1c89769215395d52bf5bb358d72bc`.

The four linked release checks (Attack Shark, Mini60, NA87, embedded ViGEm installer) passed before replacement. Build log: `.local/mg75-pro-release-build.txt`. Existing ViGEm PDB warning remains unrelated. No GitHub release was published. Logging remains opt-in through Enable logging.
