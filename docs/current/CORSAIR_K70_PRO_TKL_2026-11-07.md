# CORSAIR K70 PRO TKL stock analog feasibility — 2026-11-07

The wired magnetic K70 PRO TKL is K70 PRO RGB TKL in the official firmware
registry, USB `1B1C:2B14`, Bragi. The [manufacturer manual](https://www.corsair.com/us/en/explorer/gamer/keyboards/k70-pro-tkl/)
specifies 61 ANSI, 62 ISO or 65 JP MGX positions plus conventional switches.

## Exact stock firmware

The official [firmware list](https://www.corsair.com/firmware-storage/firmware/public/fw/k70-pro-rgb-tkl_1b1c_2b14.json)
offered three Marshall images. Package and image SHA-512 matched vendor
metadata. The tester's installed version is unknown.

| Firmware | Dispatcher | Image SHA-256 |
| --- | --- | --- |
| 1.6.37 | `0x26CFC` | `d27e88915e59a08903592b7e1e712762f793ec56aef1d4b2b56b866472956e03` |
| 1.8.42 | `0x28360` | `f13e94ad6b94be0ffe72466a0d57896a5665411573f9b6479d1cd686874a4229` |
| 1.12.54 | `0x28650` | `f2f8515ab8058aa3126f3c8ae7ba14d0162e3a6681ac5cbf1f991c74481b219f` |

## C6 and C7 read path

The two-byte Bragi request header precedes the command-2 little-endian
property at bytes 2–3. `C7` copies the 16-bit word at offset `0x10` of each
38-byte sensor record. The 1.12.54 receive parser at `0x25C38–0x25C8C`
validates incoming bytes and stores a 14-bit sample in that slot, which the
analog scan code also consumes. `C6/1` copies 80 calibration bytes. `C6/2`
and `C6/3` copy the endpoint fields at offsets 6 and 4, respectively. The
properties start reply data at payload byte 3.

The private `replay_marshall.py` executes the actual Thumb dispatcher on
all three images. All 60 cases matched the expected bytes, including four
modeled counts, all three C6 reads, and C7 before and after changes on two
independent positions. The transmit-ready byte was the only non-stack
write outside the reply buffer. The replay models RAM inputs and omits
peripherals, real USB timing, initialization and physical key motion.

The pinned [OpenLinkHub transport](https://github.com/jurkovic-nikola/OpenLinkHub/blob/76e6b3f2b99cf6cd9614223a741ab54d8a6d3f2d/src/devices/k70protkl/k70protkl.go)
uses 256-byte protocol buffers and adds a report-ID slot on write. The
current HallJoy VANGUARD route uses a 1024-byte payload and different C6
subcommands. Marshall needs its own framing and mapping within a shared
architecture. The K70 MAX firmware result is independent.

## Hardware identity from the first tester log

`HallJoy (33).log`, 2026-11-07T12:33:32Z, HallJoy 1.6.14.0 schema 2,
SHA-256 `796e46890df9937c2961a10acff3511b41aec283ed068ec7e728ca36b1da7598`.
Windows enumerated `1B1C:2B14` on interfaces 00 through 03 with the K70 PRO RGB
TKL Magnetic Mechanical product string. The four complete snapshots show
the 26-backend catalog without a Corsair backend and no detected analog
keyboard. An early generic backend-init issue was followed by Active state.
There are no bound axes or analog samples. The virtual Corsair HID entry
does not establish iCUE contention. The report contains no keyboard firmware
version, region, or report capabilities.

## Read-only identity and regional sensor mapping

The pinned host code writes `00 08 02 ...` (leading HID report ID 0) and
reads a 256-byte reply. The firmware dispatcher returns payload prefix
`00 02 00` with the `08` host header. Read-only properties `11` and `12`
return the hardcoded VID and PID. Property `13` returns major, minor and
16-bit little-endian build. Property `41` returns the region byte used
both to select the factory sensor count and to translate sensor index to
physical key identity.

In 1.12.54 the count table at `0x4DE52` has 5-byte entries. The sensor-to-
position table at `0x40B70` has 102 bytes per region. The mapper at
`0x38930` resolves positions via `0x405E0` and applies the JIS-specific
translation. The analog scan calls this same mapper. The private
`replay_identity_map.py` executed the four property reads for all four
modeled regions on each image (48 replies), and the mapper on every
initialized record (774 results). All versions produced identical maps.
A second C6/C7 replay using host header `08` and actual counts 63, 64 and
67 passed all 45 cases, including changes to two independent positions.

| Region | Geometry | Sensor records | Unique mapped keys | Space index (zero-based) |
| --- | --- | ---: | ---: | ---: |
| 1 | ANSI | 63 | 61 | 62 |
| 2 | ISO | 64 | 62 | 63 |
| 3 | Brazilian code, mapping excluded | 64 | 59 | 5 |
| 4 | JIS | 67 | 65 | 66 |

ANSI, ISO and JIS have two unmapped records each. Their unique magnetic
key counts match the manufacturer specification. Region 3 maps six records
to usage `27` and needs independent evidence before admission. The replay
supplies the firmware/region bytes in RAM and verifies the actual getter
and mapper. It does not identify the tester's physical layout or firmware.

## Targeted 1.12.54 implementation

The owner requested automatic region detection with current firmware. The official
[CORSAIR WEB HUB](https://www.corsair.com/web-hub/) bundle uses Bragi property
65 (`0x41`) in `getPhysicalKeyboardLayout`. Its first connection defaults
1 to ANSI, 2 to ISO, 3 to Brazilian and 4 to Japanese. User-selected regional
legends are stored separately by the Hub. Source snapshot `hub.js` SHA-256
`3ac5e45206ec55cd677c21f938ea5524446a1e4554f91bdabe8b94dae6999b27`. This is evidence from the web configurator and matching K70 firmware
getter, with desktop iCUE internals unverified.

Protocol 34 admits exact wired USB `1B1C:2B14`, the control IF1 used by
[pinned OpenLinkHub discovery](https://github.com/jurkovic-nikola/OpenLinkHub/blob/76e6b3f2b99cf6cd9614223a741ab54d8a6d3f2d/src/devices/devices.go),
vendor-page caps and 257-byte Win32 reports including the zero report ID.
The read-only properties must prove VID, PID, stock 1.12.54 and the region.
The first structurally valid C7 reply publishes both the native depth source
and the matching built-in preset token. The catalog includes 84 ANSI, 85 ISO
and 88 JIS key positions (61/62/65 magnetic). Geometry uses the exact-model
manual, the pinned US/DE OpenLinkHub references and the JP2 retail top view
(SHA-256 `a23ce90c051457cf57c839087d7ac358827943192ca93795e964f1070683ce15`).
The physical layout can be selected without asking the tester for a photo.
The Brazilian map remains excluded because six slots share HID code 27.

C6/3 supplies the lower endpoint, C6/2 the upper endpoint. The firmware
initializer at `0x20F8E–0x20FD2` confirms rising ADC and integer
`(bottom - rest) / 500` scale. Calibration rest is read twice to reject
an unstable initialization. The runtime adopts the three factory transfer
curves at `0x4DF92`, per-slot curve selection at `0x310E4` and strict
threshold search at `0x277F4–0x27842`. A branch replay matched 200 inputs
around threshold boundaries, spanning all three curve groups and two
calibrations. The result is 41 depth values including zero (nominal 0.1 mm
steps). Subthreshold motion is zero. HallJoy polls the entire sensor table
without a fixed interval or output smoothing. It uses factory key identity;
iCUE remaps are outside this implementation.

C7 copies the existing live ADC state for every slot, including simultaneous
values. The dispatcher replay shows no configuration, mode or calibration
writes. Actual typing coexistence, continuous USB timing, release under load
and physical range remain hardware test items. The client expires stale depths,
clears publication on I/O failure or stop, cancels and drains overlapped I/O
and releases the exclusive handle. It issues no cleanup command. A configurator
holding the same interface can prevent open. No proprietary firmware is part
of the EXE. Older and unknown firmware require independent admission review.

Ordinary `evidence.k70.*` records include discovery, HID caps, open/USB
failure, actual firmware and region, calibration status, live proof, measured
frame rate, aggregate depth changes, simultaneous keys and release counts.
Use **Open log** for the tester report. Scope and outcomes below apply to
the local test build.

Private images, pinned sources and `read-replay.json` are in
`.local/research/corsair-k70-pro-tkl-20261107/`.

On 2026-11-07, Main!C182 changed from Not investigated to Research incomplete.
Main!A181:C183 readback confirmed the model, dropdown, gray base/effective
B:C fills and neighboring rows.

## Local tester build and catalog status

`build/bin/Release/x64/HallJoy.exe`, app version 1.6.14.0,
11416576 bytes, SHA-256 `91cadc134760d1647fff4b79911fff08af760664321caca4af26bcef1c22f61f`.
The unified native suite, ordinary Release build, full native catalog and
production-linked profile/layout and startup recovery gates passed. The compiled layout
catalog confirms all three key counts and tests the generated token through
Automatic layout selection. Physical validation remains pending.

After the build, `Main!C182` changed from Research incomplete to
Implemented; awaiting hardware testing. Readback of `Main!A181:C183`
confirmed the new value, unchanged dropdown and neighboring rows, and the
matching base/effective yellow fills in `B182:C182`. The fresh 669-row model
snapshot passed the full 285-model runtime yellow notice reconciliation.

Tester workflow: update the keyboard to 1.12.54 if needed, exit iCUE,
run this HallJoy.exe with the keyboard wired and leave Automatic layout
enabled. Bind W/S to the left stick Y axis. Check slow press and release,
a held partial press, simultaneous keys and normal typing. Send the
ordinary HallJoy.log from Open log after the test.

Backup of edited inputs and the prior ordinary EXE is in
`.local/backups/corsair-k70-tkl-native-20261107/`. The layout generator
saved prior generated files in `.local/backups/layout-integrate-fnocnoit/`.

## Tester confirmation and startup fix

The tester reported successful operation. `HallJoy (34).log` UTC
2026-11-07T13:13:41Z, SHA-256 `7158125b1b6b32e2afcef00ba2bfcdd2e92eaab3320bb5399281d5b0fdef234b`, confirms stock
1.12.54, wired ISO (region 2), control IF1, usage `FF42:0001`, and 257-byte
Win32 reports. All 62 magnetic positions passed calibration. Six complete
snapshots show no queue loss. The final native snapshot has 117,985 successful
reads and zero backend failures. The second session sustained approximately
1.48 kHz in its 78.7-second interval, measured from its validated C7 replies.
This is a host read rate. Unique sensor scan rate and in-game latency were
outside the log.

The ordinary pad trace contains 753 records. The four bound keys (W/A/S/D)
show intermediate native depths, full native depth 1000 and release to zero.
Both left-stick axes reach -32768 and +32767 and return to neutral. The
retained analog summary reports up to three simultaneous keys. No right-stick
or trigger binding was configured. Region proof establishes ISO for the
session. The report does not capture the UI preset displayed.

An initial session ended at the same timestamp as a generic Windows device-tree
notification, after 1,469 valid reads and with zero I/O failures. The same
keyboard reopened about one second later. The old K70 worker incremented its
topology generation for every such notification and canceled its own transfer.
Revision 2 uses notifications to wake discovery. An active session instead
follows its HID handle, with bounded I/O failure handling and cancellation
on Pause/Exit. The log does not identify which device generated the event.

Supported scope is wired `1B1C:2B14` on stock firmware 1.12.54, with
device-selected ANSI, ISO and JIS presets. ISO was exercised on hardware.
The ANSI and JIS maps remain covered by the firmware and compiled-catalog
checks. The other firmware/region gates and iCUE contention handling still
apply. The connected experimental advisory is removed. A recognized keyboard
whose proof fails retains the model-specific troubleshooting notice.

Revision 1 source, previous executable and status inputs were backed up to
`.local/backups/corsair-k70-log34-supported-20261107/`.

Revision 2 local Release EXE, app version 1.6.14.0, 11416064
bytes, SHA-256 `60d591d6c3ed2c63bc131aca78bbd9d0dd719322ccd1a9451614be976aae9498`. Native suite,
full-catalog, production profile/layout and startup recovery checks passed.
A private Win32 transport test passed unrelated notification, actual I/O
failure and stop/cancel/drain with mocked I/O. Revision 2 remains to be
exercised on the tester's keyboard.

Added all three compiled presets to `supported_layouts.json` and passed the
intended-state check before the Sheet edit. Main!C182 changed to Supported.
Readback confirmed the value, strict dropdown, neighboring rows and green
base/effective fills in B182:C182. The fresh 669-row snapshot passed the
88-model Supported-layout gate and the full 284-model yellow notice audit.
