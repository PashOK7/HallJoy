# AULA F75 HE and LANGTU BOOM68-USB — tester logs, 2026-11-14

## Scope and verified inputs

The owner supplied two screenshots and downloaded logs. Read the actual files,
not only the screenshot excerpts. Private immutable acquisition and SHA-256
records: `.local/research/aula-boom68-feedback-20261114/`.

- AULA: `HallJoy1.txt`, 56773 bytes, SHA-256
  `d3ffe2cbd1ebf84bf036ae36281dd1e17378e18d42b52c7ea37404c8ddfeb301`.
- BOOM68: tester text log, 186066 bytes, SHA-256
  `055e4981bf8e1e70d642661c2b929c7e79f537861125df716bd37c3143d02b24`.

The existing support-log analyzer finds 3 / 14 snapshots respectively; the latest
snapshots have complete catalog coverage, consistent summaries and no observed
queue-loss marker. This does not imply the entire earlier session was retained.

## AULA: actual receiver trial outcome

See [implementation and prior exact-firmware review](AULA_F75_HE_2026-11-13.md).
The logged session reaches the expected receiver `0C45:FEFE`, exact product name,
FF60:0061 collection, 33-byte input/output reports. It opens successfully and
receives remote device info `38A6:2729`, firmware BCD `0155` (1.55). Optional
precision/frame metadata are zero. Assignment reads succeed and the 80-channel
layout cache is published. There is no firmware-version or layout admission
refusal here.

All 29 recorded exchanges have replies. Direct command 0x68 returns the request
body, explicitly recorded as `request_echo_not_analog`; it supplies no independent
measurement. HallJoy then sends reversible monitor-on 0x66, receives its ACK and
waits for live depth without a silence gate. Both evidence snapshots show zero
valid depth batches/samples; no first 0xFB shape or range rejection is recorded.
The log does not show a send/receive failure at this stage. It does not prove
physical key activity, receiver forwarding internals or the absence of another
firmware command. No wired endpoint appears in this log.

Masked branch hashes `0C52EF865F588A93` / `E1DCEC0FAD12F928` match the reviewed
reset-image branch fragments. This is not an identity hash of the whole firmware
and is not a receiver firmware dump. The result agrees with the earlier bounded
review: these images echo 0x68 and no ordinary producer for 0x66 was established.
Do not repeat the same test EXE as though opening a layout could solve this.
Next work is an independently justified analog producer / receiver path, plus
wireless-versus-wired evidence when available; no calibration-mode workaround,
synthetic values or automatic unsupported conclusion follows from this log.

### Diagnostic defect found during log review

`aula_f75_backend.cpp::Evidence` formats current/depth messages into 1024 bytes,
but `support_log.cpp::kEvidenceLineBytes` is 384. The supplied log really ends the
current record at `no_a` and depth record at `status_scale_hardware`. Therefore
late flags/scale/release-pending fields are lost. Core identity, routing, command
outcomes and sample counts above remain visible. Before another AULA tester
build, split those records into bounded messages and verify the normal log writer
retains every intended field. Do not enlarge all runtime evidence buffers blindly.
No production edit or replacement EXE was made in this review task.

## BOOM68: exact target and official firmware acquired

The tester log contains `0C45:80B5`, product `BOOM68-USB`, interfaces 00/01/02/03.
Current MINI60 admission excludes PID80B5, so the log is enumeration evidence,
not a failed analog transaction or proof of a hardware limitation. The current
MINI60 diagnostic reports zero eligible wired collections and sends no trial to
this target. The screenshot suggestion of RongYuan is not an established fact.

Fresh official [LANGTU configurator](https://langtu.driveall.cn/) assets:
`main-C8Qauz_K.js`, `layout-classic-QUpyWhzQ.js`. The latter explicitly names
vendor3141/product32949 (`0C45:80B5`), BOOM68-USB, USB and LANGTU. Its common
command table includes monitor 102/103 and direct magnetic status104, using
8-byte per-key status records with calibrationStatus, ADC LE16 and currentStroke
LE16. This is the Sonix AA/55 family shared with MINI60; the config list and
shared SDK alone do not establish working multi-key analog on this firmware.
A bare PID addition remains unjustified pending the firmware behavior review.

The official [update list](https://langtu.driveall.cn/update.json) has the exact
product32949 / BOOM68-USB entry version1.10, file `BOOM68-USB_V1.10.exe`.
Downloaded from the same origin; 2386384 bytes. Statically extracted PE RCDATA
without executing the updater or contacting a keyboard. Resource4008 says0C45,
4009 says80B5, 4010 saysSN34F280; resource4016 namesSN34F280_User.hex.
Resource4000 is a 516096-byte ARM image, SHA-256
`4d44b59daeef15271826043715ff5126324c4d84bf19c6407de0511398a86f25`.
Other resource hashes and the updater hash are retained privately. Package1.10
is not a measurement of the tester's installed firmware version.

Next: review its command dispatcher and scan producer for independent all-key
samples/releases, safe identity, typing coexistence and cleanup; extract the
exact built-in layout/auto-selection before a first tester EXE. The image has
been acquired, not yet behavior-replayed or declared suitable. No firmware or
configurator assets have redistribution permission established; retain locally.

## Follow-up

[BOOM68 integration and delivery](BOOM68_USB_2026-11-14.md) adds the read-only
67-key stock matrix and resolves truncation of the shared Sonix report fields.

## Status / changes

No production-code, EXE or support-status change. F75 remains Research incomplete;
BOOM68 is an investigation, not declared Supported/Experimental. No live Sheet
write or new conclusion requiring a status promotion. No device I/O, flashing,
manufacturer contact, visual run or physical hardware test. The existing Keydous
revision7 EXE remains intact and is not a new BOOM68/AULA fix.
