# S07 / V14-12C: bounded Hex80 lifecycle

- Date: August 1, 2026
- Status: `Implemented / Hex80 hardware gate deferred`

## Result

The Hex80 worker no longer relies on unbounded `std::thread::join`. Each
generation owns a waitable `_beginthreadex` handle, serialized start/stop state,
and one three-second deadline. Stop publishes the cooperative flag, wakes the
event, and requests cancellation of active HID I/O. The HID handle remains
owned by the worker and closes only after terminal completion is reaped.

After every polling request, the worker checks stop before decoding or
publishing. On timeout, generation resources are deliberately retained, the
registry receives `TimedOut`, restart is forbidden, dependent teardown is
skipped, and the process exits through containment code 2.

## Regression evidence

- Hex80 protocol command builders, response parser, and GET-only routing in the
  protocol `.cpp/.h` files are unchanged;
- 40/40 static audits and 26/26 portable C++ tests: PASS;
- Hex80 translation unit under MSVC 19.44 `/W4 /WX`: PASS;
- simulator-only stop timeout confirmed retained ownership and containment;
- official `BUILD.cmd`: zero errors and no unexpected warnings;
- 15-second Irok MG75 Max smoke: 57,276/57,276 successful SparkLink requests,
  clean shutdown, and no changes to any of the 11 user files.

## Limitation

No physical Hex80 was available, so device input, full-matrix publication,
unplug/reconnect, and device-specific shutdown remain unverified. The next
separate package was the MAD68 lifecycle migration.

Evidence: `tests/V14-12C_S07_HEX80_LIFECYCLE_2026-08-01.txt`.
