# Keydous NJ98-CP V4S TMR revision 4 retest

Tester reports improved 2.4 GHz input and did not notice sticking. Startup took
several attempts. The tester also reports occasional near-full output followed
by full output after a few seconds. The downloaded report is HallJoy (31).log,
430748 bytes, UTC 2026-11-06T20:14:28Z, SHA256
ee3c0971b78b1c3a102b9b1cafb2021e685bbe1933a44a269b1f5c98e3452fff.
Private original and machine analysis are in
.local/research/keydous-nj98v4s-log31-20261107/. Seven complete snapshots, with
consistent final summary and no reported log queue loss. Revision markers
bootstrap/mode4 and table_order=send_time identify the reconciliation path.

## Startup and releases

- Receiver3151:5038 and remote3793 prove successfully. The first four depth
  pages finish at517953, 313ms after session begin. They show positive values
  on all97 mapped slots, with peak345 and simultaneous nonzero analog on all
  eight traced assignments. This is a suspicious startup snapshot. The log
  does not establish physical key state or whether a stale or misreported table
  came from the receiver.
- The first session requests streaming and records zero stream packets. E5
  repeatedly sees send0/read0, including31 transients before F7 reports the
  remote offline at524343. No Keydous exclusive-open sharing error is recorded.
  The reason the receiver reported offline and why its command channel was
  busy remain unknown.
- The receiver subsequently proves the remote again. Bootstrap finishes at
  532703, 8.360s after the offline transition, and initially reports zero
  depths. Later stream data reaches8096 valid packets with recorded stream I/O
  error0. At the final native snapshot there are17925 combined updates and one
  failure. The warning is set at547671 and clear at548687 and later snapshots.
- Table reconciliation applies two real zero corrections at588687 and648687.
  This demonstrates the fallback working in this run. The tester noticed no
  stuck keys, while long table waits remain possible. The largest complete
  table sweep logged is17.8464s, separate from the direct stream.

## Near-full output

All eight traced assignments top out at native863/1000 and curve output943/1000.
Stick extrema reach -30894 and +30893. Raw peak345 at100/mm corresponds to
3.45mm on the protocol scale, normalized against the configured4mm full scale.
The sampled global curve has upper input cutoff900/1000 and output cap1000/1000,
with no per-key overrides. The native peak remains below that cutoff.
The previous wired run and older 2.4 GHz runs show the same native/curve ceiling.
The trace contains near-full plateaus lasting seconds and changes in native depth,
but does not capture the reported transition to100%. There is no basis to assign
that transition to RF transport, table reconciliation, or a physical switch
calibration change. An upper input endpoint of85% on the affected bindings
would let the observed86.3% native peak reach full gamepad output, as a profile
adjustment rather than a change to the device scale.

An untouched startup is a control for the first table contents. A single
partially depressed key cannot account for positive depths on all97 slots.
The log captures an aggregate and traced values. It leaves the cause of the
initial table contents and of the offline status open.

## Revision 5 host correction

The host treated a successful HID write of 1B as proof that the keyboard was
streaming. That immediately enabled indefinite freshness for all depths when
F7 was online. In the failing first session there were zero 1B depth packets,
yet the traced values stayed at their startup levels for 6.390s until offline.
A replay with the pre-change production liveness and publication code retains
the 863/1000 startup value at the same elapsed time with zero stream packets.
The official driver and HallJoy agree on the F7 keyboard presence bit and
online byte. The prior acquisition could establish the official host exchange,
while the exact receiver and keyboard MCU images remain unavailable.

Only a valid depth packet for a mapped physical slot now confirms streaming.
The initial table uses the existing 1500ms age guard per sample. Confirmation
does not extend the age of those old samples. A valid delta sample or a table
reply requested after confirmation can retain that slot's value while online.
The old stale value cannot reappear after the first real packet for a different
physical key, including when the keys share an assignment. Genuine quiet holds
in an established stream retain their values. There is no new poll delay or
change to scaling, commands or the reconciliation order.

The ordinary log marks revision 5. It records first-packet evidence separately
from the start request and adds the last parsed F7 device mask and keyboard state.
The first E5 table gets aggregate minimum/maximum, mapped and unmapped positive
counts, and repeated-page count. These counters help distinguish a per-key
snapshot from repeated or broadly populated data without serializing payloads.
They do not establish the firmware origin of the original table.

Use the revision 5 build for a startup and isolated full press/hold/release
retest with ordinary Open log. A new failed-start report is particularly useful.

## Support status

Experimental. The release correction has hardware evidence, and the current
RF startup and upper-range behavior still need characterization. Fresh live
Main!A471:C473 on 2026-11-07 confirms Keydous / NJ98-CP V4S TMR at C472 with
Implemented; awaiting hardware testing, strict dropdown, and matching yellow
base/effective colors. All284 live yellow rows match runtime notices. No Sheet
write was needed. README already reflects the USB-confirmed and experimental RF
scope. The revision 5 correction retains this support status.

## Revision 5 validation and delivery

The legacy replay, new startup and alias regression, 30s quiet hold, native suite
and ordinary Release gates passed. The linked profile and layout checks include
the 97-key V4S ANSI preset. All87 Supported layout entries reconcile with the
fresh Sheet snapshot. The delivered EXE equals the checked candidate.

build/bin/Release/x64/HallJoy.exe, version1.6.14.0, 11367424 bytes.
SHA256 ac7cd1f55bb2dba918ea7a13207156298d3f2afeb13cd78333fa7d7428830b92.
Hardware retest remains pending. See startup-native.log, startup-build.log,
legacy-startup-proof.txt and startup-rev5-delivery.json in the private task
directory. Verified source backup in .local/backups/keydous-nj98v4s-startup-proof-20261107.zip.
