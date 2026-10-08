# Support diagnostics contract (schema 2)

## Source of truth

The compiled `native_analog_backends.def` manifest determines both lifecycle and
telemetry capacity via `native_analog_catalog_size.h`. There is no independent
16/32-entry limit. `CollectNativeAnalogTelemetry` is shared production code with
a direct executable regression test. Ordinary UI collection retains only active
entries; background diagnostic collection includes EVERY entry, even absent or
unavailable ones, and reads registry lifecycle state/generation/error/operation.
No collector issues USB transactions. Lifecycle reads happen only in background
diagnostics, not on the realtime or UI path.

`Backend_GetAnalogDiagnosticTelemetry` captures one bounded observation sweep.
Providers are read separately, not under a global lock: this is NOT an atomic
cross-device snapshot. Summary counts derive from that same sweep. UI search and
connection observations have `_cached` suffixes and never override fresh evidence.
`none_reported` means no provider reported connection; it is not proof that a
physical keyboard or a usable firmware protocol does not exist.

## Complete reports and unknowns

Every periodic snapshot (30 seconds, including bounded in-memory history when
logging is off) includes begin/end markers with a matching sequence, coverage
counts and one row per compiled backend. Triggered snapshots use the same format.
Inventory is Windows metadata only. Each row carries stable protocol ID, catalog
index, USB/report metadata, provider availability, observed connection, counters,
and separately available lifecycle evidence. No key values/text, serial numbers,
user device names, paths, or raw packets are serialized.
HID inventory rows (`hid.candidate`) also carry `name="..."`: the USB product
string reported by the bus (DEVPKEY_Device_BusReportedDeviceDesc of the HID node,
else of its parent USB node), i.e. the manufacturer's firmware string, read from
Windows metadata without opening the device (2026-10-02, owner request, so the
keyboard model in a tester log is not guessed from VID/PID). Empty when Windows
has none; control characters, quotes and backslashes become `_`; at most 48
characters. User-editable friendly names are still not read.

`unavailable`, `not_present`, `present_not_connected`, and `connected` are distinct.
Lifecycle `running` means worker startup succeeded, NOT analog reception.
`lifecycle=unavailable` remains explicit even if ordinary telemetry is available.
`complete` describes coverage/availability of telemetry; lifecycle availability
is a separate per-row field. Counters retain each provider's existing scope; do
not subtract across reconnects or assume every provider counts complete keyboard
frames. A zero counter/age must never alone establish failure. Long idle periods
are valid for change-driven streams; there is no generic silence timeout.

Queue/history/file bounds remain 512/512/4 MiB. Reports may be truncated by those
bounds or concurrent event bursts. The analyzer verifies begin/end, expected rows
and unique catalog indices rather than declaring missing rows absent. Queue loss
is explicit. Raw logs remain available even when automated interpretation fails.

## Typed event details

`SupportLog_Event(category, value)` is a structural event.
The optional third argument MUST be `SupportLog_Data(...)`, `SupportLog_Win32(...)`
or `SupportLog_Protocol(...)`. An untyped third integer is a compile error.
`detail_kind` identifies interpretation. Data details always emit `error=0`;
nonzero board/profile/count values are not errors. Win32/protocol codes are separate
domains. A backend may have a protocol-specific payload meaning documented locally;
never infer that a nonzero event value itself means failure. Existing multi-value
call sites were explicitly classified, not automatically treated as Win32 errors.

## Analysis and regression gates

Run `python tools/analyze_support_log.py <log>` for JSON evidence: latest snapshot,
structural/telemetry completeness, source-summary consistency, and queue loss.
Legacy schema1 logs are explicitly unverifiable for catalog coverage. The tool
never infers a faulty keyboard, closed configurator, or failed analog from silence.
Tester statements are separate evidence; preserve them even if telemetry differs.

`tools/build_release.ps1` requires `tools/check_support_diagnostics.py` before
compilation/publication. It compiles the production collector, real Win32 writer
(normal and diagnostic variants), and affected fake-HID sessions. C++20 g++/clang++
(or CXX override) is required locally; no GitHub Windows workflow is involved.
The normal native-backend runner includes these tests and analyzer fixtures too.

