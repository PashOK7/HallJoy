# Keyboard tester delivery contract

Owner decision, 2026-11-08, after HZ-94 log38. Mandatory for every new keyboard
integration and every tester EXE, including Experimental and protocol-match paths.
This contract supplements the existing firmware behavior and diagnostics contracts.

## Admission must follow demonstrated requirements

Before delivery, enumerate EVERY branch between finding the intended interface
and starting a reviewed analog read. Classify each condition:

- Hard refusal: wrong target identity, malformed/inconsistent transport framing,
  unavailable/busy transport, actual ownership collision, unsupported required
  protocol semantics, or cancellation. Cite concrete evidence for the necessity,
  supply a negative executable case and a specific observable log reason.
- Metadata variation: firmware strings, remaps, disabled/duplicate bindings,
  extra/default table entries, unavailable/inconsistent assignment snapshots,
  layout/display metadata, assignment counts and
  other values not proven necessary for safe decoding. Record and normalize or
  isolate them while continuing the reviewed safe analog attempt. A desired
  value, ideal fixture, arbitrary threshold or uninvestigated assumption is not
  proof that a refusal is necessary. If a field genuinely changes safety or
  semantics, establish that dependency before promoting it to a hard refusal.

Identity gates before potentially harmful commands and reply bounds remain
mandatory. Do not interpret this contract as permission to probe unrelated HID
interfaces, guess state-changing commands or ignore malformed packet bounds. Reject malformed metadata as metadata;
continue with established physical channels when those bytes are not required
for the analog decoder.
Layout-cache/UI failures cannot alone veto independently established sensor reads.

## Required tester evidence

The first EXE must attempt the reviewed analog route for a proven identity.
A refusal must identify stage, observed condition, required condition and reason
in ordinary HallJoy.log even when continuous logging is off. Record subscription/
read attempt, first valid analog sample (zero is valid), useful aggregate multikey/
release evidence and cleanup. Unknown reply envelopes must leave bounded new
structural evidence; do not return another generic no-keyboard log. Keep existing
privacy rules, one-EXE/Open-log workflow, no tester scripts and no forced logging.

For ordinary symptom checks, the tester uses HallJoy normally, observes the
result, clicks Open log and sends HallJoy.log whether or not the symptom occurs.
Do not prescribe an arbitrary hold, idle interval or minimum test duration to
collect evidence. Collect evidence as events occur. Where a diagnostic stage
needs more data, advance on the observed event or explicit completion signal.
Open log saves the available evidence and identifies relevant unfinished stages
so an incomplete observation cannot be mistaken for a negative result.

## Mandatory executable cases before delivery

Test the production path from discovery/identity through subscription, a decoded
sample, simultaneous keys, release and cleanup. Parser-only tests or a hand-made
perfect factory map are insufficient. Include available tester evidence and benign
variations: extra table slots, all assignments duplicated, sparse/all-unbound
assignments, unknown assignment metadata, remaps and valid firmware variants.
Clearly label reconstructed fixtures: log38 provides counts, not original bytes.

For every hard refusal, include a negative case proving the command boundary and
useful reason. Verify the final normal EXE, built-in layout/autoselection and
ordinary log retention. Maintain `tester_delivery_cases.json` for the investigated
family. New tester integrations must add their cases and receipt before delivery;
existing families are not implicitly certified by the HZ-94 case.

`tools/check_tester_delivery.py --exe <candidate>` runs registered production-image
checks and validates their machine-readable receipts. The normal release wrapper
runs this before replacing the installed EXE. Missing stages, missing cases,
invalid receipt, nonzero exit or timeout block delivery. Evidence is stored with
the exact candidate SHA-256. Do not bypass a failure by weakening the assertions
or claiming software tests establish hardware behavior.

These gates prevent regressions in recorded requirements. They are not a claim
that every future firmware or every unreviewed backend has been exhaustively tested.
