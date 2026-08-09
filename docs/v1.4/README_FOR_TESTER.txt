HallJoy v1.4 integration build
==============================

Aula WIN60 HE at 1CA2:1902 with App V1.1.6 was physically validated. Other
Aula/SparkPlayJoy 6x21 profiles are accepted only after complete brand-bounded
structural protocol proof; they are not described as physically tested.

Run HallJoy.exe normally.

Release behavior
----------------

- modified settings and profiles are stored under `%LOCALAPPDATA%\HallJoy` by
  default;
- legacy settings beside the executable are copied on first start, with a
  backup under `MigrationBackups`; the originals are retained;
- create a regular `HallJoy.portable` file beside HallJoy.exe to opt into
  portable storage;
- only genuine analogue values drive gamepad controls; green digital keyboard
  indicators are UI-only;
- live Configuration and Gamepad Tester information supports MAD68 Pro R,
  ATK/QK Hex80, Addressed 09/94/02, Aula WIN60 HE, SparkLink, SayoDevice, and
  UAP/Wooting-compatible routes;
- any VID 373B Hex80-compatible PID enters native 0x96 only after two valid GET
  responses, without conflicting with UAP, Addressed, or native A0;
- Addressed 09/94/02 requires the exact FF60:0061/64-byte fingerprint and valid
  checksummed responses, allowing compatible QBZ/IPI hardware independent of
  VID/PID;
- unknown Sayo PIDs require a valid deep-protocol response;
- MADLIONS MAD68-family devices enter native A0 only after strict fingerprint
  and acknowledgement proof; other MADLIONS devices remain with UAP;
- Aula/SparkPlayJoy requires FFA0:0001, 65-byte HID reports, and complete
  dynamic read-only proof; additional proven profiles remain
  protocol-compatible rather than physically tested;
- realtime work wakes through `WaitOnAddress`/`WakeByAddress`;
- production creates no continuous trace, per-key, latency, host, or diagnostic
  log and starts no log-writer thread;
- `HallJoyCrash.txt` is created only after an unhandled fatal exception;
- shutdown performs quiet best-effort A9 recovery for MAD68.

The MAD68 stock firmware, A0 format, and telemetry frequency are unchanged.
Keep firmware tools and vendor configurators closed while HallJoy owns the
MAD68 vendor interface.
