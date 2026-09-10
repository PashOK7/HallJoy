# RM-04 — SparkLink row freshness review

Date: 2026-09-06. Scope: independently polled SparkLink matrix rows.

## Evidence and root cause

The poller records `g_sparkRowLastOkMs[row]`, but the published value is only
`g_sparkAnalogMilli[hid]`. `BackendNative_SparkGetMilli` returned that cached
per-HID value without a row freshness check. A successful query for row A resets
the worker failure streak even while row B keeps failing, so B's nonzero value
could remain published forever.

The old-bug oracle has two rows: A remains fresh and B times out after publishing
a nonzero value. B must become neutral after its deadline while A stays live.
Never-seen rows are not fresh. A duplicate HID on A and B must retain A's fresh
value when B expires.

## Options considered

| Option | Assessment |
|---|---|
| Check row age only in `GetMilli` | Rejected: realtime would scan mutable route state and no input wake would occur at expiry. |
| Zero every HID in a stale row | Rejected: a duplicate HID can still be owned by a fresh row. |
| Store row-local normalized values and recompute only affected HIDs from fresh rows | Chosen: row ownership, freshness and the aggregate are explicit; the realtime getter remains one atomic load. |
| Restart the whole keyboard after one row failure | Rejected: destroys valid input from unrelated rows and changes existing recovery policy. |

## Chosen contract

Each discovered row owns a bounded per-HID value slot. A successful row commits
its values and freshness together; expiry, RowLimit reduction and disconnect
remove that row from aggregation. For each affected HID the worker publishes the
maximum among active, in-limit, fresh row owners, then wakes realtime only if
the aggregate changed. A late response from a retired generation cannot restore
values because state is reset before a new reader generation starts.

The initial deadline is the worst bounded complete safe-mode route round:
eight rows times `(250 ms transaction timeout + 20 ms maximum safe poll sleep)`
= 2160 ms. It avoids falsely expiring a healthy last row under the documented
per-transaction and settings bounds, while still bounding a failed row whose
neighbours remain successful.

The dead `if (false)` burst code is not enabled as part of this correctness
change. It remains a separate, measured protocol experiment and must not alter
the error/freshness contract silently.
