# RM-30-UAPNUPHY protocol audit — 2026-09-06

## Scope and admission boundary

This is the embedded UAP NuPhy stream reader. Current discovery limits itself to
VID `19F5` and Generic Desktop `0001:0000`, but has no per-PID capability
exchange, input report ID/length proof, or firmware/layout allow-list. The
existing "everything by NuPhy" wording is therefore not evidence of universal
production support and is not expanded by this card.

The established stream record is `A0`: byte 0 type, byte 1 unknown, big-endian
16-bit scan code at bytes 2–3, big-endian 16-bit travel at bytes 4–5, and an
untrusted vendor field at bytes 6–7. Modifier scan-code cases are decoded
separately; ordinary values use the HID scan-code converter. `6120` and `FEE0`
retain the pre-existing 1600-scale exception; other admitted devices retain the
legacy 800-scale rule. The source does not prove that this default is correct
for every future PID.

```
A0 event -> length >= 8 -> BE16 code + BE16 raw travel -> scale-domain check
         -> per-device raw cache -> normalized active-key publication
```

## Safety and precision correction

The old parser read through `MemoryRefReader` without verifying each operation,
then converted `value / scale * 255` directly to `uint8_t`. A short HID record
could leave fields uninitialized; a raw value above the assumed scale could make
the float-to-byte conversion unsafe; and a valid 800/1600-step stream was
reduced to 256 stored levels before HallJoy received it.

The reader now requires all eight record bytes before decoding, fails closed on
raw travel above the established scale, and retains `uint16_t` raw travel until
normalization for publication. It does not silently clamp a malformed record or
invent a new scale. Constructor initialization still clears the complete state
union, so the wider cache begins deterministically at zero.

## What remains evidence-bound

There is no captured per-PID capability protocol, complete model map, report-ID
contract, full snapshot, or stream sequence number. A lost release can still
leave cached travel until disconnect, and the generic VID/interface admission
cannot distinguish a protocol-incompatible future NuPhy model. Those limitations
cannot be solved honestly by guessing a PID table or sending vendor commands;
they require representative captured hardware evidence.

- Parser/static: PASS — `nuphy_uap_static_audit.py` locks record bounds,
  big-endian field locations, fail-closed range handling and raw precision.
- Physical: no current-session hardware run. No HID session, firmware action or
  ROG Azoth diagnostic was started.
