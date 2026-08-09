# S02B.2: SparkLink C++/SEH exception boundary

- Date: July 30, 2026
- Final status: `Verified on SparkLink`

## Objective and scope

Contain C++ exceptions at the SparkLink worker boundary and make every abnormal
exit fail-safe while preserving the existing SEH protection, HID protocol,
polling loop, and performance settings. Only
`src/HallJoyProject/HallJoy/backend_sparklink.inc` changed.

The package added a separate `noexcept` C++ entry using
`RunWorkerEntryBarrier`, allocation-free `WorkerExceptionRecord` storage, a
separate atomic native SEH code, one idempotent
`SparkReleasePublishedInput()`, neutralization of all analogue publications on
every worker exit, immediate realtime wake after neutralization, closure of the
write-capable state on fault, a `g_sparkWorkerExited` completion publication,
and early-exit checks both before and after `connected=true` during startup.

That final check closes a race in which the worker could exit during
`SparkStart()` and startup could then overwrite the published disconnect with
`connected=true`.

## Deliberately unchanged

- vendor commands and packet layout;
- device-info/layout discovery;
- row routing and normalization;
- `SparkPollMode` and row limit;
- polling/yield policy and transaction cadence;
- VID/PID claim policy and HID I/O;
- the `TerminateThread` fallback in `SparkStop()`, deferred to S08.

The extracted polling loop remained symbolically identical to S02B.1:

```text
SPARK_POLL_LOOP_EQUAL=TRUE
SHA-256=6ff9632ed43a492fc582c6578936c18cba0556b35065cb9b137257eede7c8954
```

## Validation

The targeted SparkLink audit, all project static audits, all portable C++
tests, GCC and Clang gates, ASan/UBSan, MSVC project XML parse, and hot-path
comparison passed locally.

A clean Windows/MSVC build and physical SparkLink smoke then confirmed analogue
input, ViGEm, Safe/Fast Yield/Max Burst modes, row limit, hotplug/reconnect,
neutral release, restart, and shutdown without regression. MAD68/Hex80
device-specific validation remained deferred because their production files did
not change in this package.
