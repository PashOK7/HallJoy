# Keydous NJ98-CP V4S TMR: USB + receiver test — 2026-11-05

Historical revision1 report. [Gaming-path correction and tester logs](KEYDOUS_NJ98_V4S_GAMING_2026-11-05.md) supersede RF scheduling, stream selection and untested-depth conclusions below.

Owner explicitly requests an ordinary test EXE that attempts both wired and
wireless operation. This supersedes the earlier e72b6c99 wired-only local build.
No new firmware is flashed and no vendor configurator code is executed.

## Firmware acquisition and review boundaries

The exact V4S registry/class and official radio transport are pinned from the
manufacturer-linked qmk.top v4 driver. Main bundle SHA919b1049..., V4S parent
SHA7982b093.... The official desktop installer
Keydous_v4_setup_3.2.23_WIN2026083.exe (107197096 bytes,
SHA436a2a47c460353cb2cc7275df4375f6907ee801cb6171639d76d40812c64a13)
was statically extracted with7Zip, never run. Its main index.e1b98d47.js
(SHAca32f8c00f493552e3f2e51b9a0ebcbd123171241a5e03a9e388b99f3e7cab2c)
independently names board3793 and the same radio-control methods.

Exact keyboard/radio MCU bytes could NOT be established: the official firmware
API get_fw_version with dev_id3793 returns HTTP500 / Record not found; alternate
official endpoints return404. The complete installer archive lists no MCU .hex
or .bin image; Electron/V8 snapshots are not keyboard firmware. Search was bounded
to these sources and the manufacturer pages; exhaustive absence is not claimed.
There is NO exact-MCU execution or firmware behavioral PASS. Do not replace the
missing bytes with NJ98 V4 HE firmware or claim host mocks emulate the radio.
Private acquisition receipts/installer/source: .local/research/keydous-nj98v4s-wireless-20261105/.

## Candidate review and selected route

Selected: read-only E5/FE/01/page depth snapshots through the official radio tunnel.
F7 reports remote readiness/online/boot state; raw F6/0A selects the keyboard;
keyboard commands keep their own checksum; raw FC precedes response retrieval.
The receiver controls have zero padding and NO keyboard checksum. Read readiness
is required for every response. A pending prior response is consumed before our
request. Control I/O is bounded and cancellable; the command channel is exclusive
so another configurator cannot switch its target between request and response.

1B streaming over RF remains unresolved: client method availability does not prove
firmware forwards per-key notifications or recovers after host exit. It is not
enabled by this receiver path. Calibration, raw sensor/calibration helpers,
settings writes and flashing are rejected alternatives. The wired stream and its
existing cleanup remain unchanged.

## Admission, input and failure behavior

Only receiver3151:5038, interface2, usageFFFF:2, feature65 is eligible. The USB
identity alone never names a keyboard. F7/F6/FC + read-only8F must prove remote
board3793 before keyboard-specific metadata/maps/depth are used. Other remote
boards, mouse-only status, bootloader and malformed status are refused. USB is
preferred when both transports are available. Topology changes end the radio
session so the worker can rescan.

Radio metadata uses the same exact model, precision and factory/live-assignment
maps as wired V4S. The97-key ANSI preset and verified RY1B-3793 token are shared;
receiver VID/PID is not a product/layout alias for any other model.

Four pages/128 physical slots must validate before any frame is published. A
partial read, request echo, invalid report ID or out-of-range depth never replaces
the previous frame. The source becomes connected only after a complete fresh
frame. Failure immediately clears publication/connection; data older than1500ms
also returns neutral even during a blocked poll. Frames taking over1500ms are
refused. Normal simultaneous holds and explicit per-key zero releases use the
common physical publication and gamepad binding paths, with no digital emulation.

Freshness and RF latency remain hardware unknowns. Pages are read sequentially,
not as an atomic hardware snapshot. Faster readiness polling than the vendor UI's
100ms polling waits is still gated by actual ready bits; no latency is promised.
Normal typing/multikey/release/coexistence, shallow range/noise and real RF loss
need testing. Read-only commands add no persistent mode to clean up on exit/crash;
this is a static host-side finding, not measured keyboard behavior. Close the
manufacturer configurator; an occupied exclusive channel is logged and retried.

## Behavior findings (source/host scope, not MCU PASS)