Regression cases: connected provider beyond index16, missing telemetry, overflow
mismatch, absent providers, unavailable lifecycle, idle event stream, duplicate
or truncated log rows, contradictory summary, and legacy zeros. Writer coverage
includes no forced file for limited/unstable warnings, mirror behavior, privacy,
write failures/recovery, bounded overflow and portable single destination.

## Adding a protocol

Add the descriptor to the manifest. Implement meaningful generic telemetry with
read-only atomics/snapshots, and never use UI refresh as a diagnostic data source.
Keep `present` separate from confirmed live analog; document what update counters
and age represent. Log protocol-specific admission failures using typed details.
Unknown cause must remain unknown, not an invented driver-conflict diagnosis.
Tests must cover admission, normal samples, malformed replies and disconnects.
Passing host tests does not prove hardware behavior or guarantee all future bugs
are impossible. New firmware-specific gaps require extending the backend evidence.

## One executable, one log — owner decision, 2026-09-27

Tester workflow must use ordinary HallJoy: one HallJoy.exe and the existing
HallJoy.log through Open log. Device-specific research diagnostics belong inside
HallJoy's bounded background lifecycle and shared log writer. Do not ask testers
to unpack a research archive, run scripts or command-line flags, or return a
separate firmware/research TXT/BIN export. Internal offline tools and private
agent artifacts are permitted but must not become user-facing prerequisites.

Use one logical log through its existing normal/portable storage and mirroring
policy; this does not remove the already authorized enabled-log mirror beside
the executable. Preserve privacy, size bounds, cancellation and no forced logging
for instability/limited banners. The single-log decision changes delivery and
integration, not the evidence needed for support or permission to log arbitrary
user memory. Any required research evidence must be scoped and reviewed.

The Alumix104 ZIP/CMD/separate-export approach is rejected. The implementation
now starts a bounded exact-device background reader in the engine lifecycle;
Pause/Exit cancels and joins it before provider release. No export CLI remains
(other than internal self-test). Reviewed command12 evidence goes only through
SupportLog_RedSquareResearch and the normal HallJoy.log writer. No arbitrary
RAM, input values, serials or paths may be passed to this scoped API.

The writer retains up to2300 research records separately from512 history lines
inside the same bounded4MiB log, so Open log snapshot resets preserve the capture.
A research record schedules a normal log flush without enabling general logging;
this does not change instability/limited-banner policy. Records are paced, queue
admission is checked, and missing data causes offline validation to reject the
capture. This is diagnostic evidence, not confirmation of analog support.
Neighbor68 establishes the code-window interpretation; actual104 memory mapping
remains unverified and must be checked before implementing its analog reader.

## Alumix104 live stream trial — owner revision, 2026-09-28

The temporary official66/67->55FB research worker has no overall capture timer,
ACK deadline or silence failure threshold. Its Russian window-title prompts
advance on parsed one-key depth/release and two distinct post-prompt indices;
typing remains a tester observation. Pause/Exit ends the trial and attempts OFF.
Finite per-operation waits and bounded final cleanup protect lifecycle shutdown;
they are not a duration limit on the experiment. Long idle streaks are recorded
at sparse milestones without declaring a silent device defective.

Research records include admission and mode-write outcomes, stage/checkpoint
counts, read errors, explicit missing-evidence fields, cleanup and final result,
even when the stream never starts or the tester stops early. The v3 research
marker replaces a prior trial's retained evidence during Open log snapshots.
Writer regression covers a zero-frame stall and snapshot retention. No raw
travel values, keyboard text, paths or serials enter the shared log. Aggregate
calibration/ADC changes and range anomalies help assess stream plausibility.

## Alumix104 log39 correction — 2026-09-28

The first readiness build compared raw `keyStroke` and `maxStroke` as if they
shared units; the pinned official UI uses `/100` and `/10` respectively. Its
reported near-full, shallow and range-anomaly counts are not valid coverage
decisions. The next build uses a tenfold scale factor and an exact synthetic
parser test. The physical log still establishes stream, index, ADC-change and
mode-cleanup evidence.

The user requires HallJoy itself to observe whether alphabetic keyboard events
arrive. The existing Windows Raw Input registration is used; the research
worker counts only physical HID-letter down/up transitions from the admitted
0C45:80AC keyboard, with device-name matching but no path or key identity in
the log. A post-analog letter down/up automatically completes the experiment,
sends OFF and presents the final log instruction in the Russian title. This
proves Raw Input delivery to HallJoy, not text rendering in another app. If
Raw Input is unavailable or no letter arrives, explicit counters and missing
evidence remain; Exit still attempts cleanup. No Notepad or Pause step is
required for a completed test.

