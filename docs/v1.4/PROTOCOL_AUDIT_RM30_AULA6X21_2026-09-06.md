# RM-30-AULA6X21 — Aula WIN 60 HE MAX / SparkPlayJoy 6×21 protocol review

Date: 2026-09-06

## Admission and wire contract

This native route is a separately proven `5C` protocol, not the W669 `0D/18/21`
family. It admits the physical Aula WIN 60 HE MAX identity (`1CA2:1902`,
`FFA0:0001`) and the small registry of named, board-correlated 6×21 identities.
A wider compatible candidate is not claimed from a name or PID alone: it must
complete the bounded, read-only structural proof on its exact HID path.

| Operation | Request | Required response/proof |
|---|---|---|
| Synchronize identity | `5C 01` | structurally valid firmware descriptor, board/USB correlation and bounded serial handling |
| Read API capability | `5C 00 25` | valid command framing before matrix use |
| Read travel matrix | `5C 12 02` | complete, bounded 6×21 values within the proved precision range |
| Read active Fn0 state | `5C 12 03` | two matching generations before the map is trusted |
| Read key functions/default map | `5C 23` / `5C 2B` | unique positions, valid semantic functions and no more than 126 positions |

The HID envelope is exactly 65 bytes on Windows (one leading report-ID byte plus
the 64-byte wire report). A transaction accepts no more than three correlated
response reports. The route has no production mutating builder: it does not
flash firmware or alter calibration, maps, lighting or profiles.

## Parser, ownership and lifecycle contract

- The matrix is fixed at 6×21. Values are converted only after the full parsed
  snapshot satisfies the current travel and precision bounds; malformed,
  partial, stale or semantically contradictory results are not published.
- The known MAX precision contract is 10 µm resolution and 10–3400 µm travel.
  A structurally compatible sibling may pass its own positive bounded proof,
  but is reported as protocol-compatible rather than inheriting MAX physical
  status.
- Enumeration, path, instance and meaningful firmware serial evidence are
  retained after proof. Contradictory candidates, multiple candidates, identity
  drift and a busy vendor interface fail closed rather than silently rebinding.
- The backend publishes only its own current HID values, clears ownership on
  failure/removal, cancels active I/O before the bounded worker join, and keeps
  the pre-UAP routing claim separate from legacy Spark handling.

## Verification and remaining evidence

`aula_win60he_protocol_test.cpp` covers exact identities, board correlation,
wire-envelope normalization, capability parsing, matrix/map bounds, response
correlation and a 50,000-input malformed-frame corpus. The accompanying
backend audit covers source-level admission, exclusive-session, routing,
publication and shutdown contracts. The physical WIN 60 HE MAX route already
has analogue, rollover, release and reconnect evidence; listed sibling models
remain separately qualified by the rules in `SUPPORTED_HARDWARE.md`.

No live HID command or HallJoy executable was run during this review.
