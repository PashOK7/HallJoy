# IROK/IYX Pro firmware: analog array reads

> Исправление 2026-09-13: 0x29 — opcode входа Witmod SDK; фактический HID
> opcode — 0x21. Полный scanner и все serializer entry points теперь проверены
> offline. См. [IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md](IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md).


Date: 2026-09-13. Static analysis plus offline emulation; no production backend enabled.
No vendor executable was run and no physical HID device was opened.

## Acquisition and reproducibility

Continuation of `IROK_NA87_RECON_2026-09-13.md`. All downloaded binaries and
derived listings remain in `.local/research/irok-na87/`, not redistributed.
The four signed Pro updaters embed ASCII HEX payloads immediately following
their official binary URL and SHA-256. `extract_pro.py` extracts these and
requires an exact SHA-256 match. There are 15 embedded entries but only **10
unique images**: the 1.0.8 and Cyan updaters contain the same five payloads.
`pro-images/manifest.json` records full hashes, URLs, sizes and source offsets.

These are real executable RISC-V application images, not encrypted installers.
For NA87 1.0.8, both identity strings at file offsets 0x28c and 0x14678 explicitly
say `IROK NA87 PRO`. The short `NA87_App` download filename does NOT make this
ordinary NA87 firmware. Link addresses are file offsets + 0x4000, confirmed by
startup addressing and the command jump table.

Initial Capstone listings are navigation aids only: standard RVC decoding
mistakes QingKe XW byte/halfword instructions for floating-point operations.
Authoritative serializer listings use LLVM 21.1.8 `--mattr=+c,+xwchc` after
wrapping unchanged bytes with llvm-objcopy. See the official LLVM support notes:
https://llvm.org/docs/RISCVUsage.html (Xwchc).

| Image / declared revision | SHA-256 prefix | Serializer file offset |
| --- | --- | --- |
| NA87 PRO 1.0.3b | a25865c6cdd2 | 0x6fba |
| MU68 PRO 1.0.3 | 4dc2fc2124f0 | 0x6fba |
| MU68, Pro updater, 1.0.5a | 8bf873d01511 | 0x7062 |
| NA87, Pro updater, 1.0.5a | b6c09baa2a59 | 0x705c |
| ND63 1.0.5a | 7cc1d7ba2044 | 0x704e |
| MU68 PRO 1.0.8 | 7ee63ac07556 | 0x737a |
| NA87 PRO 1.0.8 | 892583b1553b | 0x737a |
| ND63 1.0.8 | 6991a24cd129 | 0x7374 |
| MU68 PRO CYAN 1.1.4 | 87e1a59d4e2a | 0x7382 |
| ND63 PRO CYAN 1.1.4 | 9eca0192473d | 0x7374 |

## Confirmed read path

Offsets here are **protocol payload offsets**, not Windows HID report-ID offsets.
Do not implement device admission/report-ID handling solely from this document.

The official web bundle `web-index-BQvZQSA6.js` has `_It.startTravels`,
`collectData` and `createDataPacket`: requests select type 2 (travel) or 6 (ADC)
and half 1 or 2. They do not enter calibration. `toggleCalibration` is a separate
command path. `Ws` computes the payload checksum, then pads through `jo`.

Logical eight-byte requests before transport padding:

```text
5C 04 12 A6 02 01 FF FF   travel, first half
5C 04 12 A6 02 02 FF FF   travel, second half
5C 04 12 A6 06 01 FF FF   ADC, first half
5C 04 12 A6 06 02 FF FF   ADC, second half
```

NA87 PRO 1.0.8 firmware independently confirms this:

- 0x4eac checks leading 0x5c; 0x4f16 loads command byte 2.
- Jump table at file 0x14c7c contains command 0x12 -> runtime 0x974a
  (file 0x574a). Subcommand table at file 0x14d4c sends type 2 to 0x57c8,
  type 6 to 0x584e.
- 0x57c8..0x57f2 selects travel array at RAM 0x20004f6a or
  0x20004fe8: second half starts **126 bytes** after the first.
- 0x584e..0x586e selects ADC at 0x20005066 or 0x200050e4.
- Both routes call serializer 0x737a. It reads three groups of 21 unsigned
  halfwords and emits each little-endian: **63 values per request**.
- The loop has no conditional filtering of zero values or last-key selection.
  There is no calibration or persistent configuration write in these read arms.
- Serializer constructs a 132-byte logical response:
  `5C 80 92 checksum 00 type`, followed by 126 bytes of measurement data.
- Checksum routine at 0x7c00 computes
  `(0x35 + byte[0] + byte[1] + byte[2] + byte[length+3]) & 0xff`.
  The last byte is offset 131 for this response; byte 3 stores the result.

The half-selection branches were checked in every image. RAM addresses differ
by revision, but the second-half increment stays 126 bytes. The first 100 bytes
of serializer starting at its header anchor are byte-identical in all ten
images, covering the header and complete 3x21 loop. The subsequent checksum
call displacement differs in older versions.

**Conclusion:** this Pro family exposes addressed array reads, not just the
last pressed key. Two reads cover 126 slots. This does not mean all 126 are
physical keys, or that the two halves form an atomic sensor snapshot.

## Verification and limits

`verify_pro_protocol.py` passed for all ten images: pinned payload hashes,
unique serializer anchor, identical loop bytes, and XW-aware disassembly.
Synthetic logical-frame tests passed for both halves, simultaneous nonzero
values, zero release representation, indices 62/63/125 and malformed frames.
These are derived parser tests, **not firmware emulation or hardware tests**.

