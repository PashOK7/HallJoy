# AULA F75 HE — deeper producer review and firmware-only experiment

The owner rejected another tester run of the same status68/simulation66 paths
with expanded logging. Continue the investigation locally. No new HallJoy EXE,
device command, firmware installation or public support promotion was performed
in this work. The installed revision3 EXE remains the prior image, not a newly
discovered analog solution. See [the model record](AULA_F75_HE_2026-11-13.md).

## Fresh official source, rather than the old public download

The official desktop archive's retained client was1.1.22. On2026-11-16 the
[manufacturer update API](https://hubapi.aulacn.com/user/Client/getNewVersion)
reported client2.7.1, hot update5. The actual archive was acquired from
`https://cdn.aulacn.com/client/clientHot/dist_v2.7.1_5.zip`,39,004,150 bytes.
SHA-256 `9d0691010aa750d8fcaadd3aa01c3a49906f33e65613e53edae120c2e679404f`
matches the manufacturer's metadata. A403 on a web landing page did not prevent
this acquisition. Vendor code was inspected locally; the configurator was not
executed against a keyboard.

The current index `assets/index-VlA_-jSj.js` routes exact wired2729 and2742 to
layout registryZ1, Info `DnX3_QbB.js`, Trigger `BnUq1ych.js`, Version
`B4o33_oA.js`. Info selects HFD SDK classIN, exported ascy from
`D9v1Pp-7.js`. The decoded exact Trigger still calls `factoryReset(5)` and
`startCalibration` for its travel display. It does not call
`getMagneticAxisStatus`, `startSimulationTest` or `startCalibrationV2`.
The SDK's newer calibrationV2 methods use69/6A. Their presence in a shared SDK
does not establish support by the exact F75 images or usage by its Trigger.
The shared SDK's68 implementation retains eight-byte physical-key requests.

Only string-array decoders and their reviewed array rotations ran in a Node VM
with code generation disabled and bounded execution. Application code, browser
APIs and HID APIs did not run. Derived sources ending `.decoded-v2.js` passed
Acorn syntax validation; earlier decoded files are superseded analysis artifacts.
Producer locators and hashes are retained privately in the evidence manifest.

The new Version module still deliberately selects an empty updater name for
2729, `NEW F75 HE` for2742, and a separate `AULA F75 HE_10070` for the UK model.
The [official UK updater metadata](https://hubapi.aulacn.com/user/EXE/getFile/AULA%20F75%20HE_10070)
provided V1.05 dated2026-09-15. Its named file downloaded successfully from
`app.aulacn.com/commonAssets/`; a403 on that host's root is not an asset failure.
The2,440,704-byte updater hashes to
`619105c1edd3b504ae73e092f76ae6051158d09fef8a069446e049c6e43573b1`.
Static PE extraction of resource10/4000 gives516,096-byte firmware, SHA-256
`74889f23b9171aae09a0b234eb5240e3ed3ff5d4557df4cfb2b0a5b88862f20f`.
It has81 physical positions and shifted code/data addresses. It is a separate
UK/ISO board, not an authorized update for the tester's ANSI2729. No cross-flash
is proposed. Its relevant command/producer behavior was compared locally.

## Actual ordinary depth source and calibration conflict

On2729/1.55 and2742/1.03 the ordinary scanner converts a float at
`20009C5C + channel*4` using scale100.0 (literal7828), then stores a uint16 at
`20002EDE + channel*2`. Instructions7742..7752 copy it to the ordinary cache
`200032DE + channel*2`. The ADC cache is `20003EEE + channel*2`; calibration
status is `2000ACDE + channel`. The channel-to-physical-key map is addressed by
literal77F8:1BB1C on both ANSI images,1BB00 on the UK image. Sentinel125 denotes
an absent position. Mapping uses physical IDs and does not depend on live binds.
The exact current Trigger divides displayed calibration stroke by100. This
establishes the producer's representation, not physical accuracy/resolution.

At7756 and77D2 the calibration flag2000045E controls both FB generation and the
normal BE00 continuation. Calibration enabled produces FB but skips that normal
call. Simulation flag2000045F alone does not enable this producer. The UK scanner
tail has the same instructions. Replaying every physical position at0/1/170/340,
both calibration states and both simulation states yields3,856 scoped cases
across the three images. Normal continuation is reached with calibration off;
calibration produces FB and skips it. This stops before BE00 and is not a full
ordinary keyboard HID/ADC/peripheral simulation.

## Additional stock routes checked

The exact acquired images return request echoes for60/68/69/6A. The prior fixed
ROM getter review remains applicable to the ANSI images. In particular GET16 is
a flash/Fn-settings getter, unlike BOOM68's RAM-reading implementation.

The HID control handler16764 and actual initialized HFD callback configuration
were followed separately from AA commands. Initialized configuration comes from
the firmware's own scatter loader6E4, not an assumption that RAM is all zero.
GET Input Report callback is null. GET Feature callback1A750 accepts reportIDs0
and2 and copies64 bytes from20015F2C, the update/service buffer. It also ORs bit2
into20000442. This is not the ordinary depth cache. The UK callback is1A734;
function/data shifts are not uniformly equal throughout that image.

The actual class request handler was replayed for four interfaces, Input/Feature
types and reportIDs0/1/2/255:96 control cases across three images. Accepted Feature
reads return the seeded service buffer; the other reviewed cases stall. Separate
callback execution covers all256 reportIDs per image,768 cases. EP0 submission
and stall are explicit stubs. Class objects use firmware configuration geometry;
real USB descriptor enumeration, endpoint hardware and Windows HID access are
not modeled. No SET Feature, bootloader entry, updater or settings writes were
sent to a device.

The USB vendor sender19F08 also has pending-buffer branches15F..162, and1B65C
can send24 bytes from0017C/184/18C under flag00179. The latter starts disabled
after actual data initialization. Bounded literal/call searches did not establish
an ordinary sensor producer or host enable sequence for these extra buffers.
Indirect aliases, other firmware, hardware states and peripheral behavior remain
coverage limits. This is not an exhaustive proof that every stock path is absent.

## A concrete firmware modification experiment

There is an independently usable cache; the missing component in the reviewed
stock path is a host-readable reply. A small original Thumb handler was written
and replayed as an **in-memory modification** of each exact ANSI image. It uses
the existing68 format to read up to seven requested physical positions, returning
calibration status, ADC and cached depth. It accepts duplicate physical keys,
marks unmapped/sentinel positions invalid, bounds payloads to56 bytes and rejects
misaligned lengths. It never infers depth or releases from digital key events.

The first experiment intercepted at13456. Further review found that the shared
dispatcher can call64E4 before that point when20000129 is set, clearing held-key
state. That prototype is superseded. The final experiment intercepts at12FF2,
after READY consumption and AA-header validation, before that cleanup. Other
commands replay the displaced flag load and continue at12FF6. Read68 copies its
own eight-byte reply header and returns through the existing ACK code136D0.

The final experiment adds180 bytes at20000 and changes one four-byte branch at
12FF2. These bytes are a local code-placement hypothesis, not proof that the
region is available in every runtime/settings/OTA state. No flashable full-image
file or updater was produced. The isolated payload is original analysis code.

Final replay on both ANSI images establishes the following bounded properties:

-120 seven-key-or-smaller batches cover all80 physical positions over five
  independent depth patterns, including full zero release. Invalid sizes,
  duplicate keys and unmapped positions are exercised separately.
-A write hook permits command writes only to the64-byte reply, READY/ACK and
  bounded stack scratch. Both values of the inherited cleanup flag are seeded
  with nonzero held-key state; read68 preserves that state and the flag.
-The actual ordinary producer tail feeds640 channel passes over four stages.
  Three independent holds include shallow1, half170 and full340; one key releases
  while the others remain held, then all release. The normal BE00 continuation
  remains reachable.96 subsequent batches read this produced cache.
-The existing vendor USB sender makes104 modeled submissions. Eight injected
  busy attempts retain ACK and retry the identical reply successfully. Hardware
  submission itself is stubbed; this is not measured USB throughput or latency.
-Nine pre-existing commands, with each cleanup-flag state, retain identical replies
  and non-stack RAM versus the corresponding unmodified image:36 comparisons.

Execution PASS does not make the modification ready for installation. Remaining
questions include bootloader/update integrity and checksum requirements, reserved
flash/code placement, exact tester whole-image identity, rollback/recovery,
interrupt concurrency, cache freshness and filtering, actual matrix polling
latency, range/rapid-trigger coexistence and physical typing/gamepad behavior.
Calibration already enabled by a different application is not remedied by this
read handler. The UK image was not patched. No unrestricted support is claimed.

## Evidence, next work and delivery decision

Reproducible own source: `tools/review_aula_f75_deep_paths.py`. Full stock/control
review and the first experiment are preserved with their exact source snapshot
in `.local/research/aula-f75-deep-20261116/replay-final/`. Its raw receipt hashes to
`d437579670cd4ecb2af96f15fe54d6a122111a0ca42b0b3d34159409c3861fa0`.
After moving the hook, only the changed prototype was rerun using
`--prototype-only`; the unchanged3,856 stock cases were not repeated.
Final experiment/source snapshot is in `prototype-early-final/`, raw SHA-256
`92d4a85f1fcd17767cce2aad7cfbe1c5c0769ca49cd07289768f8c25aa231094`.
Original180-byte payload SHA-256
`8dc447f86df48da77253e9df1a1ee870239038e41c6ef9edb7ba6274bfc98ace`.
Acquisitions, source locators and file hashes are pinned by
`.local/research/aula-f75-deep-20261116/evidence-manifest.json`.
Vendor assets remain local because redistribution permission is unestablished.

The next local research step is firmware installation/integrity and safe recovery
feasibility for this exact ANSI board, plus cache/scanner concurrency. An existing
stock path can still supersede that experiment if new evidence establishes one.
Another stock66/68 tester run is not justified by these results. A tester EXE
needs a genuinely different, reviewed usable source; a firmware experiment first
needs its own suitability and installation review. Hardware status remains gray
Research incomplete. No Sheet status change is warranted by emulator success.
