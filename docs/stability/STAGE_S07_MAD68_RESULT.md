# S07 / V14-12D: bounded MAD68 lifecycle

- Date: August 1, 2026
- Status: `Implemented / MAD68 hardware gate deferred`

## Result

The MAD68 worker moved from `std::thread` to a waitable `_beginthreadex`
generation. The owner publishes stop, wakes the event, and cancels the
persistent overlapped read without closing the HID handle. The session worker
unregisters itself, reaps pending I/O, and closes its read, write, and control
handles.

MAD68 requires special shutdown semantics: a new A8 command is forbidden after
stop, while the final direct A9 recovery remains mandatory. Reads completing
concurrently with stop cannot reach decoding or publication. The external join
is bounded to 3000 ms; timeout retains generation resources, blocks restart,
and selects process containment.

## Regression evidence

- the protocol `.cpp/.h`, command sending, interrupt/control transports,
  `BestEffortRestore`, `RunStrategy`, and `ProcessPayload` are unchanged;
- 41/41 static audits and 26/26 portable C++ tests: PASS;
- MAD68 translation unit under MSVC 19.44 `/W4 /WX`: PASS;
- simulator timeout confirmed retained ownership and exit code 2;
- official build and a 15-second Irok smoke passed;
- all 11 user files remained hash-identical.

## Limitation

Without a physical MAD68, the A8/A9 transition, full-matrix publication,
hotplug, and real stuck-driver shutdown remain unverified. The S07 code work is
complete, but device-owner qualification remains a release gate.

Evidence: `tests/V14-12D_S07_MAD68_LIFECYCLE_2026-08-01.txt`.
