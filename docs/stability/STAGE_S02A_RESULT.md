# S02A: exception barriers for realtime, debug, and overlay workers

- Date: July 30, 2026
- Source: `HallJoy_v3_9_0_STABILITY_S01_LIFECYCLE_CONTRACTS.zip`
- Status: Local implementation and portable gates complete; Windows/MSVC gate
  required before the next runtime package

## Package boundary

Only the outer C++ exception boundaries of three worker groups changed:

1. realtime loop;
2. diagnostic log writer;
3. overlay HTTP worker.

Native protocol workers, UAP exports, cooperative shutdown, and removal of
`TerminateThread` were explicitly excluded.

## Shared contract

The new header-only `worker_exception_barrier.h` adds no production object or
static initializer. It provides a fixed-size, trivially copyable
`WorkerExceptionRecord`, catches `std::exception` and unknown C++ exceptions,
creates fault records without allocation, statically requires `noexcept`
fault/completion callbacks, calls completion exactly once, and assigns a
deterministic thread exit code on fault.

## Worker-specific fail-safe behavior

### Realtime

The scheduling and tick loop are unchanged. The `noexcept` OS entry invokes the
shared barrier, while timer/MMCSS/multimedia-period resources live under
`RealtimeThreadResources` with a `noexcept` destructor. Fault closes
`g_run`, resets UI/raw/physical/button/XUSB publications to neutral, and makes a
best-effort allocation-free neutral ViGEm update. Repeated start cannot report
success for an old generation that has already faulted but has not been reaped.

### Diagnostic writer

Queue, drain, and flush behavior are unchanged. Fault closes the `g_logReady`
producer gate, requests stop, prevents further queue growth, and preserves the
old generation until its thread, event, and file are reaped. Shutdown still
releases those resources even if the fault callback already reset
`g_logReady`.

### Overlay

The accept/request loop is unchanged. Fault clears running/port publications
and closes worker-owned client/listen sockets. WSA ownership remains with the
owner and is released once by stop or startup rollback. Restart is forbidden
until the old thread handle is reaped.

## Regression evidence

- GCC and Clang common static/portable gates: PASS;
- barrier test with `-Werror`, ASan, and UBSan: PASS;
- MSVC project XML parse: PASS;
- `ClCompile` remains 58;
- new production objects: 0;
- normal realtime, logger, and overlay algorithms: byte-identical to S01.

## Known limitations and Windows gate

The C++ barrier does not contain access violations, heap corruption, stack
overflow, or `std::terminate`. Existing `TerminateThread` timeout fallbacks,
startup-generation handshakes, native backend entries, and the UAP C ABI were
deferred. Windows SDK/MSVC/ViGEm behavior was not available locally.

Before S02B, the required Windows gate was: clean x64 Release build; normal
HallJoy/ViGEm operation; repeated overlay start/stop; normal application close
with overlay enabled; and no new warning, crash dialog, freeze, or hang.