Still required before a production implementation:

- pin exact model identity, HID endpoint/report IDs and transport padding/
  fragmentation/reassembly; the logical response exceeds one HID report;
- derive each model's sensor-to-key map and travel units/range;
- trace sensor-array production and normal keyboard-report scheduling;
- measure update freshness/rate, USB contention, physical simultaneous presses,
  release behavior and ordinary letter input during polling.

The read path does not require calibration or an explicit keyboard-disable
command, but this is not a hardware guarantee that keyboard input never stalls.
The host also has a separate Royuan E5/FE/FF read protocol; do not confuse it
with this JingTai 5C/12 path merely because both occur in the same web bundle.

## Ordinary family boundary

ND75 is a different, ARM/M484 firmware. Its recovered SHA-256 exactly matches
the earlier analyzed image; existing protocol evidence is in
`IROK_ND75_M484_STATIC_ANALYSIS_2026-08-17.md`: host 29/18/02 subscribes using
22 column masks; device 21/01 events carry row, column and an 8-bit value.
That is not the Pro array-read protocol above. Physical multi-key delivery and
normal-input coexistence remain unverified for ND75.

There is still no exact ordinary NA87/MU68 image, nor an Ultra/Polar75 image.
Do not advertise their support or transfer another model's firmware.

## Continued trace: sensor production, ranges and transport

NA87 PRO 1.0.8, file offsets (add 0x4000 for runtime addresses):

- Producer 0xd34e receives the sensor source and a lookup table. Loop at
  0xd654..0xd674 advances 21 columns and six rows (126 slots). Missing sensor
  value 0xffff is skipped at 0xd43c; this is not a last-key-only producer.
- Working base starts at 0x20004a76 and advances two bytes per position.
  0xd4da writes ADC to base+0x5f0 (=0x20005066 for slot 0).
  0xd50a writes converted depth to base+0x4f4 (=0x20004f6a).
  These exactly match the arrays selected by the USB read handler.
- Normal computation at 0xd796..0xd7be creates a 0..1023 ratio from sensor
  calibration bounds; 0xd4fe..0xd50a looks it up. Call at 0xdeaa supplies
  runtime table 0x4d54, file 0xd54. That table has 1024 monotonic uint16
  entries, from 0 to 4000. This traces the range, not just a matching constant.
- Use `+m` in addition to `+c,+xwchc` for multiplication/division decoding;
  earlier navigation listings mark these as unknown, not absent instructions.

All ten images contain a 1024-entry monotonic conversion table at file 0xd64
(1.0.3/1.0.5) or 0xd54 (1.0.8/1.1.4). Regular NA87/MU68 and ND63 tables end at
4000; the two **Cyan tables end at 3600**, with different curves. Do not
normalize every Pro model by one blindly shared maximum. Full producer-to-table
tracing above was performed on NA87 PRO 1.0.8; table presence/range was compared
in all ten, not claimed as a full control-flow audit of every revision.

Transport queue 0x4c8e always copies 64 bytes per slot, even for the last chunk
of a logical response; queue depth is 16 slots (15 usable). A 132-byte frame
therefore consumes three slots. At 0x4cbc a full queue skips the chunk, rather
than waiting. A future backend must pace requests and reject incomplete frames.
It must trim to the declared logical length before decoding the next half;
padding must never be interpreted as extra keys. Two halves are not atomic.

ND75 now has a separately reproduced single-pending-event overwrite limitation;
see the 2026-09-13 continuation of its M484 analysis. Do not describe that older
stream as equivalent in reliability to the Pro array read mechanism.

### Cross-revision producer follow-up

Located the lookup-to-depth store in every extracted image (file offsets):
NA87 1.0.3 0xbf7c; MU68 1.0.3 0xbf8e; NA87 1.0.5 0xcc1c;
MU68 1.0.5 0xcc24; ND63 1.0.5 0xcbe6; NA87 1.0.8 0xd50a;
MU68 1.0.8 0xd50c; ND63 1.0.8 0xd4dc;
MU68 Cyan 0xd514; ND63 Cyan 0xd4dc.

Older producers use working-base+0x2fc for depth instead of +0x4f4. For
NA87 1.0.3, base 0x200048ee + 0x2fc = 0x20004bea, exactly the read handler's
first-half pointer. Its ADC is base+0x3f8. Caller 0xc94c passes lookup table
runtime 0x4d64 to producer 0xbd7a. Loop 0xc1e4..0xc204 walks 21 columns and
6 rows. Thus the older code is not inferred solely from the newer serializer.

`pro_full_listings.py` regenerates XW/M-aware listings for all ten images.
`verify_pro_producers.py` pins hashes, uniquely locates the lookup-to-store
pattern and checks all conversion tables; output `producer-evidence.json`.
This adds structural cross-revision evidence, not a whole-program proof of
normal input behavior for all ten firmware builds.


## Дополнение: этап перед аппаратной проверкой

См. [IROK_PRE_HARDWARE_STATUS_2026-09-13.md](IROK_PRE_HARDWARE_STATUS_2026-09-13.md).
Теперь проверены USB descriptors всех10 images, physical maps87/68/64,
сдвиг host indices после padding, serializers в RISC-V emulation, полный
producer NA871.0.8 и потери queue при full/busy. Подготовлен offline parser.
Ранее перечисленные static TODO частично закрыты; актуальные границы и
аппаратные критерии находятся в новом документе.
