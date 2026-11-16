# Keydous NJ98 V4S — owned F7/FC replies, revision7, 2026-11-14

Owner requests fixing the reproduced response ambiguity without blind sleeps or
rejecting a valid matrix, and a useful ordinary tester EXE. Investigation:
[alternative firmware/cache source](KEYDOUS_NJ98_V4S_ALTERNATE_FIRMWARE_2026-11-14.md).
Exact3793/Keydous-receiver binaries remain unavailable. No manufacturer contact:
the owner declined it. The MonsGeek dump is comparison evidence, not a target image.

## Problem and bounded fix

The old receiver Client could accept a previous F7 status as an E5/FE depth page
when FC SET_REPORT was staged but its main-loop response copy had not completed.
Production-client counterexample preserved privately. Excluding all status-looking
pages would discard legitimate matrices; a fixed delay would not prove ownership.

After exact remote3793 proof on3151:5038, opt in to the local receiver F0 version
GET marker. This is a read-only control command, not a calibration/flash/reset or
remote setting. The reviewed receiver branch echoes request payload byte7 into
reply byte7. Read the currently visible reply, choose a DIFFERENT high-bit tag,
encode a valid Bit7 checksum, send F0 and poll GET_FEATURE for the echoed tag.
Unused request padding is poisoned; a plain full request echo is not accepted as
an owned marker response. The reviewed local F0 handler clears its response body
and ignores this padding; replay checks preserve RF cache/read-ready. This is
response-processing proof, not a restriction on a version string or depth page.
Firmware version is collected as metadata; no version-number admission gate.

The verified marker is installed before changing response type to F7 and before
FC. Poll GET_FEATURE until that marker is replaced; only then decode the requested
response. No full-page equality/status-pattern/assignment-count rejection. Every
reviewed units scale10/100/200/1000 makes the high-bit marker impossible as a valid
depth page (one16-bit word is at least32768, beyond6*units). A legitimate page
identical to F7, a low-tag F0-looking page, identical consecutive pages and all-zero
pages are explicitly accepted after owned completion.

A delayed FC remains in its fetch state across an I/O deadline. Resume only its
GETs and drain it under the ORIGINAL page identity before issuing another command;
never overwrite it with a new F7/F0 or relabel page0 as page1. The table ticket is
still captured at actual E5 issue, after readiness/marker work. A later stream
sample still supersedes its older in-flight table value.

No Sleep, fixed post-write gap, artificial input frequency cap, deadzone, invented
release, digital-release substitution or new calibration. Deadlines only bound
unresponsive I/O; progress depends on observed replies. Necessary control round
trips cost real bus work; no claim that they have zero hardware overhead. The
independent overlapped1B reader,128-report input queue and immediate publication
remain in place; feature I/O never blocks that thread between packet and publish.
Wired and HATOR receiver implementations retain their existing protocol paths.

## Nonblocking fallback and metadata

A missing/wrong marker echo is not an unsupported-keyboard verdict. Record that
synchronized table transport was not confirmed in this session, stop issuing
unowned table reads, and attempt the real1B interrupt stream. Do not repeatedly
hammer F0 in that live stream-only session. Maintain F7 link status independently
of held/change-driven input. A newly opened session may probe again.

Keydous assignment/layout metadata cannot veto physical analog. If live remaps
cannot be safely read, keep the exact factory physical channels and log that live
remaps are unavailable. If an optional metadata transaction remains unresolved,
switch to stream-only and record the abandoned opcode; no later table can consume
its untyped map reply as depth. Received/malformed completed metadata alone does
not disable synchronized depth. Full layout cache also cannot veto analog.
Known model identity, transport framing, cancellation, genuinely unavailable I/O
and undecodable units remain hard conditions. No new model/firmware allowlist.

Existing built-in97-key ANSI layout and preview/live autoselection remain keyed
by RY1B-3793 /token6CB4C60ADBE352F9. No geometry changes in this patch.

## Evidence and behavioral limits

