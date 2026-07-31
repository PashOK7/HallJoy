## New analogue protocol

### Device and evidence

- Manufacturer/model:
- Firmware versions:
- VID/PID and HID fingerprint:
- Specification/capture source:

### Safety proof

- Capability request(s):
- Response invariants validated:
- Read-only or reversible rationale:
- Forbidden/destructive commands audited:

### Implementation

- [ ] Generated/structured as a standalone protocol + backend module
- [ ] One descriptor catalog entry; no lifecycle/read/UI special case
- [ ] Pure parser/builder tests include malformed fixtures
- [ ] Exact VID/PID is claimed only after semantic proof
- [ ] Disconnect clears analogue values and wakes realtime
- [ ] No digital-to-analogue fallback, interpolation or prediction
- [ ] Production hot path performs no file logging

### Validation

- [ ] `python tools/run_native_backend_checks.py --require-compiler`
- [ ] Windows Release x64 `BUILD.cmd`
- [ ] UAP/native simultaneous-device test
- [ ] Connect/disconnect/reconnect test
- [ ] Sustained hardware test and logs/measurements attached

### Support status

- [ ] Documentation-only/untested
- [ ] Experimental hardware validation
- [ ] Hardware-tested
