# ARDOR GAMING Viper investigation — 2026-11-13

Current delivery uses the full raw matrix; see the final verified matrix delivery
section below. Earlier event-based trial notes are retained as superseded history.

## Initial source inspection

The owner relayed a tester offering an ARDOR GAMING Viper, reported as
0416:7372, MI_02, controller M484, product GK8163HERGB, firmware V1_13_08.
These are screenshot-reported values. No full HallJoy.log for this device
has been supplied or analyzed. HallJoy had no Viper admission at initial inspection.

The ordinary Witmod backend already enumerates 0416:7372. IsExpectedDevice
in irok_na87_protocol.h accepts M484 / GK8260HERGB for IROK NA87.
ReceiveIdentity in irok_na87_backend.cpp additionally admits exact AJAZZ
and HZ-94 identities. GK8163HERGB matches none of those. This explains
rejection for the reported identity, conditional on the actual HID caps
and identity reaching that branch. The actual refusal stage needs its log.

Viper is a credible candidate for the existing Witmod family. Shared USB
identity or controller alone does not prove compatible sensor semantics.
No runtime, support status, compatibility sheet or EXE changed during this
investigation. No physical USB access or firmware flashing occurred.

## Official acquisition

[ARDOR driver routing](https://ardor-gaming.com/drivers/) directs users to
DNS. The [Viper driver page](https://www.dns-shop.ru/product/driver/26e5d7a95567c777/klaviatura-provodnaa-ardor-gaming-viper-cvet-cernyj/)
provides the following pinned archives, downloaded and unpacked offline.

- [Black driver](https://drv.dns-shop.ru/drivers/oth/A/ardor-gaming-viper-ag-zd-vi83gy-b-hs-ms-gm-ds_instrukcia_151256_24122024.zip),
  28072705 bytes, SHA-256
  `be6c2d243f6ee650c84fcf91f9e41055fd195c650aa694c644990f4cdc7d23d2`.
  Installer V2.06.06.
- [White driver](https://drv.dns-shop.ru/drivers/oth/A/ardor-gaming-viper_drajver_171416_08082025.zip),
  35225779 bytes, SHA-256
  `7b68bdf8f0c848302ccd9ef5c2f2cac03340b1924f622cf63ee1974b194ed60a`.
  Installer V2.06.10.
- [September update](https://drv.dns-shop.ru/drivers/oth/O/obnovlenie-0925_drajver_190815_18092025.zip),
  2425328 bytes, SHA-256
  `ab9a503b038ea34114aa7bfea2b170f5762b1ed18c71eeee2b6836e8dd9f7f9e`.
  Updater YBY101CKB_M484KBJC_G870 viperr RGB V1_13_06.exe.

Vendor installers, DLLs, artwork and firmware remain private under
.local/research/ardor-viper-20261113. None was executed or redistributed.
The existing local innoextract unpacked the installers statically.

## Producer and geometry

Black witmodSdk.dll at file offset 0x2C395 links hidName GK8163HERGB to
likeName ARDOR_VIPER, keyNum 82 and M484 XXX. White witmodSdk.dll retains
that identity and adds GK8163WHERGB / M484 YBY333 / VIPER at 0x2CA83.
The white variant requires independent protocol verification.

Black ARDOR GAMING.exe registers Qt v1 resources at VA 0x6029E4 with
tree 0xC5EC70, names 0xC59368 and data 0x69A090. Offline traversal found
KeyInfo_GK8163HERGB.config, 15577 bytes, SHA-256
`da775be6c729e0b039b27c8f54d459e22e0fcfba8ab5505b700d0e4751869cc0`.
Its six primary 22-slot rows contain [14, 15, 15, 14, 14, 10] visible keys, totaling 82.
A seventh row contains nine locale alternatives, which do not independently
prove another retail regional variant. Exact matrix and geometry are
available for a Viper-specific preset. The vendor's JSON-like file has
leading-zero numbers that need normalization for strict JSON parsing.

CWitmodHid_HidWriteBuff is exported at VA 0x100083E0. A 64-byte buffer
and opcode 0x21 assignment at 0x100085BB provide a concrete producer lead.
The full Viper subscription/event/cleanup sequence remains unverified.

## Firmware and investigation limits

The update executable contains an ARM vector array at file offset 0x2CFE08.
Its loop at VA 0x4057E9 reads 0x40000 bytes from VA 0x6D1008. The exact
262144-byte array has SHA-256
`1653a398c93ed4555948eeba2ef5213430575e55b270b94c1115e82f71d619ee` and identifies
M484,01,KB,JC,GK8163HERGB,V1_13_06 at offset 0x1FE1C.
Although the updater filename says G870, its image identifies GK8163HERGB.

V1_13_08 in the black installer belongs to GM081XHERGB. An intermediate
user-facing claim that the exact tester firmware had been found was
corrected. Exact Viper V1_13_08 bytes remain unavailable. Viper 1.13.06
has not yet been replayed. Full sensor semantics, typing coexistence,
multikey/releases, settings persistence and cleanup remain to be reviewed.

Next implementation work must follow FIRMWARE_BEHAVIOR_REVIEW.md and
TESTER_BUILD_CONTRACT.md, include this model's exact layout/autoselection,
and exercise production discovery/subscription/sample/release/cleanup.
Benign remaps, assignments or firmware-string differences must not veto a
proven safe physical analog path. Normal HallJoy.log must retain concrete
refusal stages and bounded structural evidence. Support destinations are
synchronized once the actual implementation warrants a support change.

## Implemented stock trial

The local ordinary EXE now admits exact M484 / GK8163HERGB on the reviewed
Witmod transports and publishes its own 82-key ANSI layout token. Firmware
version text is recorded and does not veto 1.13.08 or another compatible
revision. GK8163WHERGB remains a separate unreviewed identity.

The firmware review executed the real 1.13.06 dispatcher 0xA6F8, command
handlers, accepted-change fragment 0x8742..0x87CE and scheduler 0xA70E.
Capability is 40, subscribe/unsubscribe preserve cache, and raw ADC mode
changes only bit19 of USB RAM flags. A pair of accepted changes before
service emits only the last coordinate. This includes an overwritten zero
release. The validated local assessment.json is LIMITED_ONLY, with
simultaneous-key, release and packet-loss restrictions for this exact image.
1.13.08 bytes were not found, so the same behavior is unverified on the tester.

The physical scan table 0x13650, logical pointer table 0x13A60 and mask
0x1FE60 jointly select 82 distinct logical positions, exactly matching the
six primary rows of KeyInfo_GK8163HERGB. The larger 132-pointer logical
table alone does not identify physical keys. Fn is included as HallJoy's
existing physical Fn code. The derived layout report contains numeric
geometry and original labels, with no copied vendor image or executable.

The reviewed stock depth stream is retained for the trial. Raw 0x23 rows
observe all 132 ADC slots independently and never synthesize depth, zero
releases or guessed per-key calibration. Addressed 0x21/05 reads settings,
not the live depth cache. Calibration/factory commands 0x21/06..08 can
change settings and are excluded. An unrestricted calibrated full-snapshot
output route remains unresolved. This trial cannot claim that 1.13.06's
firmware event-overwrite problem has been fixed.

## Admission and executable coverage

Discovery uses the existing exact VID/PID, FF1B:0091 and 64-byte I/O gate.
Other ownership, metadata/open failures, changed transport identity and
cancellation retain their original bounded stages and errors. Only the
read-only identity query precedes model proof. Exact controller/product
proof precedes capability, optional assignment reads and volatile stream
control. The source is connected only after a valid physical depth sample,
including zero. Incompatible scale, foreign product/controller, real I/O
failure, routing conflict and cancellation stop the corresponding operation.

Assignment count, extras, duplicates, unknown codes, disabled keys, remaps,
missing/inconsistent rows and unavailable layout-cache metadata do not deny
the physical analog attempt. The exact matrix owns the sensors. Missing
aliases keep factory/manual interpretation available. Session revalidation
and cleanup use the same production functions as ordinary operation.

The registered exact-image Viper self-test exercises 14 positive fixtures
and four negative fixtures over overlapped private pipes. It includes the
reported 1.13.08 string, 1.13.06, another padded version, benign assignment
variants, first zero, independent W/A depths, releases, automatic preset
identity and unsubscribe/raw-disable. It opens no physical HID or gamepad.
These are software checks, with no claimed physical Viper test.

## Open log evidence

The ordinary log records collection caps, open and identity results,
capability, optional map policy, subscription, first depth, aggregate depth
motion/releases/peak, raw row coverage and cleanup. Unknown raw envelopes
retain bounded header fields. Continuous logging stays optional.

The worker caches aggregate raw/event counters as data arrives. The log
worker collects that cache before every snapshot, including the report
requested by Open log. This requires no extra device command and no minimum
tester duration. It remains independent of the Keydous receiver-trace hook.
A native Windows writer regression requires collector evidence to appear in
the same requested report.

Raw changes without a depth event can be normal quantization or filtering.
zero_references counts channels with an observed ADC value paired with a
firmware zero-depth state. zero_reference_returns counts later returns to
that measured value while the cached firmware depth is nonzero. This is
candidate missed-release evidence, with possible hysteresis or report-order
ambiguity. It never clears the key automatically. No per-key ADC values,
key identifiers, serials or paths are added to these records.

## Compatibility synchronization

Main!C61 changed from Not investigated to Implemented; awaiting hardware
testing. Readback verified the status dropdown, yellow base/effective B:C
colors, other formatting and adjacent cells. A fresh full bounded read of
670 model rows found only Viper changed. Live notice audit passed for 283
models, and the compiled catalog/live Sheet check passed for 91 Supported
models. Viper remains Experimental in README and hardware documentation.

## Historical event delivery (superseded)

The final native suite passed, including the new Windows writer regression
that collects aggregate state in the same requested snapshot. Exact ordinary
EXE tester checks passed for all five registered families; Viper passed
14 positive and four negative cases. Production profile/layout and startup
recovery gates passed. The final compiled catalog contains the Viper preset.
The formal firmware verdict remains LIMITED_ONLY for the reviewed 1.13.06
image, independent of these software test results.

Ordinary delivery is build/bin/Release/x64/HallJoy.exe, version 1.6.15.0,
11622912 bytes, SHA-256
`44d2e23002128621006044576585751be0811037101b6b48fa42d99f79157ab6`.
The installed file matches its exact-image receipt. Earlier 195c1c2a is
superseded by this build's current-state Open log collector.

Tester workflow is to close the vendor configurator, run this EXE with the
wired Viper, use the keyboard normally, observe analog/gamepad behavior,
and send the ordinary HallJoy.log through Open log. No prescribed test
duration is required. 1.13.08 has not been physically tested by the agent.

## Superseding owner decision and matrix route

The owner rejected the known-loss event trial above. Its EXE is historical,
not the recommended tester delivery. The follow-up instruction explicitly chose
no manual calibration for the next trial, using ordinary operation and Open log
to assess the automatic range. This does not establish exact calibration.

The revised route unsubscribes0x21 depth events and enables0x23 raw matrix rows.
Each valid row updates its physical positions immediately. It never depends on
a stock depth event, digital state, release timer or additional temporal filter.
Disabling the event subscription also avoids its higher pending-bit priority
competing with the raw matrix. Six rows cover132 slots, of which82 are physical;
the rows are not claimed to be a simultaneous atomic scan. Transport cadence
and physical USB reliability are not established by local emulation.

Strict replay in .local/research/ardor-viper-20261113/audit_full_matrix.py uses
the shared read/execute-only image and read/write-only RAM runner. The actual
initializer0x8C66 sets all 132 lower references to150 and upper references to0.
The actual raw serializer/scheduler returns all 132 values over six rows, with
independent modeled press/release values on all 82 physical positions. A later
cycle again carries the release values for a previously omitted row.

The saved lower and upper references remain unavailable through the reviewed
addressed settings reply. Two synthetic states with upper references300/340,
identical remaining settings and sensor250 return identical settings packets
but compute depths20/27 in the actual conversion code0x86D4..0x872C. Thus neither
that reply nor a raw reading alone proves the correct calibrated depth.

The provisional host model uses each channel's observed maximum(raw>>3) as
the upper reference,150 as the known default lower reference, and the reviewed
integer 1.2-gain/40-scale transfer. It is not a saved-calibration read. Starting
with a held key can underestimate the initial range; seeing a higher value
updates the reference. Noise/drift and a saved lower reference different from150
can affect shallow input or range accuracy. These are explicit trial limits.
No claim of perfect analog, calibrated millimetres or guaranteed absence of
physical sticking follows from this implementation.

Capability/event scale is optional metadata for this raw route. Both a scale41
and an absent capability reply must still reach raw reads. Identity and raw
packet framing remain required. The production-image cases cover these along
with all previous assignment variants. No metadata mismatch enables probing a
different product or controller. Raw ADC>400 after scaling is rejected because
the real scan code rejects it; a reference below190 cannot supply the actual
nonzero integer denominator. Both conditions leave diagnostic counts.

Ordinary Open log snapshots include current matrix and provisional-range
counters, without waiting for a fixed duration. They identify row coverage,
sample/release/peak/current counts, invalid rows, convertible channels,
reference adjustments/returns, full-output coverage and aggregate extrema.
Returns and extrema are evidence, not physical calibration certification.
The output source, event_subscription=0 and calibrated=0 are explicit.

Additional bounded review:0x21/05 exposes only settings lanes0,2,1,6,7 of each
eight-byte record, excluding calibration lanes3,4,5.0x21/06..08 change calibration
state and are not sent. Bulk configuration/macro commands are writes; no normal
calibration-read route was established from them. The updater uses application
reset/version and flash-writing flows; no reviewed calibration-read-only flow
was found. A USB class GET_REPORT/GET_IDLE implementation has an unchecked
wIndex byte-read address, but ordinary Windows HID fixes wIndex to the interface;
no safe stock-HID full-memory route was established. Descriptor string indices
are bounded below8. None of these findings is an exhaustive proof of absence
in the unacquired1.13.08 firmware.

Bound gamepad keys additionally retain current ADC, provisional reference and
output permille as trace.viper_matrix, under the existing bound-key trace privacy
exception. This is collected from per-field cached values by Open log, rather
than requiring a particular test duration. Unbound keys are aggregate-only.

## Verified matrix delivery

Ordinary EXE: build/bin/Release/x64/HallJoy.exe, version 1.6.15.0,
11,628,032 bytes, SHA-256
`6db2a0d1041ff788ae8c658448d861c56d6eb377e97fb6839c849162de9e979f`.
The installed image matches its exact-image receipt. Prior0f025453 was replaced
by this build's expanded ADC diagnostics;44d2e230 was the rejected event route.

The exact ordinary candidate passed all five tester families. Viper passed 16
positive/four negative production-pipe cases, including zero-first sample,
independent W/A, all 82 simultaneous presses/releases, an omitted release row
followed by a fresh row, ignored stale stock event, assignment variants,
capability 41 and absent capability metadata, automatic ANSI selection and raw
cleanup. Native checks passed; final diagnostics changes additionally passed
release writer, compiled-image, full-catalog, profile/layout and startup recovery
gates. No physical Viper test or visual run was performed by the agent.

full-matrix-audit.json and assessment-matrix.json are pinned local receipts.
The validated formal suitability verdict is LIMITED_ONLY, selected
raw_matrix_provisional_range, with depth-scale and physical-release accuracy
restrictions. The event-loss dependency has been removed from host output;
provisional range accuracy, noise/drift and the tester's 1.13.08 remain unverified.
The normal report identifies calibrated=0 rather than claiming those are solved.
ADC extrema also include samples rejected by range conversion. Bound-key traces
show converted=0 when the current ADC/reference cannot be converted, while milli
identifies the retained published value. Both evidence records fit the actual
384-byte writer with its uptime prefix, including worst-width counters.

Live Sheet re-read: exact ARDOR GAMING / Viper remains Main!C61 = Implemented;
awaiting hardware testing. B:C yellow base/effective colors and status validation
were confirmed; no Sheet write was necessary. A fresh bounded full read passed
all 283 yellow notices and all 91 Supported/catalog checks using the final compiled
layout catalog. README remains Experimental. No Supported promotion/publication.

Tester flow: close the vendor configurator, start the ordinary EXE with the
wired Viper, use it normally and observe the analog/gamepad response, then Open
log and send HallJoy.log. No manual calibration, timed hold or waiting period is
required. A good-looking run is useful feedback but is not mathematical proof of
absolute calibration. Backups and exact source/build hashes are retained in
.local/backups/ardor-viper-matrix-20261113/ and matrix-verified.json.

## Tester feedback, 2026-11-15

The owner relayed that the tester says everything works well. This confirms
operation of the wired trial in the tester's use. The reply includes no fresh
HallJoy.log or measured sensor range. The Experimental classification still
reflects the provisional range conversion described above.
[Tester follow-up](KEYBOARD_TESTER_FOLLOWUP_2026-11-15.md).
