# AULA F75 HE / Akko 5087 intake and GT75 request — 2026-11-13

Scope: review the owner's two newly downloaded reports and the accompanying
MCHOSE GT75 request screenshot. No runtime edits, new EXE, firmware writes,
physical test, support promotion or publication were performed in this review.
Private original copies and hashes: .local/research/aula-f75-akko5087-gt75-20261113/intake.json.

## AULA F75 HE — HallJoy (50).log

Report1.6.15.0, UTC2026-11-13T17:26:52Z. Twenty snapshots; latest535 has all28
providers, consistent summary and no observed logging queue loss. No provider
reports connection or presence in these snapshots. Observed keyboard inventory
is 0C45:FEFE, interfaces00..04, product AULA F75 HE Dongle. No wired F75 endpoint
appears in the supplied inventory. Inventory is metadata, not analog proof.

Mini60 evidence counts receiver collections but explicitly says receiver_commands=0
and connect_usb_required=1. In aula_mini60_diagnostic.cpp, the inventory recognizes
FEFE/FEFC as receivers and skips them. This is a deliberate absence of a receiver
implementation, not an observed failed analog command or broken keyboard. Other
providers' lifecycle errors are not evidence that this receiver denied access.

The September HFD registry recorded wired F75 HE38A6:2729; that older registry
is not a measurement of this tester's wired hardware. Obtain a normal wired
HallJoy.log to establish the actual endpoint/revision before selecting firmware
or commands. No assumption that this receiver uses the Mini60 wired protocol,
and no conclusion that wireless analog is impossible, follows from this log.

## Reported Akko 5087 HE V3 — message (25).txt

Report1.6.15.0, UTC2026-11-13T18:08:09Z. Five complete/consistent snapshots,
all28 providers, no observed queue loss. USB3151:5029 reports Akko Keyboard on
interfaces00/01/02. The intended MI_02 collection initially returned Win32 sharing
violation32, then opened successfully with feature length65 and usageFFFF:0002.
Busy discovery resolved in the third recorded pass; it is not the final blocker.

Three repeated identity attempts return id=3131, repeated_id=3131, command143
(0x8F), report_id=0. In attackshark_pro_diagnostic.cpp Session::Identify, paired
identity acceptance also requires Known(board,pid). Current native/paged and
stream profile tables contain no3131 entry. Therefore the paged source stops
at identity admission before E5 depth requests. The RongYuan stream provider's
present_not_connected/failures counter does not establish a malfunctioning
analog transport. No usable depth/release sample was captured.

The retained catalog .local/research/monsgeek-akko-linux/data/devices.json has:
board3131, USB3151:5029, ry5088_akko_5087bv2he_1m_8k, displayName5087S V2 HE,
magnetism=true, keyCount87, Common87_5087SV3, reportRate8000. The same saved catalog
lists5087 V3 HE as board3359/PID5030. This is a catalog/name mismatch to reconcile,
not a reason to tell the owner their reported model is wrong. Preserve the
reported retail name and investigate the actual3131 identity. Do not substitute
another 5087 revision's protocol, physical map or regional variant silently.
The saved catalog is not presented as a fresh manufacturer registry read.

Next implementation work can target exact3131, review the device's full-matrix
paged/stream protocol and authoritative87-key geometry, then include the matching
built-in preset/autoselection and ordinary Open log evidence in the first EXE.
This review does not establish that adding an ID alone is sufficient.

## MCHOSE GT75 — screenshot, no attached GT75 log

The screenshot requests3837:302D, MI_01, usage0001:0000 and65-byte I/O, and reports
that a third-party PID-patched Mix87 EXE remained unstable. These are user-reported
observations, not an analyzed GT75 log or captured packets.

The retained official HUB bundle from2026-11-09 independently lists GT75 as
3837:302D, type138, boot3837:302F, usage1:0, and distinct layout-gt75/default-keys-gt75
resources. The locally named mchose-gt-layout.js is actually Ace68 GT, not GT75;
GT75's layout filenames are referenced in the saved manifest but that geometry
was not loaded/verified here. Current Mix87/Jet75 model tables omit302D.

The existing Ace68 GT3837:3007 investigation is relevant family evidence, not
proof of GT75 compatibility. Mix87 and Ace68 GT use different settings-read
addresses/transactions, including reviewed persistent flag updates. Identical
HID sizes do not justify a PID patch or copying an initialization write to GT75.
Review its actual HUB initialization/firmware and layout before proposing a test.
No claim of an impossible analog path or a proven entirely new protocol is made.

## Follow-up scope (owner, 2026-11-13)

Investigate all three cases, explicitly including wireless AULA. Start with Akko
through a normal test EXE. A wired AULA report is useful additional evidence,
not a prerequisite to researching its receiver. The official saved Akko driver
explicitly names3131 **5087 V3 HE**; the older community catalog label above does
not override the manufacturer. See [Akko implementation](AKKO_5087_V3_HE_2026-11-13.md).

## Original intake result

AULA: supplied report is receiver-only; next useful input is a wired report.
Akko: final observed blocker is missing exact3131 admission/profile; actual analog
behavior remains unknown. GT75: catalog identity corroborates the screenshot,
while protocol compatibility is unproven. Existing support statuses and notices
remain unchanged; no hardware confirmation was inferred. Structural log analysis
passed. No code tests/build were needed for this documentation-only intake.
