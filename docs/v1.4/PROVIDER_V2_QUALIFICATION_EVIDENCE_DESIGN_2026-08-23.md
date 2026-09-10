# Provider V2 physical qualification evidence design

Date: 2026-08-23

Package: R2-B2g. Status: historical equality evidence and the promoted
production-route evidence contract. Provider V2 is now the authoritative
ordinary input source when its same-transaction snapshot is available; legacy
dense data is retained only as the comparison shadow and compatibility fallback
when the V2 plane is unavailable.

## False-success conditions found

The R2-B2f counters prove that live comparison exists, but exporting their final
values directly would not be a trustworthy hardware gate:

- an idle run could report zero mismatches without exercising any mapping;
- repeated realtime ticks could count one unchanged UAP sample many times;
- `Backend_Init` reset the counters, so a later reinitialisation could erase an
  earlier mismatch;
- a crash before finalization could leave an old report looking current;
- field equality without a later neutral release would not prove release-to-zero.
- output activity from mouse/native input could impersonate UAP V2 exercise;
- one virtual pad could satisfy a same-named field requirement for another pad.

## Alternatives

1. Write the counters continuously to the ordinary production log. Rejected:
   this adds permanent I/O/noise and still does not define a fail-closed verdict.
2. Show counters only in the settings UI. Rejected as the evidence authority:
   it has no durable session identity/finalization and makes the user manually
   interpret a screenshot.
3. Add a hidden command that starts a second partial backend. Rejected: it would
   not exercise the ordinary application lifecycle and configured game path.
4. Build an opt-in ordinary HallJoy image with one transactional summary.
   Selected.

## Selected contract

The dedicated build runs the normal UI, backend, realtime mapping and qualified
XUSB publication. It writes `HallJoyProviderV2Qualification.txt` beside the EXE
twice: `INCOMPLETE` at process start and a finalized result only after normal
application shutdown. A temporary file is flushed and atomically replaced; no
qualification file I/O occurs in `Backend_Tick`.

Process evidence is monotonic and is not cleared by `Backend_Init`. The report
counts backend initialisations, unique UAP sample generations, eligible duration,
matches/mismatches, skip reasons and all seven field mismatches. Physical
coverage is gated by Provider V2 raw ownership/travel, never by mouse, native
input or Windows keydown alone. Activation requires at least half raw travel and
a non-neutral mapped shadow field. That field then remains latched pending until
both its Provider V2 raw bindings and mapped shadow field are neutral. It uses
28 independent bits (`pad_index * 7 + field_index`) so every V2-backed configured
field on every virtual pad requires this complete activation/release proof. A
repeated backend initialisation, digital fallback, curve mutation,
excessive V2 unavailability, insufficient generations/activity or abnormal app
exit produces `INCOMPLETE`. Elapsed duration is recorded for context but is not
an equality criterion: a hard 60-second boundary was rejected after physical
review because 59.9 versus 60.0 seconds cannot establish correctness or
stability. Any field mismatch produces `FAIL`. Only complete evidence with zero
mismatch produces `PASS`.

The current report explicitly records `production_route=provider_v2_authoritative`
and `legacy_dense_shadow_submitted_to_vigem=0`. It therefore proves the
authoritative V2 route against the same-generation legacy dense shadow; it does
not submit the legacy shadow to ViGEm. When V2 is unavailable, the ordinary
compatibility fallback remains deliberately visible in telemetry rather than
being misrepresented as V2 evidence.

## Representative physical result

Two reports from the same schema-1 qualification EXE (SHA-256
`0357A68E60E3DFDDC496DAC7D514C862A873C6FC3EC47AC56AF50F9D4DE9A8BB`)
were audited together. The first report (SHA-256
`BE0084B54F2A753B623AE78D56D44BB26514849BF58E0747AB07B06668614A51`)
contains 35,744 eligible/matched frames, 8,776 unique generations and complete
activation/release mask `0x78`; LT was the only configured field absent from
that run. The second report (SHA-256
`DE1380EEE913D5DA1292F4A030ACF0C70C8AFE9D149CDC497AD14C058CAB2620`)
contains 8,135 eligible/matched frames, 2,001 unique generations and complete
activation/release mask `0x26`, including both LT and RT.

Because the executable and evidence contract are identical, their direct
temporal evidence may be combined without runtime learning or digital
correlation. The aggregate is 43,879 matched frames, 10,777 unique generations,
configured/activated/released union `0x7E`, zero mismatches and zero unavailable,
fallback or mutation ticks. This closes representative physical configured
Provider V2 equality for R2-B2g. It is not a long-duration stability claim.

## Corrected schema-2 artifacts

Schema 2 makes `eligible_duration_ms` explicitly informational and removes it
from the verdict. A 2,547 ms exact-artifact idle smoke records 2,039 matched
frames, 504 unique generations and zero mismatches/skips; it remains correctly
`INCOMPLETE` only because no configured field was physically activated and
released. No temporary report remains.

The corrected qualification EXE is 8,653,312 bytes, SHA-256
`60C2E205C61CF6F61EE6216DB46A825121A34975924D35D2C9A96FC85473ED71`.
The rebuilt ordinary release is 8,644,608 bytes, SHA-256
`9E7FD20D41E441AF50D42A12FB9C44AEDB917C29D5A69FB4CA0778678DC63166`;
its linked image and runtime directory contain no qualification-report path.

Stability remains a separate soak/reconnect/fault-injection gate with an
explicit test purpose; no finite duration is presented as proof that a bug can
never appear after hours, days or a week. The promotion implementation still
requires fresh exact-artifact physical evidence before release approval.
