# Native protocol review checklist

## Evidence and safety

- [ ] Device/firmware evidence is archived and attributed.
- [ ] Every runtime command is documented byte-for-byte.
- [ ] Flash, factory reset, calibration-write and destructive opcodes are listed and absent.
- [ ] Capability proof is read-only or fully reversible.
- [ ] Probe validates semantics, not only VID/PID or a constant prefix.
- [ ] Unknown/short/malformed/checksum-failed responses cannot claim ownership.

## Parser and mapping

- [ ] Parser is independent from HID transport where practical.
- [ ] Key/slot-to-HID map is documented and tested.
- [ ] Raw direction, zero, full scale and deadzone are proven.
- [ ] Values are clamped to HallJoy `[0,1000]`.
- [ ] Fn/vendor/unused slots are not published as standard HID keys.

## Runtime

- [ ] Worker has bounded read/write timeouts and a joinable shutdown.
- [ ] Disconnect clears values and wakes realtime.
- [ ] Every changed packet/chunk wakes the common pipeline.
- [ ] No file logging, heap churn or UI call occurs in the hot path.
- [ ] No digital HID value is converted into analogue depth.
- [ ] No interpolation/prediction is presented as a new measurement.

## Arbitration

- [ ] Exact VID/PID is claimed only after proof.
- [ ] UAP sees unconfirmed devices.
- [ ] Two native protocols cannot claim the same pair.
- [ ] Simultaneous keyboards aggregate correctly.
- [ ] Generic UI telemetry identifies the actual protocol and VID/PID.

## Release evidence

- [ ] Pure unit tests pass.
- [ ] Static routing/safety audit passes.
- [ ] Windows Release x64 build passes.
- [ ] Real hardware connect/reconnect and sustained test pass.
- [ ] Supported/tested status matches available evidence.
