# MIDI M1: source-qualified normalized observation history

Owner requested the next [MIDI roadmap](../development/MIDI_ROADMAP.md) stage.
This implements an opt-in history at normalized host publication boundaries,
with explicit identity, connection epochs, order, timing origin and quality limits.
It does not claim that a latest-snapshot upstream API exposes every sensor sample.
M2 musical velocity estimation has not been redesigned in this task.

## Input contract and implementation

Each observation carries a process-local source id, connection generation,
physical position or explicitly identified bindable-code namespace, assigned code,
normalized value, availability, sequence and timestamp. Source ids are runtime
provenance, not USB serials, persisted profile identities or log fields. A real
publication and an initial baseline have different roles: a baseline never starts
a held note. Old assignment records cannot be interpreted as a new binding.

The common physical Publication now emits at the existing normalized write;
explicit native descriptor callbacks select the same factory/remapped publication
as the ordinary read path. Physical positions stay distinct before note aliases
are combined. Fixed-code native publishers expose their actual code identity,
without inventing an unavailable matrix index. SparkLink currently publishes its
existing per-code aggregate; upstream physical aliases already combined there
cannot be separated by this downstream history. SAYO exposes its physical slots.
No USB command, firmware mode, transport frequency or keyboard admission gate changed.

Native descriptor ABI5 adds an optional observation callback. The final ordinary
EXE full-catalog check independently requires the callback for every retained
playable protocol in its explicit expected list. Metadata-only diagnostic modules
are not falsely advertised as gameplay observation sources. The ordinary gamepad
consumer retains its original values/arbitration and default one enabled pad.

## Bounded optional history and lifecycle

A bounded 32,768-cell multi-producer/single-consumer queue preserves committed
publication order. Atomic cell sequences prevent unread cells being overwritten.
Producers do not wait for the consumer. Full/bounded-contention failure and
malformed data have explicit counters; lost history releases owned MIDI voices,
discards old queued observations and requires release before rearming held keys.
Discard also fences a producer that reserved an old cell but commits it later.
The voice table is bounded at 2,048 contributions with a fixed 4,096-entry lookup;
normal event processing does not scan all voices. Retired slots are reclaimed.

The history, tracker and voice state are allocated only with enabled MIDI. The
disabled publication path checks a capture flag without timestamp calls, history
queue writes, device reads or new port/worker activity. Existing providers publish
at their current cadence. When enabled, a committed observation wakes the existing
realtime consumer; no new MIDI input timer or deliberate poll reduction was added.

Disable closes capture and revokes public history/tracker references. Session
memory remains with the control owner until publication leases have returned,
so a last HID/bridge lease does not destroy a large queue on its input thread.
Nonblocking control reconciliation and the existing bounded five-second stop
retain ownership on failure. No detached or force-terminated worker was added.

## UAP and provenance limits

The parent snapshot bridge now records separate UAP devices, using the existing
coherent Provider V2 broker when available. Standard and extended key codes,
ownership/value/freshness flags and per-device values are retained; a temporary
broker mismatch does not erase extended keys. Dense-only compatibility uses the
reported per-device timestamp or the known host publication timestamp. Ambiguous
SDK ids remain separate explicit session slots, rather than guessed stable ids.
A host restart or device loss fences the source; old cached values cannot keep
notes alive after the host becomes unavailable.

UAP observations are labelled HostSnapshot. The native hooks are labelled
HostPublication. Neither is a claim of hardware sensor-clock timing. Gaps between
received UAP publications are counted. A skipped snapshot may have been unchanged;
it does not manufacture a keyboard disconnect or a fabricated trajectory.
A temporary tracker read conflict retains known held snapshot voices until a
coherent source view can be read, instead of producing a spurious release.

The isolated host also captures these validated publications BEFORE it signals the
parent snapshot event. A separate 32,768-record SPSC history mapping is created
only on explicit MIDI activation. It is unnamed; the parent verifies the exact
live child in its existing job and explicitly duplicates a noninheritable handle.
The child validates the immutable owner/host/nonce/generation/header. An atomic
single-transfer slot gives the handle one owner, including stop before acceptance.
The generation protocol tolerates claiming a newer handle before its tag is
published. Disabled MIDI creates no mapping/encoder or per-key history scan.
No new worker, input timer, SDK request or cadence reduction was added.

The existing bridge imports every queued received publication, preserving source,
connection, standard/extended codes, captured time, availability and release order.
A delayed/coalesced parent wake no longer discards movement that this child already
received. A weak tracker identity avoids retaining a stopped MIDI session between
bridge wakes. Overflow/malformed IPC records expose their separate counters, reset
MIDI ownership and start a new history generation; held notes must release/rearm.
An actual handle/open/handshake failure is logged and falls back to the existing
source-qualified snapshot route, instead of vetoing analog or gamepad operation.
The tester identifies ordered history, starting history or snapshot fallback.