Unrelated full-length reports are classified by structural category and logged
at cumulative power-of-two milestones to avoid one line per interleaved
report. The writer retains the first research marker plus the newest2299
records on long runs, so final OFF/quality/end records survive Open log
snapshots. The long-run writer regression exceeds the retention limit.

## Alumix104 correlated hold v4 — 2026-09-28

The v3 `two_indices_after_prompt` was insufficient for simultaneous-key
evidence. v4 joins the pinned exact-model HID-letter-to-sensor map with
target Raw Input held-state revisions in memory. The stage advances only
after two mapped positive sensor samples in one unchanged two-letter held
state. The log stores aggregate witness/positive/zero/missing counts only.
The final stage requires a full post-prompt down/up pair for the same
physical letter. The writer resets retained research evidence on a v4
start marker and retains the first marker and latest tail across long runs.
Asynchronous observations are corroboration, not a synchronized device
snapshot; no witness does not prove analog multi-key impossibility if the
digital interface is suppressed. Closing an incomplete run still records
its counters and attempts OFF. No experiment-wide timer was added.

## Alumix104 log40 and v5 release/range diagnostic — 2026-09-28

The physical v4 log showed the original held pair's release was ignored by
the final gate, which required a new letter down/up. v5 completes after
those same two letters are digitally released and each mapped sensor has
subsequently reported a positive-to-zero transition. The Russian title
separates held-pair release from waiting for sensor-zero evidence; no new
press is required. Removal of the Raw Input device is explicitly recorded
and cannot count as digital release. Incomplete stages still log and attempt
OFF on close, without an experiment-wide time limit.

The v4 aggregate `range_anomaly` combined zero declared maximum with travel
above the normalized maximum. v5 logs separate counts and cumulative coarse
excess-ratio bands, plus anonymous minimum/maximum sample and depth-change
counts for the witnessed pair. No exact key identities, text, raw reports or
individual travel measurements are serialized. The v5 research marker
replaces retained earlier trial evidence in the bounded normal HallJoy.log.

## Alumix104 native gamepad trial — 2026-09-28

The earlier stream-probe completion stages are superseded for tester handoff
by the exact104 native analog backend. Its session remains open while the
device supplies data and HallJoy is active; there is no test duration or
manual Pause requirement. The shared log carries bounded admission, ACK,
stream, quality, keyboard Raw Input, consumer and gamepad publication counters.
On stop or failure it records OFF cleanup and a summary even when no valid
sample was obtained. A temporary Russian title reports active analog or a
fault. Open log and the single ordinary HallJoy.exe remain the tester flow.
Gamepad publication counters are correlated by connected session, not a proof
of per-key causality or final ViGEm application. No forced continuous logging,
key names, keyboard text, serial/path or individual depth values are added.

## Alumix104 source-to-gamepad trace v2 — 2026-09-28

The exact104 temporary trial records a bounded common chronology in the
existing HallJoy.log. Each observed sensor receives an opaque session slot.
While recently active, 250 ms windows record source reports, repeated unchanged
positive reports, explicit zero reports, last-report age, Raw Input letter
activity as reference only, consumer reads and coarse depth bucket, calculated
gamepad candidate/pad mask and publication counts. Quiet windows use 1 s.
These windows pace observation; neither interval is a release timeout or a
test-completion deadline. The final summary includes each slot's positive and
zero totals, exact repeats, maximum inter-report gap after a positive sample,
positive-to-zero transitions, and its per-pad binding action mask. Action bits
0–3 mean axis minus, 4–7 axis plus, 8–9 triggers, and 10–24 buttons. Other
report headers are counted by family/opcode without payloads. The trace ends
with OFF cleanup even on interruption or zero useful samples.

An absent report for one slot is unknown, even while other slots update. The
current gameplay reader can retain the last positive value for 1000 ms; this
trace is intended to determine whether lag occurs at the source, retention,
binding, calculation or publication stage. Digital key-up never changes analog
depth. No raw report, exact key index, travel value, text, serial or path is
logged. The 2300-record/4 MiB existing bounds still apply; long captures may
lose early windows, while the retained start marker and latest final summary
survive. Passing synthetic/Win32 tests is not physical exact104 validation.

