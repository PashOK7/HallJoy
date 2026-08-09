# HallJoy Universal Addressed Analog v4

This historical version replaced the model-specific QBZ75 backend with a
protocol backend for devices that implement the proven 64-byte `09 94 02`
request/response protocol.

## Compatibility proof

A device is never accepted by brand or VID/PID alone. It must pass both:

1. HID fingerprint:
   - usage page `0xFF60`;
   - usage `0x0061`;
   - input and output reports at least 64 bytes long.
2. Read-only capability probe:
   - valid checksum, with the 64-byte sum equal to `0xFF mod 256`;
   - `09 94 02` response;
   - exactly the requested key IDs;
   - six-byte records with plausible current analogue values.

Until proof succeeds, this backend does not claim the interface from other
HallJoy routes.

## Profiles and polling

The backend first issues read-only `09 83 00` to obtain a `key ID -> USB HID
usage` map. A sufficiently complete response creates the profile dynamically,
allowing different layouts without a model-specific source file. When the map
is absent or partial, the confirmed canonical QBZ75/AULA 84 HE map supplies only
the known family positions shared with QBZ65/QBZ75 firmware.

After proof, polling uses only addressed `94/02`, with up to nine key IDs per
request. One `98/02` at session start disables the legacy last-key diagnostic
mode. The backend does not enable `94/00`, send old `94/05` registration, depend
on digital down state, stop polling for Rapid Trigger, accept devices merely
because they use an Aula VID, or log every USB packet.

The platform-neutral `addressed_poll_scheduler.*` supports up to 255 positions.
Priority order is: bound keys; moving/pressed/recently released keys; then a
background sweep. Every packet reserves two background slots, so many bindings
span additional packets without starving the sweep.

## Coexistence and evidence

`AddressedAnalog_Start()` probes synchronously before `Backend_Init()`. The old
Aula route calls `AddressedAnalog_ClaimsDevicePath()` before opening an
FF60:0061 interface. Successful proof assigns the exact endpoint to Addressed;
rejected proof leaves it available to the Aula `0x98` route. This prevents two
reader threads from sharing one interrupt endpoint without a hardcoded PID list.

Release `DebugLog_Write` is disabled. The bounded
`HallJoyAddressedAnalog.log` is created only when an FF60:0061 candidate exists
and records identity, fingerprint, proof result, map source, key count, and
ten-second rate/loss/RTT/freshness summaries. It rotates at 2 MiB. On error, the
last 256 transactions may appear in `HallJoyAddressedAnalogTrace.log`.

## Historical limitations

- one confirmed addressed device is actively served at a time;
- unknown hardware without `0x83` must use canonical key IDs or proof fails;
- exact factory calibration was known only for QBZ75 W/A/S/D; other keys used
  conservative adaptive calibration;
- each new model still required a `probe accepted` log and multi-key movement
  evidence.

Historical build command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_ADDRESSED_ANALOG_V4.ps1
```

After testing a new model, `HallJoyAddressedAnalog.log` was the primary evidence;
`HallJoyAddressedAnalogTrace.log` was added when present. The unrelated
`HallJoyAnalogHost.log` described isolated UAP behavior, not this native route.
