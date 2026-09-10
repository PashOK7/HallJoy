# RM-30-HEX80 — ATK x QK Hex80 `0x96` protocol review

Date: 2026-09-06

## Admission and wire contract

The route requires `VID 373B`, usage `FF60:0061`, adequate HID reports and
either a known PID (`1176`, `1177`, `1250`) or the same complete GET proof.
Payloads are 128 bytes, accepting an optional leading zero report ID.

| Operation | Payload | Correlated response |
|---|---|---|
| Travel scale | `02 96 24` | matching `02 96 24`, big-endian scale in bytes 3..4, valid only in 256..20000 |
| Four-slot matrix chunk | `02 96 1C`, big-endian offset bytes 5..6, count byte 7 | matching `02 96 1C` with exactly the requested offset/count and enough five-byte entries |
| Calibration restoration | `03 96 19` | no value is trusted from this write; it is permitted only after both GET proofs on the current open interface |

The `03 96 19` command is not the calibration-entry command (`03 96 18`). It
remains because existing physical/protocol documentation identifies it as an
idempotent restoration to ordinary mode. A new path never receives it before
travel scale and first matrix chunk have been re-proven.

## Parser/publication contract

- The fixed layout has 104 slots and 82 published HID usages. Unmapped and
  vendor-only slots, including Fn, are never aliases for another key.
- Every full cycle requests chunks of four slots. A response with a different
  offset/count, insufficient length, malformed header or travel above the
  bounded plausible domain is rejected; a missing chunk prevents a complete
  cycle record and repeated failures end the session.
- Travel is normalized only after parser validation, with an 8-count deadzone
  and the current proved scale. Stop/failure clears owned values and cancels the
  active I/O before its bounded join.
- The route starts after realtime, uses the shared exact-interface registry and
  blocks digital fallback only for its currently owned HID usages.

## Verification and remaining evidence

`hex80_protocol_test.cpp` checks layout count, PID gate, optional report ID,
normalization, wrong chunk offset/count, short input and implausible travel;
routing and cooperative-stop static audits passed in the full static suite.
No live HID command or HallJoy executable was run during this review. Physical
firmware variants, sustained input/release and multi-device behavior remain
separate evidence gates.
