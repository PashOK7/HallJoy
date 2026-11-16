# Keydous NJ98-CP V4S — log51 review, 2026-11-14

Owner supplied the downloaded report and November11 tester feedback: three keys
seem fine, more can stick; wireless remains usable but less reliable than wired.
This is evidence review, not a request to remove RF support or deliver another
speculative EXE. Current RF remains Experimental, wired confirmation is retained.

## Exact input and coverage

`HallJoy (51).log`,595734 bytes, SHA256
f3d206dd587b64f647fb000867f0b637fe82d4c4071fff3a05962def7abc30f5.
Header2026-11-11T12:44:31Z /HallJoy1.6.15.0 matches the screenshot's attachment.
Revision6 RF trace is present; an exact tester EXE hash is not recorded. Original,
reproducible own analysis and pinned summary are local under
`.local/research/keydous-nj98v4s-log51-20261114/`.

Eight complete snapshots, catalog28/28. Last connected snapshot: exact receiver
3151:5038, remote3793,97mapped positions;12343 provider-defined updates,1failure.
Two RF sessions; first completes only one page and no stream packets. Session2
subscribes at707953 and ends with307successful pages,5113stream packets and4
genuine table-zero corrections. Initial session.begin662234 to subscription is
45.719s; do not claim immediate startup. The reason for every initial gap is not
established. Clean session.end793062 is retained.

## New decisive observations

The trace has87health,348page and271bound-slot rows. All saved health rows report
zero structural rejects, other reports and stream I/O errors. In the saved slot
state there is no latest received stream-zero timestamp without the matching
applied-zero timestamp; skipped_zero_ms and drained_zero_ms remain0. These are
retained observations, not a byte-for-byte history or proof that USB never lost
a packet before HallJoy received it.

An extended residual is now shown in **both** host input paths:

-Bound slot51 /HID0e: last stream update745718 returns raw16. From observations
 at746359 through772406, physical/effective publication stays40/1000 for26.047s,
 with stream_count unchanged104. Fresh accepted E5 table replies at751328,
 751375,763265 and769281 also return raw16; none contains a zero to apply.
-At772875 a new stream measurement changes it to raw13 /33permille; another
 accepted table at781390 reports13 too. This is not merely a fixed gamepad/UI
 value left behind while new source values were ignored.
-The40permille example is suppressed to zero by the configured output curve;
 it does not by itself prove gamepad drift. A different episode does affect the
 pad: after the digital releases around738500, slot45/HID0d retains raw131,
 328permille. At740296 and742250 trace.pad shows native328,filtered273 and
 right-stickRX=-8939. Digital release corroborates the tester's observation;
 Rapid Trigger key-up alone is not proof that the physical sensor reached zero.

The table fallback really repairs some missing stream releases (4corrections).
It cannot fix a residual when the table itself still supplies that residual.
Full table sweeps can be very slow: maximum17.847079s in this run; direct stream
publication is on an independent input thread, not paced by this sweep time.

## Code and protocol cross-check

Current production rongyuan_stream_protocol.h parses one manufacturer1B slot/depth
record from a32-byte report, with no three-key counter or cap. The event pump
publishes each mapped physical slot independently. E5 pages contain32values;
queries are serialized, pending responses are drained before a different request,
and the ordering ticket is taken immediately before send, under the publication
lock. No observed superseded zero explains this run. The log has simultaneous
positive native values on six bound channels; the tester's threshold must not be
rephrased as an established protocol capacity of exactly three keys.

These checks narrow the problem to the values delivered through the wireless
input path in the shown episodes, before the response curve/gamepad stage. They
do not prove that every HallJoy defect is excluded, that all replies are fresh
physical ADC measurements, or that a specific keyboard MCU function is faulty.
Keyboard firmware, radio forwarding, receiver/cache behavior and pre-application
USB loss are not separately observable here. Raw payloads and sequence numbers
are absent; exact keyboard/receiver MCU images were not established by prior
acquisition. Do not claim a firmware execution PASS or exhaustive impossibility.

See [fresh firmware/cache investigation](KEYDOUS_NJ98_V4S_FIRMWARE_2026-11-14.md)
for the current manufacturer3.2.33 acquisition and scoped sibling execution.
Exact3793/receiver image acquisition remains unresolved.

## Decision and maintained status

Keep the wireless path **Experimental**, with its measured residual/recovery
restriction. Preserve the confirmed wired route. No timer-generated zero,
digital-release substitution, arbitrary rate cap or three-key admission limit.
No new EXE is justified solely to repeat this diagnostic: revision6 answered
whether an observed zero was lost after parsing and whether the table could fix it.
Further implementation needs an evidenced improvement to freshness/full-matrix
delivery, or exact receiver/keyboard producer code; repeated speculative builds
and a fixed number of failed tests cannot prove RF analog is impossible.

Live Sheet read2026-11-14: Main!A474:C474 is Keydous /NJ98-CP V4S TMR /
Implemented; awaiting hardware testing. Strict dropdown and matching yellow base/
effective colors retained; no Sheet write or structure change. README's USB
confirmed /experimental2.4GHz scope remains accurate. Hardware evidence and
current owner entry updated. Runtime, normal EXE605d4556 and release untouched.
No build/test rerun is needed for this evidence-only documentation change.