## Alumix104 unknown-frame and collection trace — 2026-09-28

Log44 physically returned only echoed 0x68 payloads in OFF and ON phases.
The next ordinary EXE therefore stops issuing 0x68 and inventories all seven
exact-product HID collection descriptors from metadata, then observes the
known 0x66/0x55FB session until HallJoy exits. The six previously rejected
collections are descriptor evidence, not assumed analog sources.

Non-0x55/non-0xAA reports on the admitted FF68:61 collection receive anonymous
session class IDs. The shared log contains aggregate counts, payload-change
counts, same-Raw-Input-state changes, two-letter-overlap counts, zero-body
counts, nonzero-byte maxima and changed-byte-position masks. Prefix values,
payload bytes, exact keys, keyboard text, serials and device paths are not
logged. Per-window counts join existing 250 ms active/1 s quiet source windows.
No digital transition supplies or zeroes analog. A varying packet is only an
investigation lead; it is not a decoded per-key measurement.

The new research start marker clears retained prior trial records. Collection
metadata is emitted after this marker, so Open log snapshots retain the
inventory. Final class/source counts and OFF cleanup are emitted even if no
class changes or no useful analog arrives. No overall test timer, calibration
commands, 0x68 replay or gamepad publication was added. The normal 2300-record
research tail and 4 MiB writer bounds continue to apply; queue loss remains
explicit. Production fake HID and Windows writer regressions passed. Physical
classification and typing coexistence remain unverified.

## Exact Alumix104 packet-capture exception — owner correction, 2026-09-28

The earlier aggregate-only unknown-frame trace (physical log46) hid the bytes
required to decode its single changing report class. The owner explicitly
challenged that trade-off. For this exact support investigation, the ordinary
HallJoy.log may contain up to 512 complete changed 65-byte vendor input
reports from the exact 0C45:80AC product's accepted FF68:61 collection.
Capture the first frame per class and every changed frame, including its report
ID and prefix, with monotonic log time, event ID, class report count, Raw Input
held-letter count/revision and the last selected 55FB sample/age. Do not
capture standard keyboard HID reports, text transcription, paths or serials.
The packet MAY encode key state; disclose that in the Russian title and mark
the log header `raw_hid_payload=1`. Record cap saturation, write failures and
normal cleanup explicitly. Retain the one EXE/one Open log workflow, 2300-line
research tail, 4 MiB file bound and all ordinary support. Digital state is
reference only and never computes analog. This exception supersedes the
no-payload rule above only for this exact diagnostic; it does not authorize
raw capture for other devices or routine HallJoy operation.

## Snapshot request versus displayed banner — 2026-09-28

An explicit `SupportLog_RequestSnapshot()` can be triggered by Open log or by
backend cleanup. It now emits `support.snapshot_requested source=api`.
`support.banner_shown incident_latched=1` is reserved for a pending
`SupportLog_ReportMissingSource()` transition. Older logs used the latter
line for both cases; do not infer a visible banner from that line alone.
Physical log47 contained this false attribution at shutdown. The corrected
writer test verifies that an explicit snapshot retains the request marker
without manufacturing a banner event, while a real missing-source report
still produces the banner marker.

## Failed start / Resume — 2026-10-05

