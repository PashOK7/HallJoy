# Universal Analog Plugin for HallJoy Madlions V6 SafeHID

V6 replaces the unsafe Windows HID transport instead of masking its effects.

The old Soup path could have unfinished `ReadFile` and `WriteFile` operations
on the same HID handle at the same time. Both `OVERLAPPED` structures used
`hEvent == NULL`, and the write structure also lived on the function stack.
If the wrong operation woke the wait, the function could return while the
driver still owned a pointer into that stack. Field logs consistently located
the crashes after sending a request and before the read returned.

V6 uses a dedicated Madlions state machine that:

1. safely drains reports that were already queued;
2. starts and fully completes the write;
3. starts the response read only after the write is complete;
4. calls `CancelIoEx` on timeout and waits for final completion;
5. never reuses an `OVERLAPPED`, event, or buffer prematurely.

Reads and writes use separate persistent manual-reset events. The Madlions path
no longer performs concurrent reads and writes on one handle.

The plugin still runs only inside an isolated child process. This is a fallback
boundary for firmware, driver, and third-party-library faults; restarts should
not occur during normal operation.

See `CHANGELOG_HALLJOY_FIX.txt` for the complete change history.

## V9 high-rate pipeline

In HallJoy V9, polled devices run in independent worker threads without an
artificial delay. The analogue host is notified when a new snapshot arrives and
no longer calls the combined `read_full_buffer` on a fixed eight-millisecond
interval. The Madlions SafeHID transport remains sequential and unchanged.
