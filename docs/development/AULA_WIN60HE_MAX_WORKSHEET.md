# Aula WIN 60 HE MAX protocol worksheet

## Device identity

- Manufacturer/model: Aula WIN 60 HE MAX, SparkPlayJoy RM6x21 platform.
- Firmware versions tested: firmware image `App V1.1.6`, build `Feb  4 2026`;
  no physical hardware test.
- VID/PID candidates: exact `1CA2:1902` only.
- Interface number/path marker: exact SetupAPI path retained; no substring or
  interface-number assumption.
- Usage page/usage: exact `FFA0:0001`.
- Input/output/feature report sizes: input/output exactly 65 Win32 bytes
  (`00` report ID plus 64 protocol bytes); no feature-report dependency.

## Evidence

- Source: reproducible proof archive, firmware binary, firmware verifier,
  captured oracle JSON and exact npm package sources.
- Proven: identity, framing, checksum, read requests, response correlation,
  default/active maps, travel layout, scaling and firmware RAM addresses.
- Inferred: real Windows HID timing and physical reconnect behavior remain
  unverified because no Aula device is available.
- Real hardware owner/tester: unavailable.
- Recovery: any transaction uncertainty poisons the exclusive session; close,
  re-enumerate and repeat the complete proof on the retained exact identity.

## Capability proof

- Candidate filter: `1CA2:1902`, `FFA0:0001`, exact 65-byte input/output.
- Exact requests: sync, precision, three default row pairs, two five-packet Fn0
  generations, travel halves 1 and 2; see the protocol document.
- Response framing: `5C length (command|80) checksum payload`.
- Length/report ID: exact frame length and response report count; Win32 byte
  zero must be zero.
- Checksum: low byte of `35 + 5C + length + command + last payload byte`.
- Correlation: command, selector, rows, requested key and layout are exact.
- Semantic/range checks: firmware/version/date, 10/10/3400 micrometres,
  canonical 61-position map, publishable 16-bit functions and plausible travel.
- Read-only/reversible: all 17 production proof transactions are reads.
- Forbidden: calibration, key writes, profile writes, firmware update and any
  undocumented opcode.

## Analogue semantics

- Stream or polling: polling, two sequential travel requests per snapshot.
- Keys per request: 63 travel values; 126 per complete matrix.
- Aggregate update rate: transport-dependent; no physical rate claimed.
- Raw zero/full scale: 0..3400 micrometres, increasing with travel.
- Calibration/deadzone: no calibration command; normalize against proven max.
- Map: canonical 6x21 physical map plus two-generation active Fn0 functions.
- Release: zero travel publishes zero; disconnect clears all values.
- Freshness: current exclusive session only; any uncertain transaction destroys
  the session rather than publishing stale or mis-correlated data.

## HallJoy integration

- Start phase: `BeforeUap`.
- Flags: `PolledTransport | ReadOnlyProbe`.
- Ownership: normalized exact SetupAPI interface path.
- Claim point: after exclusive open, post-open identity/caps correlation and the
  complete 17-transaction proof.
- Pre-open guard: reject foreign exact claims before metadata `CreateFileW`;
  reconnect accepts only the retained identity and protocol claim.
- Shutdown/reconnect: `CancelIoEx`, bounded 3000 ms generation join, retain
  resources on unconfirmed completion, no force termination.
- Telemetry: candidate/protocol/connected, exact USB/caps, report sizes,
  precision/max travel, mapped/active keys, updates/failures/intervals.
- Fixtures: protocol, official oracle, end-to-end transport and session-policy
  suites under `src/HallJoyProject/tests`.
- Hardware status: `firmware-proven / implementation-tested /
  hardware-unvalidated`.