Remaining source capabilities: the SDK can still coalesce sensor updates BEFORE
returning a snapshot to HallJoy, and a provider can combine physical aliases before
its normalized publication boundary. HallJoy cannot recover information that API
did not expose. Such sources retain HostSnapshot or code-aggregate provenance;
they are not described as exact sensor streams. The software M1 gate covers the
received acquisition publications, not hidden firmware/SDK samples. M2 must use
these honest timing/resolution limits when defining the estimator.

## Musical handling and diagnostics

Independent source/position contributions prevent one keyboard/key alias from
releasing another. Sampled complete presses and releases survive a delayed MIDI
consumer. Existing attack/touch math is shared with the legacy engine; it now
receives captured timestamps for observed input. First-edge baseline timing can
remain sparse and is counted, rather than presented as measured force or a
calibrated estimator. Controlled reset/reconnect does not replay held keys.

The MIDI tester exposes history sample count, input-loss resets, invalid observations,
snapshot observation/gap counts, UAP history mode and maximum observed queue age. This is input/queue
telemetry, not DAW receipt or audible latency. Ordinary structural events identify
history loss and failed history-stop ownership; no played note/key values, raw
packets, serials, source ids or port names are logged. Logging remains optional.

## Verification and delivery

Targeted portable observation tests and production Windows output-owner tests pass:
complete press/release between reads, repeated chords, aliases, two sources with
the same code, held silence, reconnect, repeated slot reuse, consumer scheduling,
8,000 concurrent producer events, full queue, malformed data, snapshot gaps and
disabled capture. The Windows owner sends all 100 captured Note On/Off pairs after
the motion has already completed, tests loss/rearming and retained publication
leases, and retains prior M0 failure/isolation coverage with a fake MIDI driver.

The full cached native suite passed after the UAP history integration, including
new portable stream tests and a separate Windows fake-child process test. The
child publishes 100 three-key chords before parent consumption; all 600 exact
Note On/Off edges arrive, including an extended code. Additional cases cover
unnamed transfer, suspended-child unclaimed-handle reclamation, wrong generation,
a tag/handle publication race, malformed/full wire storage, per-device aliases,
duplicate SDK ids, disconnect/reconnect-held, availability and zero disabled scans.
These tests exercise the production encoder, ring, handle transfer, import and
musical observation path; they do not open a physical keyboard or DAW.

During an earlier ordinary build, file-only startup recovery crashed in a double
free inside RTSSHooks64 while keyboard_canvas::WarmUpThread created a hidden window.
The crash dump stack is retained in heap-analysis.log. The file-only profile test
incorrectly started graphics warmup before its early return. app.cpp now skips
warmup only in that simulator/file-only mode, with a static regression guard.
Normal application graphics startup and RTSS settings were not changed. The next
ordinary build passed all 18 recovery scenarios and repeated startup.

Final validation: cached full native suite PASS, followed by focused final tests
on the exact current observation/output headers and the extended process test.
The latter also forces 40,000 publications into the IPC ring, proves loss reporting
and generation replacement, suppresses a held baseline and accepts the next
released/rearmed attack. The optional-history failure case retains snapshot input.
Ordinary release build, exact-image full catalog and tester contract, embedded
resources, profile transactions, layout catalog, 89 Supported layout entries and
all 18 recovery scenarios plus repeated startup PASS. The wrapper installed the
candidate byte-for-byte and restored the previously running HallJoy.

Delivered local EXE: build/bin/Release/x64/HallJoy.exe, version 1.6.15.0,
11,558,912 bytes, SHA-256 `38f99f2f305cd305e97c837c551e72468e468539211ea615ad70ef71048a1170`.
Its exact image receipt is .local/tester-delivery/38f99f2f305cd305e97c837c551e72468e468539211ea615ad70ef71048a1170.json.
Source hashes, evidence hashes and limits: .local/research/midi-m1-20261108/result.json.
The received-publication software portion of M1 is complete. M2 and all hardware
musical/DAW acceptance remain separate work.
Evidence: .local/research/midi-m1-20261108/.
Original M1 backup: .local/backups/midi-m1-20261108/manifest.json and adjacent
additional originals (supplemental-manifest.json). Two descriptor-only original
files were reconstructed by reversing the exact unique callback addition and are
explicitly labelled in reconstructed-descriptor-originals/manifest.json; they are
not represented as pre-edit full-file copies. The verified intermediate EXE plus relevant source originals
before UAP history are under .local/backups/midi-m1-uap-history-20261108/.
Restore only scoped files after checking current bytes. No physical keyboard,
DAW/audio measurement or visual UI run was performed. No support status/Sheet
change or GitHub publication was requested.