Private `.local/research/keydous-f7fc-fix-20261114/` contains before.zip with a
six-file hash/restore manifest, own marker-replay.py/json, marker-replay-padded.py/json and host test/build logs.
Marker replay pins receiver SHA256ad3ab42828cf31ed9a7a1e7646462621c556d27ece5dfce590a11152e4ba2f55:
four tags, all four units scales; F0 at0800661A..08006796 writes only USB reply/
stack and preserves the RF cache/read-ready byte in supplied synthetic SRAM.
This executes neither exact Keydous firmware nor hardware USB/RF scheduling.
Runtime echoed-tag proof gates the new snapshot route rather than assuming the
comparison image is the target. No vendor code/binary redistributed.

Behavior review: typing/report source, physical mapping, calibrated range/scale,
shallow filtering and per-slot ordering remain as before; independent simultaneous
keys/releases exercised in host cases. Persistent marker writes absent at reviewed
boundary. Exclusive control ownership retained; normal cleanup still sends1B off,
with bounded cancellation/disconnect handling. Physical typing coexistence,
real RF loss, latency and the residual observed in log51 still need the tester.
A fresh owned RESPONSE is not proof of a fresh keyboard ADC scan: firmware/radio
may still return a stale upstream value. No Supported promotion or RF gaming-rate
guarantee. Do not infer that fixing host ownership fixes all sticky keys.

## Ordinary diagnostics and verification

Revision7 retains prior bound-slot stream/table/applied-zero evidence and adds
trace.keydous_rf.control to ordinary Open log: attempts/ACKs/failures, marker GETs,
old-marker waits for F7/FC, confirmed receiver version, stage, outstanding fetch,
coarse marker reply kind and fallback reason/abandoned metadata opcode. Reply kind:
0matched,1F0 with different tag,2status-shaped,3other,4full request echo;
no raw payload/key data.
Fallback reason1marker unconfirmed,2optional metadata pending. Values come from
cached control observations; support/UI collectors issue no HID commands. No
forced logging, special export, tester script or minimum observation duration.

Native suite with cache PASS once; affected receiver cases rerun after additional
fallback changes. Queued-control cases use actual production Client/decoder/
publication:16positive and4negative, plus existing4positive/3negative Keydous
trace scenarios, total20/7 registered in tester_delivery_cases.json. Includes
late/wrong replies, exact F7-looking matrix, identical/zero pages,1000/mm, unknown
receiver version, stale starting marker, no echo/marker stream attempt, pending
metadata stream attempt, request-echo-only stream attempt, late FC recovery, malformed owned depth and every
control I/O/cancellation boundary. Independent HATOR6positive/7negative retained.
These are software scenarios; no physical keyboard was opened by self-tests.

Final ordinary build_release.ps1 PASS: exact-image8-family tester gate (Keydous
20positive/7negative,16/4 synchronization subset), full catalog, embedded licenses,
production-linked layout/profile tests,91Supported layouts and18startup-recovery
scenarios. Compiled catalog contains97 records for the exact Keydous ANSI preset;
its unchanged auto-selection token is verified above. Additional private test
executes all97 physical channels simultaneously with delayed controls, independent
release to96 and final release to0. Both F7/FC old-marker waits are exercised.
No UI visual run or physical keyboard test was performed.

Installed ordinary EXE: build/bin/Release/x64/HallJoy.exe, version1.6.15.0,
11813376 bytes, SHA256
83b9a97e4aa06dedd9ad1ea82641587c815f0eac7770cc4a92410784bfb3ff4e.
Candidate and installed hashes agree; private delivery-manifest.json records source
hashes, catalog and exact-image receipt. The initially built candidate encountered
the previously observed Shark self-test20; isolated retry passed on the same hash,
then remaining gates completed. The final non-echo-marker change received a new
full wrapper run, all gates PASS; no test was disabled/weakened. Existing features
and the previously running HallJoy window were restored by the normal installer.
No GitHub publication requested. Public support remains Experimental for RF and
confirmed USB as before; no changed support conclusion or live Sheet write.
Tester uses HallJoy normally, observes the symptom, clicks Open log and sends it.
The next log will distinguish confirmed synchronization, stream-only fallback,
and retained values in owned table/stream data; success is not assumed in advance.

## Revision7 result received — 2026-11-16

[Log59 review](KEYDOUS_NJ98_V4S_LOG59_2026-11-16.md): synchronization succeeds
(1098 marker ACKs), but4+ key sticking remains. The remote table transaction
stalls for at least41s while real1B input continues. Saved received zeros are
applied. This fixes response ownership, not complete RF multikey reliability.
