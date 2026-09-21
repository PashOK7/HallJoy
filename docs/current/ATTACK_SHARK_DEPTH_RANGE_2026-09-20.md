> Follow-up: [bank initialization audit](ATTACK_SHARK_BANK_INIT_2026-09-20.md) resolves X82 application decompression (seven730-ceiling tables) and the X68 switch selector. Unresolved statements below describe the earlier audit; see the follow-up for current findings. Runtime scaling is unchanged.

# ATTACK SHARK depth range audit — 2026-09-20

## Result and runtime decision

The shared 3.5 mm HallJoy endpoint is supported by additional firmware evidence
for exact X65 HE dev2268 v309 lookup tables and X68 MAX dev2755 v504 bank0.
It is not proved for every revision/switch bank. Do not replace it globally with
the official UI's 3.3 mm actuation-setting maximum, nor infer endpoint from the
largest recent key press. No runtime protocol/normalization or EXE changes were
made in this audit. Remap readback remains deferred by owner.

## Units versus endpoint

Pinned RY5088 client `f9b6af43.js` uses RF version when nonzero, otherwise USB:
versions below0300 use10 units/mm,0300–04FF use100,0500+ use200. The live E5 FE
reply is a raw little-endian table of already calculated travel. Settings pages
E5 00/01/06 expose actuation/release/dead-zone settings, not measured full travel.
E5 FC is described by the client as switch-type selection, not an endpoint.
The client reads FC separately per key; the existence of that read is not proof
that every firmware bank is selectable, or that its index maps directly to the
scanner's active bank without a conversion.

The client `index.2e5bd916.js` selected-key maximum getter uses model metadata,
4 mm for older versions, or a switch helper currently returning3.3 mm. Its callers
include the press/release setting controls. These settings limits are not proof
of the raw live-depth saturation value. The exact files are already pinned by
the family and layout research.

## Firmware conversion evidence

Both reviewed flash-table scanners compute a normalized sensor index from live
sensor values and stored sensor boundaries, then select a bank of 2048 uint16
values. Index2047 stores the valid-index bound; index2046 stores the saturation
value. The lookup uses the selected table entry until the index bound is
exceeded, then returns the saturation value. This is firmware-internal sensor
normalization, not HallJoy enabling a calibration mode.

| Exact firmware | Lookup banks | Raw saturation | Interpretation with version units |
|---|---|---:|---|
| X65 HE dev2268 v309 |0–5 |350 |3.5 mm at100 units/mm |
| X68 MAX dev2755 v504 |0 |700 |3.5 mm at200 units/mm |
| X68 MAX dev2755 v504 |1–4 |350 |1.75 mm numerically at200 units/mm; active selection/use is unresolved |
| X82 Pro HE dev2935 v503 |RAM-backed |Not established |Do not substitute injected test depths for a measured/generated endpoint |

X65 tables begin at080147A8 with4096-byte stride. Bank0 index bound is2000;
banks1–5 use2020. The actual lookup block0800CDEC–0800CE30 was executed for
all2050 input indices0–2049 in each bank.

X68 MAX tables begin at08017444 with the same stride. Bank0 bound2030;
banks1–4 bound1980. Actual lookup block0800DF98–0800DFD2 was executed for all
2050 indices in each bank. The additional zero-filled area was not classified
as a usable switch bank.

A conditional scaling risk is now concrete: if X68 MAX really publishes a
350-capped bank without later rescaling, HallJoy's current700 endpoint reaches
only50%. This audit does NOT establish which nonzero banks a real keyboard can
select, whether they are unused legacy data, or whether firmware performs any
other bank-selection/unit adjustment. Do not ship a blanket factor-of-two fix.

X82 Pro v503 references a table base at2000000C (literal at0800E468), rather than
the flash arrays used by the other images. Its startup scatter/decompression
stage alone did not materialize usable monotonic tables there. The initialization
source and final contents need further tracing. Previous independent-depth tests
injected already calculated depths, proving publication and reads, not full-range
conversion. They must not be cited as evidence of a700 endpoint.

## Verification and remaining work

`python tools/review_attackshark_depth_range_20260920.py` passes22,550 component
cases, comparing the executed lookup block against the pinned table for every
index and bank. Firmware SHA256s are asserted. No HID, physical keyboard,
calibration commands, remap reads or firmware writes are involved.

Machine-readable results: [depth-range evidence](../research/attackshark-depth-range-20260920.json).
Firmware acquisition and provenance: [family research](../research/ATTACK_SHARK_FAMILY_2026-09-20.md).

Next bounded investigations:
1. Trace the X68 MAX active bank selector back to switch configuration and prove
   whether FC can identify its effective bank. If successful, check exact-version
   per-key scaling without changing keyboard settings.
2. Trace X82 Pro's RAM-table initialization, instead of guessing from UI settings.
3. Keep all other firmware revisions explicitly unverified; a shared command
   protocol does not prove identical saturation tables.

The current ordinary EXE is unchanged from the family-layout delivery:
SHA256 `26c4ff4733fc258ad721abfde99d6709f49d54fd1f5f1a9f04e8e4025c6a5cd4`.
