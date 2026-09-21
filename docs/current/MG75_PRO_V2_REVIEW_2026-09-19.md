# MG75 Pro and V2: firmware and SparkLink SDK review

## Findings and correction

MG75 Pro has a usable independent travel read mechanism in its inspected 1.1.0 firmware. The official SparkLink V1 SDK documents and implements the same mechanism. Calling Pro's SparkLink association unconfirmed merely because it differs from the V2 row protocol was unjustified. The owner's report that Pro works with SparkLink's configurator is consistent with the evidence. This review does not identify the silicon vendor by physical inspection.

MG75 V2 v1.21 has a separate protocol. Its normal travel monitor returns only the greatest current depth and the corresponding key. This behavior is in the firmware, not just in IROK's display. A normal independent multi-key read route has not been established. This is not proof that every possible alternative command has been exhausted.

Neither finding is a live USB or game test. No production backend was enabled, no updater was executed, and the running HallJoy/build was untouched.

## SparkLink documentation and packages

- Owner's link https://sparklinkplayjoy.github.io/keyboard-docs/keyboardv2/keyboard.html is the V2 key-code table. It does not by itself promise one wire protocol for every SparkLink generation.
- V1: https://sparklinkplayjoy.github.io/keyboard-docs/keyboard/api/performance.html documents `getRm6X21Travel()` with independent `travels` and separate `status` matrices. Calibration has separate start/end calls. Travel values do not need reconstruction from digital status.
- V2: https://sparklinkplayjoy.github.io/keyboard-docs/keyboardv2/api/performance.html documents `getRoute({row})`.
- Downloaded npm packages without executing them: `@sparklinkplayjoy/sdk-keyboard` 1.0.24 (tgz SHA-256 35a339ab2dfeb3b336116b4fc17c6be8fd6f6ad8f8506b9a4e8fe18f7224048f), and `sdk-keyboard-v2` 1.0.29 (1dffe515b515d13f40967e21ccfd1a23364f47a6702d53fa4cf4a9a14b834fce).
- V1 `getRm6X21Travel021/022` requests type 2, halves 1/2 through `rm6X21Pack`, command 0x12 framed with 0x5C. `getRm6X21data` decodes exactly three rows of 21 little-endian values per half, divided by 1000. V2 uses the distinct row command. Two SDK generations are not one universal packet format.

> Runtime integration is now implemented: see [MG75 Pro native integration](MG75_PRO_INTEGRATION_2026-09-19.md). Hardware validation remains pending. Earlier next-step paragraphs below describe the pre-integration state.

## MG75 Pro 1.1.0

Pinned image: `356aa3a6c6c77fde51f32d041c9aaa5b145aca183e8024e085410fdd20544feb.bin`, 130456 bytes (name is SHA-256), extracted from official updater documented in MG75_FN_REVIEW_2026-09-19.md. Cortex-M vector table at file offset 0x1000, image-address minus file-offset 0x08040000; product strings identify IROK MG75 PRO.

Dispatcher command 0x12 reaches 0x0805502C. Type 2 selects half 1/2 at 0x08056674; sources are 0x200092BE and 0x2000933C, 126 bytes apart. Serializer 0x0804951C copies three rows of 21 independent 16-bit values. Response length is 132 bytes: `5C 80 92 checksum status type` followed by 126 data bytes. No calibration call is in this selected read branch. The existing HallJoy Spark row backend cannot consume this framing directly.

The SDK waits for three 64-byte reports per half but decodes only 63 values. Padding must not become extra keys. The IROK UI's concatenation convention includes padding: its Fn index 146 is not a direct compact-matrix index (93 padded slots in the first half; compact Fn index 116, row 5 column 11). A production decoder should use bounded half payloads and physical positions, not copy the UI's padding convention.

Next implementation work: exact device/protocol admission; validated three-report assembly for each half; complete matrix/key mapping and Fn; stale/release handling; device range settings and native publication. A two-half read is not an atomic whole-keyboard snapshot. Runtime compatibility remains pending implementation/testing; firmware capability is established.

## MG75 V2 1.21

Owner decision after this review: defer further investigation until a tester is available. Firmware availability removes the old acquisition blocker; support remains unimplemented, not proven impossible.

Official source https://www.irok.cn/news/85 links `MG75V2-20260713-v1.21-A.exe`, SHA-256 2d1b128d671aa0f9b3206ca95a1e60472af01b0be547ae7714df673984262b7a. Parsed its .NET resource `IAP_Demo.Resources.data.bin` without loading/executing the program. Header gives load address 0x08006000 and payload size 393904 bytes after a 64-byte header; payload SHA-256 a95e1450ac9ad668f15d5b56cc561b820fbc3e20f00b6b393c855761f85cba17. Resource remainder is padding. Product string: IROK MG75V2 C12. Official web-driver filter: VID 0x2F81, PID 0x3007, usage page 0xFF01, usage 1.

Normal travel command is 0x43; handler 0x0800F8A0 toggles monitor bit 5 in byte 1 at 0x2000048C. Routine 0x0801584C scans a 15x6 matrix, converts each sample with 0x08008818, and updates one retained key only when its depth is greater than the retained maximum (comparison at 0x080158DA). It emits `A0 key adc_hi adc_lo depth_hi depth_lo` through 0x08017BF0. Equal depths retain the first encountered key. Repeated polling cannot recover simultaneous independent depths from this winner-only report.

A separate per-key report routine at 0x080143D0 is gated by the calibration state; official driver uses command 0x44 for calibration and 0x43 for monitoring. Do not enable calibration as a HallJoy input mode. Whether another read command can expose independent depths without changing normal typing remains open; this review does not claim a completed exhaustive command audit.

## Reproducible validation

`python tools/review_mg75_firmware.py` requires the pinned local firmware files plus dnfile and unicorn. Three cases execute the real Pro serializer (all released, distinct depths, alternating partial/full values) and assert every one of 63 values and the header/length. Four cases execute V2's real selection routine (released, three simultaneous depths, changed winner, tie), substituting calibrated depth conversion and USB send only. Thus V2 tests prove the selector's information loss, not sensor calibration accuracy or live USB timing.

All seven cases passed. Evidence: `.local/research/irok-mg75-fn/review-pro-v2-tests.txt`; selected disassembly and hashes: `../research/MG75_PRO_V2_FIRMWARE_EVIDENCE_2026-09-19.json`. Official documentation snapshots and npm packages are retained in the same private research directory. This review supersedes the Pro family inference in MG75_FN_REVIEW_2026-09-19.md; its historical observations remain available.
