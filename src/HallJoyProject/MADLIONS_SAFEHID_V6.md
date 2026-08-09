# HallJoy Madlions V6 SafeHID transport

## What the V5 field logs proved

An external supervisor captured eight analogue-host failures in one session.
They occurred at intervals ranging from seconds to minutes, ruling out a
Steam-startup-only trigger. Every failure had the same signature:

- exception: `0xC0000005`;
- operation: `execute`;
- address: `0x21`;
- checkpoint: `madlions_before_receive`;
- all preceding polls and snapshots were successful;
- the parent HallJoy process stayed alive and started a replacement host.

The response parser had not yet begun processing a new report. The corruption
therefore occurred between the `sendReport` and `receiveReport` boundaries.

## Old Windows HID lifetime violation

Soup opens HID with `FILE_FLAG_OVERLAPPED`. The old Madlions path:

1. started a persistent `ReadFile` with an `OVERLAPPED` whose `hEvent` was null;
2. started `WriteFile` with another stack-local `OVERLAPPED`, also with a null
   event;
3. waited for the write through the shared file-handle state;
4. then waited for the read.

With several overlapped operations on one handle, the handle state cannot
identify which operation completed. A read completion could wake the write
wait, allowing the stack-local `OVERLAPPED` to die while write I/O still owned
it. A later driver completion would then write into reused stack memory. This
matches the observed execute-at-`0x21` failures.

## V6 implementation

Madlions no longer uses the old
`discardStaleReports/sendReport/receiveReport` combination. Each transaction is
strictly sequential:

```text
drain stale input
  -> cancel and reap any pending probe
write request
  -> persistent OVERLAPPED + dedicated manual-reset event
  -> complete or cancel-and-reap
read response
  -> persistent OVERLAPPED + dedicated manual-reset event
  -> complete or cancel-and-reap
validate length, layout, and travel
publish
```

A read and write are never pending simultaneously. Context objects and buffers
remain at stable addresses until terminal I/O completion. After timeout, no
object is destroyed or reused until Windows reports final completion.

## Errors, reconnect, and isolation

Win32 transport errors are published to shared diagnostic state and crash
evidence. Eight consecutive failed responses move the device to disconnected
state. The plugin returns a controlled error, the host immediately publishes a
neutral snapshot and exits, and the supervisor starts a fresh process that
enumerates HID again and opens a new handle.

A normal PnP error uses this controlled reconnect path and does not create a
crash dump.

Process isolation remains an independent final containment boundary. Correcting
the transport removes the known corruption mechanism; isolation protects the
main application from unrelated firmware, driver, or third-party defects and
does not substitute for the transport correction.
