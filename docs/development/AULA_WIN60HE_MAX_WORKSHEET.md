# Aula WIN 60 HE MAX protocol worksheet

## Device identity

- Manufacturer/model: Aula WIN 60 HE MAX, SparkPlayJoy RM6x21 platform.
- Firmware versions tested: physical `App V1.1.6` with the exact 60-byte sync
  descriptor recorded in `HallJoy (3).log`.
- VID/PID candidates: exact `1CA2:1902` only.
- Interface number/path marker: exact SetupAPI path retained; no substring or
  interface-number assumption.
- Usage page/usage: exact `FFA0:0001`.
- Input/output/feature report sizes: input/output exactly 65 Win32 bytes
  (`00` report ID plus 64 protocol bytes); no feature-report dependency.

## Evidence

- Source: reproducible proof archive, firmware binary/verifier, exact npm
  sources and three complete physical proof sessions.
- Proven: identity, framing, checksum, read requests, response correlation,
  default/active maps, travel layout, scaling and firmware RAM addresses.
- Physically proven: exact HID identity/envelope, exclusive ownership, sync,
  precision, default map, two stable active-map generations and travel frames.
- Still pending: non-zero runtime travel and physical reconnect behavior.
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
- Semantic/range checks: exact physical firmware descriptors, 10/10/3400 micrometres,
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
- Hardware status: `physical-protocol-validated / runtime-travel-pending`.

## V14-12V family generalization

- Exact known profile: WIN 60 HE MAX `1CA2:1902`, 61 positions, 17 reads.
- Compatible profile: brand-scoped Aula/SparkPlayJoy identity, fixed
  `FFA0:0001`/65-byte transport, structural 60-byte sync, plausible dynamic
  precision/travel and unique 6x21 default map.
- Active Fn0 batching: derived from physical positions, up to nine packets per
  generation; two generations must match.
- Claim boundary: full exclusive-session proof on the exact path; no arbitrary
  HID scan and no coarse VID/PID ownership.
- Validation: alternate 84-position synthetic profile passes ASan/UBSan and the
  full portable suite. Only WIN 60 HE MAX has physical evidence.