| Finding | Disposition and evidence boundary |
|---|---|
| Normal typing | Hardware unknown; no keyboard setting/mode writes in Client.Frame. |
| Simultaneous keys | Host established by rongyuan_receiver_test.cpp independent depths; RF unknown. |
| Depth scale | Static source reviewed in07c90958.js getPrecisionEnum/getLegacy; same ModelUnits as USB; physical range unknown. |
| Shallow input | RF/firmware unknown; host normalization imposes no invented cutoff. |
| Temporal filtering | Firmware unknown; sequential page latency is measured, not guessed or hidden. |
| Release | Host test establishes per-key zero and failure clearing; RF delivery unknown. |
| Physical mapping | Established from pinned exact V4S factory/geometry;97 mapped keyboard/Fn usages. |
| Packet loss | Restricted: complete frame only, failure clear and1500ms age guard; actual RF loss/firmware stale replies unknown. |
| Persistent settings | Host established: F7/F6/FC transport controls and read-only keyboard queries; no save/calibration/flash commands. |
| Normal exit | Host stop/cancellation and publication clear; no RF stream mode enabled. Physical behavior untested. |
| Crash recovery | No persistent/stream mode is entered by this route; actual keyboard/dongle recovery unknown. |
| Configurator conflict | Restricted to exclusive control ownership; access denial is logged/retried. Real coexistence untested. |

Concrete locators: pinned v4 main class kQ methods ___dongleSend24GDeviceByF6,
___dongleCheck24GStatusByF7,___dongleNoticeReadByFc; V4S class aade6cc1.js ->
07c90958.js _getMulitMagnetismCMD. Host producer is rongyuan_receiver_client.h
Client::Query/Frame and rongyuan_stream_backend.cpp RunReceiver/Connected/Get.
Acquisition/source hashes and endpoint failures are preserved in acquisition.json
and fw-query-3793.json. No missing exact firmware asset is relabeled as a PASS.

## Ordinary log evidence

No forced continuous logging or raw HID dumps. Retained evidence includes:
- evidence.rongyuan.rf discovery revision1: exact eligible receiver collection.
- evidence.rongyuan identity revision4: proved remote board, actual USB/RF
  firmware fields, feature enum, selected units and full-travel range.
- rf attempt: admission phase1=open,2=identity/units,3=maps,4=depth,5=frame-age;
  named radio stage, actual command, native error, online/send/read/boot flags,
  exchange/poll counts, failed page and snapshot refusal reason.
- snapshot_reason:0=I/O/none,1=report ID,2=request echo,3=depth range,4=units missing.
- rf travel/end: complete frame count, measured frame time/max, aggregate moving/
  positive slots and release transitions. No key identities or per-key values.
Repeated identical failures are rate-limited; stage changes are retained promptly.

## Validation and delivery

Native checks PASS. The initial new test expected truncation instead of the
existing normalization's nearest rounding (187 vs188); corrected only its expected
values. The resume retained87 successful C++ results and ran54 failed/unexecuted
tests. No successful static audits were rerun. Logs: native-checks.log and
native-resume.log. The exact production receiver client passed fake-control tests
for stale-response draining, board admission, every complete-table page,
independent holds/releases,32 injected I/O failures and32 cancellation points.
Invalid data never partially replaces a frame. These are HOST tests, not RF tests.

Ordinary tools/build_release.ps1 EXIT0. Complete native catalog, embedded resources,
production-linked profiles/automatic layout transitions, layout coverage and
startup recovery (18 scenarios plus repeated startup) PASS. Compiled catalog has
97 V4S ANSI keys; remote Model3793 resolves the SAME verified product/preset as
USB, and other boards/receiver-as-product identities stay excluded.

Live Main!C472 remains Implemented; awaiting hardware testing. Fresh readback
matches Keydous / NJ98-CP V4S TMR, strict dropdown, yellow base AND effective colors.
All284 yellow notices and86 Supported layout records reconcile; no Sheet write
was needed. Evidence: sheet-readback.json and sheet-rows.json in the task directory.
README, hardware evidence and NEXT notes now describe the experimental receiver
attempt. This does not promote hardware support or claim a successful RF test.

Delivered ordinary EXE: build/bin/Release/x64/HallJoy.exe, version1.6.14.0,
11,329,536 bytes, SHA256
`6cb724b04939dc8aefbca38195aedac13b97037de20f7fb0c244cfa76c579698`.
Installed bytes equal the gated Release candidate; prior wired e72b6c99 is
superseded locally. All approved native functions are retained. No GitHub upload.
Tester: unplug USB to test RF (USB wins), close the vendor configurator, exercise
several simultaneous partial presses/releases, return normal HallJoy.log via Open
log. The same EXE also tests USB. No firmware flashing or tester scripts required.

Verified before-change backup: .local/backups/keydous-nj98v4s-wireless-20261105.zip.
No GitHub publication is authorized by this test-build task.