A Resume that rolls back to Paused is an incident, not a user pause:
`engine.resume_failed` (Win32 detail = owner's last native error) goes through
`SupportLog_ReportFailure`, so the log is saved automatically.
`engine.resume_step_failed value=<step>` names the step (1 reset providers,
2 catalog, 3 backend init with the init-issue mask as detail, 4 dependency
guidance, 5 start generation, 6 restore UI input) and
`engine.resume_retry value=<attempt>` each automatic retry. Structural values
only; no keys, input values or paths. See
`docs/current/STARTUP_PAUSE_FIX_2026-10-05.md`.

## JingTai/IROK admission failure — 2026-10-05

`mg75.admission_failed value=<packed>` (Win32 detail) records why the
`irok-mg75-pro` backend could not admit a present keyboard: phase, exchange
stage, frame reject cause, command and the first reply header bytes (length,
command echo, status) plus the report count. Header metadata only, never
travel values or key data; identical repeats are suppressed. Layout of the
value: `docs/current/WLMOUSE_YING75_2026-10-05.md`.
`mg75.shared_open value=1` (once per process): a WLMOUSE Ying75 session had to
open the vendor interface shared because another program held it; stage 5 of
`mg75.admission_failed` means a foreign reply was seen and the session stopped.

## W669 release evidence — 2026-10-05

`w669.travel_config` (once per session: range, unit, minimum actuation) and
`w669.release` (aggregate presses/releases, number of keys left with a
residual value and the largest such value in permille, floored-event count)
let a support log show whether a W669 keyboard reports release-to-zero. No
key identities or per-key values. Layout of the values:
`docs/current/REDRAGON_K686_2026-10-05.md`.

## Input configuration summary — 2026-10-06

Every snapshot adds `input.config seq=N` (after `uap`) with aggregate counts
of the active profile: `virtual`, `pads`, `global_invert`, `global_curve`,
`global_low/high/cap` (permille), `unique_keys`/`unique_inverted` (per-key
curves), `axis_directions`/`axis_inverted`/`axis_same_key`,
`triggers`/`triggers_inverted`, `button_keys`/`buttons_inverted`
(inverted = effective curve inverted: such a key outputs full value at rest).
No key codes, names or input values. Provider: `input_config_summary.cpp`,
registered after the startup profile loads.

## Input-chain trace — 2026-10-06 (owner decision)

Owner: aggregate-only logs could not explain a tester's "sticks reversed,
triggers always pressed" (Redragon K686), and anonymizing everything made the
log useless for that bug. The ordinary log now carries a bounded input-chain
trace (`input_trace.h`, header `bound_key_trace=1`), kept in its own window of
the newest 4000 lines so snapshots and trace never evict each other:

- `trace.bindings` — binding table per pad (HallJoy key codes in hex, `!` =
  effective curve inverted, `*` = own per-key curve), global invert, snappy and
  last-key flags; on change and every 10 s.
- `trace.pad` — gamepad output (sticks, triggers, buttons) with
  `key=raw/filtered[/nNATIVE]` permille of every axis/trigger key and of
  button keys while they carry a value; on change, at most every 16 ms per pad.
- `trace.src w669` — each W669 event of a bound key: row, column, travel,
  published permille, key code; `trace.session w669` — product, factory
  profile, range, floor, mapped keys.
- `trace.os_key` — Windows Raw Input down/up of a bound key (keyboard id
  hashed), the physical reference for the provider's key identity.

Only keys bound to the gamepad are traced, never other keys; event producers
share a 300 lines/s budget. Writing still follows the normal policy (banner,
Open log, logging enabled).

## Format capture store — 2026-10-06 (owner decision)

Owner: decoding an unknown protocol must not depend on time windows or on
instructions for the user; opening HallJoy, pressing any keys for 5-10 s and
sending the log must be enough. `SupportLog_Capture` stores records starting
with `capture.` immediately (own lock, own `uptime_ms`), outside the timed
queue and the trace window; the first 8000 records are kept, never evicted,
and every written report contains them (appended live in continuous mode).
Bounded by content only.

- `capture.logitech n=.. <hex>` / `capture.logitech repeat=N` — raw HID++
  reports of a Logitech 0x1B08 version without an established event layout
  (PRO X2 RAPID, version 2); nothing is published meanwhile; closed by
  `logitech.raw_capture` (records, detail 1 = store full). Since the same
  day version 2 is decoded and published; the capture (and `capture.key`)
  keeps running alongside until the store is full, to complete the key table.
- `capture.key hid=.. down=.. keyboard=..` — while a capture runs, the first
  two presses of every key (any keyboard, id hashed) as physical reference.

This includes Logitech key ids of pressed keys; it is needed to decode the
format (owner rule 2026-10-06). See `docs/current/LOGITECH_RAPID_2026-10-05.md`.

## Generic protocol events — 2026-10-07

- `ipi.generic_profile` value = BY UUID outside the catalog, detail =
  present IDs << 16 | calibrated IDs (generic BY discovery, read-only).
- `logitech.generic_analog` value = PID of a Logitech device outside the
  catalog that reported HID++ 0x1B08, detail = feature index << 8 | version;
  `logitech.generic_model` replaces `logitech.model` for such a session.
- The UI notice `GenericProtocol` is driven by telemetry `genericProtocol`.
See `docs/current/GENERIC_PROTOCOL_SUPPORT_2026-10-07.md`.
