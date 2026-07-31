# New keyboard protocol worksheet

Complete this before implementing a backend.

## Device identity

- Manufacturer/model:
- Firmware versions tested:
- VID/PID candidates:
- Interface number/path marker:
- Usage page/usage:
- Input/output/feature report sizes:

## Evidence

- Source of specification or capture:
- Which bytes are proven and which are inferred:
- Real hardware owner/tester:
- Recovery method if a control transaction fails:

## Capability proof

- Candidate filter:
- Exact request bytes:
- Expected response framing:
- Length/report ID checks:
- Checksum/CRC checks:
- Echoed offset/count/key-ID checks:
- Semantic/range checks:
- Why the request is read-only or fully reversible:
- Commands explicitly forbidden at runtime:

## Analogue semantics

- Stream or polling:
- Keys per packet/request:
- Aggregate update rate:
- Raw zero/full scale and direction:
- Calibration/deadzone:
- Key/slot-to-HID map:
- Release-to-zero behavior:
- Freshness timeout:

## HallJoy integration

- Start phase:
- Descriptor flags:
- Exact point at which VID/PID ownership is claimed:
- Worker shutdown/reconnect strategy:
- Telemetry fields:
- Parser fixture files:
- Hardware validation status: untested / experimental / hardware-tested
