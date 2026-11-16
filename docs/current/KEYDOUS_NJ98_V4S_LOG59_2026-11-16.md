# Keydous NJ98-CP V4S — revision7 result, 2026-11-16

The screenshot reports better three-key behavior, with sticking still present at
four or more keys. This is the awaited [revision7](KEYDOUS_NJ98_V4S_F7FC_FIX_2026-11-14.md)
result. USB confirmation is retained; RF remains Experimental.

## Pinned input and coverage

`HallJoy (59).log`,713,712 bytes, SHA256
`7e7f071f70477d16c61c4e1698caa91e0877d5250b2934ded6ce799338d5674c`.
Header2026-11-15T21:23:36Z, HallJoy1.6.15.0, exact3793/3151:5038, units100/mm,
97 mapped positions. Revision7 diagnostics prove the relevant implementation ran,
but no exact tester EXE hash is present. Do not infer83b9a97e byte identity.

Private `.local/research/keydous-nj98v4s-log59-20261116/` preserves the original,
own reproducible `analyze.py`, asserted `analysis.json`, fresh Sheet readbacks
and before-edit documentation hashes/backups. All13 snapshots have complete
coverage and consistent summaries; no logging queue-loss record is present.
102 health/control samples,408 page samples,247 bound-slot states are retained.
These are sampled states, not a complete raw USB/RF history.

## Findings

- Synchronization succeeds: session2 ends with1098/1098 marker ACKs, zero failures,
  no fallback, receiver0802. No old-marker waits. This confirms owned responses,
  not fresh keyboard ADC data behind a response.
- Session2 receives4248 stream samples and210 table pages, with two table-zero
  corrections. Saved health shows no malformed/other input or stream I/O errors.
  Every saved latest stream-zero timestamp matches its applied-zero timestamp;
  accepted table zeros also show no application mismatch. Loss before parsing
  or outside retained evidence remains possible.
- Slot51/HID0e gets raw233 at7796703. From7797046 through7803062 its count stays113
  and publication583/1000:6.016s of observed retained depth. At7796937 the gamepad
  really has RY=-18131, native583/filtered553. Digital key-up7796718 corroborates
  the symptom but does not measure physical zero under Rapid Trigger. Next press
  is7803093; no received analog zero is shown as ignored during this plateau.
- Its visible rejected table zero is from7794671,2.032s BEFORE the stream sample.
  Rejecting it is correct ordering; reusing it would erase a newer physical press.
- Page2 remains pending in stage9 (`read_ready`) from7808062 through7849125:
  at least41.063s observed,52 completions unchanged, failures59->262, last reply
  7807750. No new page0/1/3 completion occurs while the stream continues.
  `fetch_pending=0`: it waits for the REMOTE reply-ready state BEFORE FC, rather
  than waiting for FC USB response replacement. The marker fix cannot repair it.
- Startup remains poor: subscription7757718 follows session.begin7602843 by
  154.875s. Early errors13/32 and readiness waits are present; not every gap has
  a unique explanation. Session1 completes one page and no stream.

## Root boundary and next implementation targets

There is no three-key cap in the production parser/publication. `Client::Prepare`
preserves pending ownership rather than labeling a late untyped reply as another
page. However, one unanswered remote request blocks the entire four-page sweep,
removing table repair for missed stream releases. The source of that missing reply
is not uniquely located: keyboard producer, radio, receiver cache and loss before
the app remain alternatives. Exact3793 and this0802 receiver images are unavailable;
the acquired0801 sibling is comparison evidence, not this target.

There is also a concrete host opportunity: `Drain` records late pages but does not
publish them. Drained page1 zeros appear at7807312; the new accepted page1 follows
at7807781,469ms later. Publishing such a reply safely requires its ORIGINAL
stream-order ticket, page identity, framing and units. This can recover useful
late data, but does not solve a remote reply that never becomes ready.

Two separate targets remain: retain/apply valid late pages without overwriting
newer stream changes; establish an evidenced remote-command recovery rule.
Blind E5 retries, clearing pending state, switching page or toggling1B can recreate
the ownership race unless late replies are distinguishable. Prioritizing bound/
active pages can reduce work but cannot recover an already outstanding command.
No timer/digital-derived release, three-key restriction, calibration or settings
write is introduced. No exhaustive impossibility claim. The previously declined
manufacturer request remains unsent.

This review does not justify repeating the same tester trial. No runtime source,
EXE or build settings changed; no compilation was run. Ordinary imagec343d8e6
is retained. A complete RF fix is not claimed.

## Support synchronization

Live metadata confirms Main/id0,1306x26. Main!A473:C475 locates Keydous/NJ98-CP V4S
TMR at row474, `Implemented; awaiting hardware testing`, strict ONE_OF_LIST,
effective yellow B474:C474 RGB(1,.9019608,.6392157). No Sheet write or structure
change is needed. Fresh full color/status readback reconciles all286 yellow models
with runtime notices. README's USB-confirmed/experimental2.4GHz scope remains
accurate; hardware evidence, release wording, owner context and tester-follow-up
now include the remaining revision7 multikey failure.
