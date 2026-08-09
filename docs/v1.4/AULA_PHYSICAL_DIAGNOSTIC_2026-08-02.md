# Aula WIN60 HE physical diagnostic record

Initial investigation date: August 2, 2026

## Initial evidence and routing correction

`HallJoyStabilityTrace (1).log`, SHA-256
`BA14561242740E0985F3A12EDE325DA926436008E9C76FD1AA2815279018CFED`,
contains 47 complete records over 14.9 seconds but no final `session.end`. It is
a live-process snapshot, not lifecycle evidence.

The log proved one backend defect: SparkLink opened the exact Aula
`1CA2:1902 / FFA0` interface nine times and failed nine protocol probes,
beginning immediately after the Aula worker started and repeating roughly every
two seconds. Correct firmware and a closed web driver did not prevent this.
Because production `DebugLog` was disabled, the log did not expose Aula's first
failure, so Spark could not yet be claimed as its sole cause.

SparkLink was changed to reject the dedicated Aula VID/PID before `CreateFileW`.
This is not a broad UAP reservation: only a fully proven Aula interface enters
the claim registry; failed strict proof leaves the path available to UAP. The
foreign Spark probe loop is impossible after this gate.

## Single-executable aggressive diagnostic

The diagnostic package contained only `build/aula-diagnostic/HallJoy.exe`. Each
launch automatically created one bounded `HallJoy.log` beside itself; testers
needed no BAT, PowerShell, Python, collector, previous-log pair, or portable
marker.

The log included enumeration counts and hashed identity, each metadata/open/
re-correlation/queue failure stage, every read-only Aula transaction and final
TX/RX pair, decoded firmware/precision/map/travel evidence, pre-UAP ownership,
first matrix publication, and runtime mismatch. Serial bytes were sanitized and
raw HID paths were never recorded.

Diagnostic proof continues through later read-only stages after a semantic
mismatch, but an imperfect proof never claims the path or publishes analogue
values. Timeout, malformed/correlation failure, or loss of the exclusive
session forces close/reopen because this protocol has no transaction ID.

Tester procedure was: close every vendor/web configurator; place the executable
in a normal folder; press analogue keys for 20-30 seconds; close HallJoy; return
the single generated `HallJoy.log` unchanged.

ASan/UBSan, Aula/routing/exact-claim static audits, regular and diagnostic
Release builds, the complete native gate, and the single-file package/binary
audit passed. High-detail markers were absent from production.

## Logs 1 and 2: ownership and sync-envelope root cause

`HallJoy (1).log` (37,904 bytes; SHA-256
`E01FC3176933C7BB32DD72AABC07F4D6364CD344D217ABBCA6C58570BDBD1B5E`)
ended cleanly, but all nine attempts failed at the first TX after exclusive open
with Win32 32 (`ERROR_SHARING_VIOLATION`). That was a temporary external owner,
not grounds for weakening interface ownership.

`HallJoy (2).log` (325,015 bytes; SHA-256
`9103611B57190C541A15136CD7FF98DBBF9D0D5AC3AE8689CE1115A7D919A006`)
recorded 62 successful exclusive opens and the same sync reply 62 times. Spark
rejected the Aula ID before opening. The remaining blocker was HallJoy's parser:
the keyboard returned a valid single-report `5C 3C 81 4D ...` frame with a
60-byte payload, while the hardware-free oracle required exactly 54 bytes.
Command `0x81`, length, report count, and checksum `0x4D` were correct; the old
`unexpected-first-report` result was HallJoy's contract error.

The diagnostic parser was broadened to inspect the proven 60-byte envelope but
still withheld claims until every physical semantic was established. Production
remained on the older oracle contract at that intermediate point.

## Log 3: complete protocol proof

`HallJoy (3).log` (66,147 bytes, 223 lines; SHA-256
`30FFE7CFB512F9FCE5988D71FF38D2F58922957DCEE7B675CA5113E7A7979DAB`)
contains a complete clean lifecycle. All three exclusive sessions completed the
17 read-only transactions without transport or decode error:

- stable physical 60-byte sync descriptor;
- precision/minimum/maximum: `10 / 10 / 3400` micrometers;
- 61 physical positions and 60 published HID usages;
- two identical full Fn0-map generations, 60 active mappings;
- both travel halves with valid 128-byte payloads across three HID reports.

Travel was zero because proof completed before the tester pressed a key, and
runtime polling was still blocked by the diagnostic-only firmware mismatch.
This log therefore proved the channel and format, not a nonzero stroke.

The final mismatch came from treating binary sync data as old text fields. The
physical structure contains three 16-byte descriptor blocks with `0x10` markers
and a `0xFF` trailer; `Feb  4 6320` was an accidental rendering of binary build
descriptor bytes. Production moved to the exact physical 60-byte contract while
keeping serial data device-specific.

## Logs 4 and 5: first physical analogue observation

The tester reported visible analogue travel. Both launches completed strict
proof with `mismatch_mask=00000000`, claimed the route, connected, and published
the first matrix with 60 mapped HID usages.

`HallJoy (4).log` (1,840,992 bytes, 9,311 lines; SHA-256
`8529C724EDA13F93892237B11C5012D85FA8CD36A38BC71D85B64EF4BAC7E52C`)
contains about 58 minutes of stable polling. At the end the device disappeared;
one failed read was followed by 59 clean searches with no candidate before
HallJoy closed. This proves disconnect/retry, not reconnect.

