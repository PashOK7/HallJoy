# Testing a new native protocol

## One-command source checks

```text
python tools/run_native_backend_checks.py --require-compiler
```

This validates MSBuild XML, runs every static safety/routing audit, and compiles all
portable `*_protocol_test.cpp` files plus the shared scheduler/lifecycle/contract
tests. A generated protocol test is discovered automatically.


## Pure tests

Keep packet builders/parsers free of Win32 handles. Test:

- minimum and maximum valid values;
- all offsets/counts/slot IDs;
- short reports and wrong report IDs;
- checksum/CRC corruption;
- duplicate, missing and unexpected requested IDs;
- direction and normalization;
- release to exact zero.

## Routing tests

A static or mock-HID test must prove:

- no claim from fingerprint alone;
- no claim from a response belonging to another HallJoy protocol;
- exact claim after full capability proof;
- claimed device is excluded before UAP `CreateFileW`;
- failed probe leaves the device to UAP.

## Runtime tests

Record at least:

- update rate and worst transaction time;
- sustained errors/timeouts;
- disconnect and reconnect;
- multiple active keys;
- simultaneous second analogue keyboard;
- UI telemetry and Gamepad Tester output;
- no continuous production logs.

## Hardware status labels

- `implemented`: code/spec only, no physical device test;
- `experimental`: one or more physical tests, incomplete coverage;
- `tested`: sustained test on named firmware and reconnect/multi-key coverage.
