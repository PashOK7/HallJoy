# CORSAIR VANGUARD PRO 96 native protocol

Scope: exact1B1C:2B0E, USB control IF2, stock2.8.171, Bragi1.5. Reference and full
behavior assessment: [current review](../current/CORSAIR_VANGUARD_PRO96_2026-11-02.md).
Hardware validation: untested. The existing tester log establishes the USB product,
not its firmware version or successful analog operation.

## Wire and admission

1024-byte Bragi payload; Windows HID I/O additionally carries report ID0 (1025
bytes). Requests are zero-filled: [subdevice0,direction1,session,command,args...].
Normal replies: [0,protocol-error0,own-session,echo-command,status0,data...].
No checksum is defined on this host command channel. One request is in flight.

- 27/session-open: header session0, arguments[1,nonce32LE,0]. Reply data echoes nonce
  and returns the owned nonzero session ID. Close uses header own-session and
  arguments[2,own-nonce32LE,own-session]. Never close an unproven session.
- 2/get-property: arguments[property16LE]. Read11/12 (VID/PID),13 (version bytes
  major,minor,patch16LE),105 (Bragi1.5),41 (hardware layout).
- 13/open-file: arguments[handle,file16LE]. Only directory11 and preflighted
  existing switch file4B are used.9/describe and8/read take[handle]. Describe data
  starts with two metadata bytes followed by size32LE; read returns up to1019 bytes.
- 5/close-file: [count1,directory-handle0] only. Switch-file handles are closed by
  ending the temporary session, avoiding firmware's settings reload.
- 2/C6: arguments[C6,00,subcommand]. Only3/2 (EEPROM rest/bottom),6/5(saved rest/
  bottom), with rest reread for consistency. This firmware exceptionally writes
  calibration data at reply offset3, overwriting command/status. Require its exact
  version, session prefix, expected length/zero tail and valid per-key records.
- 2/C7: arguments[C7,00], full current16-bit sensor array at reply offset5.

Set-property1, write-file6/7, calibration10, factory-reset15, C6/16 and all other
state-changing payloads are absent. Open-file4B can create defaults when missing:
the directory check and exclusive control handle are mandatory. Directory identity
is reread before initialization completes. No global relaxed-reply decoder exists.

## Normalization and maps

Reviewed data lives in corsair_vanguard_protocol.json; generate_corsair_vanguard.py
checks the C++ fact table without vendor downloads. Calibration comes from the
keyboard. Valid physical keys need nonzero14-bit endpoints, rest>bottom, span>=100.
EEPROM sensor6's bottom adjustment and integer scale match the actual firmware
loader. Existing switch records apply in order, including aliases; unknown records
are rejected. Descending41-point lookup preserves non-monotonic firmware tables.
The output is step*25 in0..1000; no interpolation, digital fallback or guessed
endpoints. Short switches retain the4mm scale and existing end-deadzone controls.

IDs1/2/4 map96/97/98 physical analog keys; unused sensor slots are not bound. Sensor0
is valid N. BR/ID3 stays excluded. Fn/OEM1 are existing extended identities for Fn/
StreamDeck. The built-in ANSI, ISO and JIS layouts follow the device-reported
region. The verified layout token is available only while a valid analog
session is connected. The hardware-testing advisory remains.

## Runtime and diagnostics

The polled backend starts AfterRealtime; UAP's known sources contain no Corsair
route. Discovery performs no HID opens. Before opening, foreign exact-path claims
are rejected; opened attributes/caps, identity, calibration and a complete live
snapshot all precede the exact-path ownership claim. Sibling interfaces remain
untouched. Common native aggregation owns curves, bindings, SOCD and ViGEm.

Target100 snapshots/s with10ms pacing, not a hardware throughput claim. Normal I/O
has a250ms deadline; canceled I/O is drained. Publication expires after100ms and
is cleared/wakes realtime on disconnect. Changed packets wake the common loop.
Pause/Exit joins the worker and attempts owned-session cleanup with a100ms budget.
No continuous diagnostic mode needs OFF. Rejected proofs are remembered until bus
absence or a deliberately fresh Resume generation; busy opens retry.

Generic telemetry reports presence separately from connection, busy, caps, mapped/
active counts, successful/failed snapshots, age and nominal41 levels. Structural
retained evidence: evidence.corsair.start(revision2),caps,input/output lengths,identity(packed
firmware and layout),calibration(source0/2 and key count),switch_settings,
travel_started,admission_or_read_failed(stage and protocol status),io_failed(actual
Win32 error),session_cleanup. No paths, serials, raw frames or per-key input values
are logged. Connected limitations never force logging; missing-source incidents
and Open log retain the normal writer policy.

Revision 2 diagnostics (2026-11-02):

- Discovery logs exact-product and IF2 collection counts, metadata errors and
  first Win32 error only when the result changes. Discovery opens no HID handles.
  Multiple control collections remain ambiguous; a missing IF2 is explicit.
- Transient attributes/preparsed/caps-query/queue-flush failures are retryable;
  actual incompatible attributes/report lengths and failed protocol proofs remain
  rejected until observed bus absence or a fresh Resume. A partial failed scan
  cannot erase the rejected list as if it established physical absence.
- `failure` includes a named stage/reason, request command/property/subcommand,
  actual Win32 and protocol status. Reasons distinguish version/layout gates,
  directory shape/change, calibration tail/range/change/endpoints/span, switch
  records, session echo/prefix and snapshot zero/sentinel/tail. Host identity and
  parser failures with Win32=0 remain explicit local refusals, not device errors.
- `transport` carries read/write phase, expected/actual report bytes, report ID
  and discarded prefix/opcode counts; it does not log payloads or session tokens.
  `admission` carries observed VID/PID, actual vs reviewed firmware, Bragi/layout,
  calibration source, switch-file presence and current file ID/size.
- `travel` summarizes valid frames, changed raw sensor slots, positive normalized
  samples, normalized changes, releases and maximum simultaneous positives. It is
  emitted at the first valid frame and first raw-motion/positive/release/multikey
  milestones (each once), every 30 seconds and at session end, including zero
  valid frames. No per-key values/identity. Target rate is not a measured hardware rate. This can
  distinguish raw motion below the first 0.1 mm step from no observed raw change.
- These records use the existing bounded first-2000/newest-8000 evidence store,
  not only the 512-line history. Open log includes retained records and explicit
  omitted counts; no retention promise is made after process exit or file errors.
  Every production record fits below 330 bytes, leaving room for uptime within
  the writer's 384-byte evidence line. Failure context is split across records
  joined by attempt ID. Neither logging nor incident policy is forced by evidence.

Software tests execute the production client and Win32 transport with fake I/O:
multiple keys/releases, source/layout variants, identity/version refusal, bad and
stale data, partial reports, report IDs, timeout cancellation/drain, foreign claims
and neutralization. They are not a hardware typing, latency or USB reliability test.