`HallJoy (5).log` (629,044 bytes, 3,015 lines; SHA-256
`5C1FC4F0DFC1AE152EA395463CD458C6123F08A27F783C05E2B8E9B8EDFF2A48`)
contains about 17.5 minutes of stable polling. Its only continuation-read failure
coincides with normal shutdown and represents cancellation of pending I/O.

The raw-report cap had already been consumed by startup proof, so later nonzero
presses were not retained byte-for-byte. Physical input was accepted from the
tester's observation plus proven claim, connection, and runtime polling, but the
documents do not pretend these two files contain raw nonzero samples. Complete
disconnect/reconnect was still open.

## Diagnostic schema v2

Logs 4 and 5 exposed weaknesses in the diagnostic design: raw capacity was
spent on startup, repeated Spark skips flooded the file, and normal
`CancelIoEx` shutdown looked like a transport warning. Schema v2 corrected the
architecture instead of merely increasing the limit:

- `matrix.health` every five seconds: matrix rate and min/average/max interval,
  paired-transaction duration, and seven latency buckets through `>100 ms`;
- activity distribution across 0, 1, 2-4, 5-9, and 10+ simultaneous keys;
- `matrix.activity` on first nonzero, each new concurrency record, and first
  ten-key state, including HID, row/column, and micrometer travel;
- final `matrix.coverage` with maximum travel per actually pressed usage;
- session summary, disconnect duration, and reconnect/re-proof outcome;
- expected shutdown cancellation recorded as INFO;
- one Spark skip summary per minute rather than thousands of repeated records.

The required run lasted at least three minutes, included slow and rapid strokes,
a ten-key hold followed by complete release, a ten-second physical disconnect
and reconnect without closing HallJoy, renewed input, and normal shutdown. One
run could then prove frequency/latency, rollover, release-to-zero, per-key range,
disconnect/reconnect, repeated proof, and clean shutdown. Disconnect evidence
was written only after authoritative neutralization and recorded active counts
before and after clearing.

## Log 7: stable frequency and multi-key evidence

`HallJoy (7).log` (142,904 bytes, 417 lines; SHA-256
`EBDDF2DCEA3D72BBCA1E6219A340312A0BB55167826F6BC2187FC41079B968A9`)
ran for 61.703 seconds and exited cleanly. Strict pre-UAP proof passed,
60 usages were mapped, and 21,027 complete matrices arrived over 61.129 seconds
without one failed update.

- lifetime rate: `343.973 Hz`;
- twelve five-second windows: `340.245-346.178 Hz`;
- matrix interval min/average/max: `2138/2907/15812 us`;
- paired travel transactions min/average/max: `1677/2076/2644 us`;
- every transaction completed within 4 ms;
- maximum simultaneous active keys: 22; 2,654 frames with 10+;
- eight zero-to-pressed transitions and eight complete releases;
- 40 distinct nonzero HIDs, reaching the confirmed `3400 um` maximum;
- no runtime protocol error, semantic mismatch, or failed update;
- shutdown correctly classified as cancellation, and all workers joined.

This proves physical analogue input, stable measured rate, 10+ key rollover,
release-to-zero, range, and clean shutdown. The tester did not unplug the device,
so reconnect still required one repeat of that step.

## Log 8: reconnect accepted

`HallJoy (8).log` (592,665 bytes, 1,800 lines; SHA-256
`3360D442A527DA993E846B6F88456406BAD2EADD02B4A18E3FAF49C63A0041C7`)
ended cleanly and recorded three disconnects plus three successful reconnects.
Every recovery preserved path/instance hashes, repeated strict proof with
`mismatch_mask=00000000`, and reconfirmed 60 mappings, 10-micrometer precision,
and 3400-micrometer range.

The first recovered session delivered 1,008 matrices in 2.951 seconds
(`341.463 Hz`), 329 nonzero and 238 changed frames, 12 nonzero HIDs reaching
3400 micrometers, four full press/release cycles, and paired transactions no
slower than 2.522 ms. Intermediate open/read failures occurred while Windows
briefly exposed a HID path before firmware was ready; bounded retries completed
strict proof both times.

Disconnect, repeated claim, re-authentication, and recovery of real analogue
data therefore passed. No further MAX diagnostic rerun was required.

## W669 follow-up boundary

The cited W669 `HallJoy (4).log`, SHA-256
`C24DD91D26EF4056202DCA659E168E43DADFF8AA9BF0FB8A2A5C320BBA39E28A`,
was incomplete: only 35,120 bytes of a 64 MiB mapping contained data, and two
writers competed for `HallJoy.log`, hiding raw W669 and timing evidence.

It nevertheless showed that absent MAX discovery enumerated 21-22 HID
interfaces every ~1.01 seconds for 8-20 ms. Discovery was changed to react only
to `WM_DEVICECHANGE`; W669, MAD68, and lifecycle evidence now share one
asynchronous mapped writer; normal shutdown avoids blocking forced flush and
creates no diagnostic-exit sidecar.

The corrected W669 diagnostic executable was
`build/aula-w669-diagnostic/HallJoy.exe`, 2,318,848 bytes, SHA-256
`6931EA0AA3B32205F0AEA395C90C7081C3A4F9A1606CE1C1A74583D00FDB377E`.
Local `WM_CLOSE` ended in 75 ms with all workers joined and one log. A fresh
physical W669 run was required because the earlier file could not establish
stability.
