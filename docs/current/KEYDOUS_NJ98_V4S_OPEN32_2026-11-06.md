# Keydous NJ98-CP V4S: wireless-v2 access failure — 2026-11-06

Later [successful direct-stream log and startup clarification](KEYDOUS_NJ98_V4S_STREAM_2026-11-06.md) supersede the unmeasured-stream conclusion below. Startup reliability remains unresolved; reboot alone did not resolve it.

Tester reports no recognition over RF. Local attachment is named
HallJoy-vireless-v2.log (92947 bytes), SHA256
301a0bc77d5b77299b2acff2ecc02ff3635e4a87ccff997ee9a2cc2559b1d58c.
Report UTC2026-11-05T21:00:50Z corresponds to November6 in Moscow.

## Established observation

The receiver3151:5038 IF2 is present and passes collection filtering.
revision2 discovery and exact_stream_pairs=1 establish that the new enumeration
found both control and a uniquely paired eligible input collection. This is not
a missing receiver ID, missing input collection or absent model-layout record.

Every one of73 exclusive control opens fails with Win32 error32 / sharing
violation, uptime4284062..4356109 (~72s). Admission stays phase1, command00,
board0, units0, polls0, exchanges0. No identity proof, stream enable, depth-page
read or publication occurred. Latest native entry is present_not_connected,
mapped_keys0, updates0. The zero native failure counter is not evidence of no
error: open failures are recorded by exclusive_open_failed and rf attempt events.
Five snapshots have complete coverage, consistent summaries and no queue loss.

Thus this run did not exercise the fast scheduler or measure its gaming rate.
It cannot establish a latency regression, stream incompatibility, bad units or
unsupported remote board. No new firmware command reached the receiver here.
The log identifies an access conflict, not the identity of its holder.

## Code review and limits

Session::Open intentionally uses GENERIC_READ|GENERIC_WRITE, share0 for RF.
Its shared-open fallback excludes candidate.receiver to protect the multi-step
F7/F6/FC command channel from another configurator. This open policy is unchanged
from revision1. The earlier RF8k log also had31 error32 opens before later success;
this failure class is not newly introduced by removing the polling delays.

Enumerate owns metadata handles via RAII and returns strings/caps, not open HID
handles. ReceiverStreamPump starts only AFTER a successful control open, proof
and mapping, so its input handle cannot be the cause of the first failed open.
The bundled UAP producer's hwHid::getAll consults MayUsePath before CreateFileW;
SupportedVendor excludes3151. This narrows internal contenders but is static
source review, not a measured inventory of this tester's process handles. No
claim is made that an external app, the game, Keydous software or HallJoy has
been positively identified as the holder. No blind RF shared-handle change is made.

## Next discriminating test

Keep the same revision2 EXE. Restart Windows, leave vendor tray application,
browser configurator and games closed, connect only the receiver and start
HallJoy first. If connected, exercise analog briefly before launching the game.
Return ordinary HallJoy.log even if connection still fails. If a vendor utility
autostarts, exit it fully from the tray before this test. This establishes whether
a clean-start window permits control ownership; it does not assume a culprit.
No2k/8k comparison is useful until admission succeeds. No flashing or scripts.

The same EXE's genuine RF rate, 1B forwarding, typing coexistence and cleanup
remain unmeasured; the previous wired smoothness confirmation remains valid.
Do not award Supported or describe the fast RF path as hardware validated.

## Documentation / Sheet / artifacts

2026-11-06 live metadata: Main, sheetId0, grid1301x26. Exact row472 located by
Keydous + NJ98-CP V4S TMR; A471:C473 readback confirms C472 remains Implemented;
awaiting hardware testing, strict dropdown and matching yellow base/effective
colors. No Sheet edit needed. Fresh all-row notice reconciliation PASS283 models;
receipts are sheet-rows.json and sheet-readback.json in the private task directory.
Runtime, README scope and support notice remain Experimental. Hardware evidence
and the previous gaming report now link this blocked test explicitly.

Log and machine analysis preserved in
.local/research/keydous-nj98v4s-open32-20261106/.
Verified before-change documentation backup:
.local/backups/keydous-nj98v4s-open32-20261106.zip.
No source, runtime, EXE, firmware or public release changed; no build/test rerun
was needed for this evidence-only update. Current delivery is the prior gated
5f667be6 revision2 EXE; this report does not infer its exact hash from the log.
