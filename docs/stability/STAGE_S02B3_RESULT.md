# S02B.3: Addressed main/reader exception boundaries

- Date: July 30, 2026
- Status: `Implemented / Addressed device gate deferred; Windows and SparkLink
  regression gate pending`

## Objective and scope

Contain C++ exceptions at both Addressed streaming boundaries—the main worker
and nested HID reader—without changing the 09/94/02 protocol, polling
scheduler, overlapped-I/O state machine, or existing shutdown fallback. Only
`src/HallJoyProject/HallJoy/addressed_analog_backend.cpp` changed.

The package added independent `noexcept` entries through
`RunWorkerEntryBarrier`, fixed fault records, neutral publication on fault,
mandatory reader completion signaling, RAII registration of the reader handle,
a protected session block that joins the reader before rethrowing, cleanup of
stack-backed scheduler/pending-response state, owner-side reaping of a completed
main thread before replacement, and startup rejection when the worker exits
before `AddressedAnalog_Start()` returns.

`ResetPublished()` became `noexcept`; its best-effort realtime notification
cannot escape from a fault callback.

## Deliberately unchanged

- command and packet layouts for `0x09/0x94/0x02` and `0x98/0x02`;
- checksum and response parsing;
- scheduler classes, deadlines, polling cadence, and recovery thresholds;
- `HidIoOperation` and the stack-backed `OVERLAPPED` state machine;
- cross-thread `ForceCloseReaderHandle()` fallback and reader wait constants;
- routing, claims, and calibration.

Cross-thread forced closure of active overlapped I/O remained assigned to S06.
S02B.3 only ensured that a C++ exception could not bypass owner cleanup through
`std::terminate`.

## Hot-path proof

```text
Addressed session polling loop: EQUAL
SHA-256: e187cbd4657f969c9911be4b5fef0676ac1a0ef391b2b96a09585bd43a1653a6
Addressed reader I/O loop: EQUAL
SHA-256: 2b9171b926230674ab334e7bdd2d98d5b1ef98747fbca39f42bfc7ed29a973ca
MakePacket: EQUAL
SHA-256: 85c081a6a2c6959540cfad6dc051c638a7419511af2b0d2a205c0399077f2f45
MakePollPacket: EQUAL
SHA-256: 0ab9e6c46fa4970281379ec7bb6c49accba1aff2ec817aa143d3c041c2fcc6d8
```

Fourteen static audits, nine portable C++ tests, GCC/Clang, ASan/UBSan with
strict warnings, MSVC project XML parsing, the targeted boundary audit,
hot-path comparison, self-excluding package manifest, and a repeated clean
unpack gate all passed. Physical Addressed validation remained deferred; the
shared Windows/ViGEm and available SparkLink regression was required before the
next runtime package.
