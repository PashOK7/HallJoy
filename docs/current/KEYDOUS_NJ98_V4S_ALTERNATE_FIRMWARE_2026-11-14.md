# Keydous NJ98 V4S — alternative acquisition and receiver cache, 2026-11-14

Continuation of [exact-firmware investigation](KEYDOUS_NJ98_V4S_FIRMWARE_2026-11-14.md).
Owner explicitly requests sources beyond the unavailable firmware API. This pass
uses official offline installers, archived official downloads, distributor shares,
public reverse-engineering source/dumps and the existing local firmware corpus.
No target-device command, vendor executable, flashing, new HallJoy build or support
status change. Private root:
`.local/research/keydous-nj98v4s-firmware-20261114/alternate-01/`.

## Offline packages and archives

Downloaded from links embedded in the current Chinese manufacturer page:

| Artifact | Bytes | SHA256 |
| --- | --- | --- |
| Keydous_v4_setup_3.2.19_WIN20260514.zip |106607660|79cb27897911b13f53dd18edc221b0fe1ebdbc28a3962afa63214f70a04aed31|
| Keydous Driver v4 3.2.31_MAC20261021.dmg |210717724|4d28189ec19c2349f4f4b338ba25aa1249b87191594bea1b70000d57be08df6d|

Static7zip extraction:1453 files from Windows resources/app;1788 from macOS
Contents/Resources/app. Version/company JSON confirms Keydous3.2.19 /3.2.31.
Both have exact3793 support; their target bases d00cfdc9 /13b2e050 still use
E5/FE table reads, Boolean1B reports and disable calibration-view polling over RF.
Both root updaters still reference the same get_fw_version service and six named
ZIP members; those names are expected download members, not embedded images.
No firmware image/header established from these extracted app payloads. No
AT32F405 header string found in the scanned extracted application files. This is
not an exhaustive proof that no encoded blob exists anywhere in an installer.
The private older-host-review.json pins exact source paths/hashes and findings.

Wayback CDX found16 official download-page snapshots from2025-01 through2026-08.
Actually acquired2026-02-04 and2026-08-23 HTML: these point to old official OneDrive
Windows/Mac packages. Four historical links were resolved; one returns an error
page and three return OneDrive HTML. The two August share-metadata requests
return401 unauthenticated; no package acquired from those shares. No attempt to
bypass authentication or use private credentials. A distributor's public Google
Drive folder link returns404; its other two Drive links are unrelated VIA JSONs.
Current manufacturer community/legacy download URLs returned429; not repeatedly
hammered. A broad OSS Wayback query timed out; MIME-filtered query returned empty.
Common Crawl2026-43 returned no OSS captures. These are bounded search outcomes,
not an absence claim. The local research firmware corpus search found no existing
NJ98/V4S-labelled image. Public GitHub repositories and release inventories were
also checked; no exact3793 update or dump found in these inspected inventories.

## New receiver/radio dump source

[monsgeek-akko-linux](https://github.com/echtzeit-solutions/monsgeek-akko-linux),
pinned tree79c45f11dcc0fdd24d6075a0f30f5dabbc748c9b, supplies previously unacquired
receiver and radio bytes:

| Public source path | Bytes | SHA256 |
| --- | --- | --- |
| firmwares/DONGLE_RY6108_RF_KB_V903/dfu_dumps/dongle_working_256k.bin |262144|ad3ab42828cf31ed9a7a1e7646462621c556d27ece5dfce590a11152e4ba2f55|
| firmwares/pan108_flash_dump.bin |524288|0d4503130ba6f7bed3ebd5491e61ea4c30f733e6f2112f0945f113f4302d7d7d|

**These are NOT established exact Keydous target images.** Receiver header is
AT32F405 8K-DGKB; product string at file7EEB is MonsGeek 2.4G Wireless Keyboard.
Tester inventory says2.4G Wireless Keyboard. The directory saysV903, but actual
F0 reply branch returns LE0801. Do not silently equate directory name, reported
version and target identity. The unmodified hook prologues and original speed-gate
bytes match this repository's described stock patch input; this is not proof of
factory provenance for every byte. The PAN image contains PAN108 MEKBRF-9K /
ROYUAN KEYBOARD strings; its exact board/role/revision relative to V4S is unproved.
Vendor/provenance/redistribution limitations remain: all downloaded bytes local;
no vendor firmware or third-party implementation added to published HallJoy.

## Own receiver branch execution

receiver-family/receiver-cache-audit.py uses existing strict firmware_replay.py,
exact receiver hash above, ROM base08000000 and synthetic SRAM/registers.
**PASS six scenarios**. Selected boundaries only, not full device execution:

- Incoming decoded reply08005E94..08005EEA: a65-byte SPI payload in normal mode
  sets state+D9 read-ready and copies64 reply bytes to state+8A (200003BA).
- F7 at08006646..08006796 builds status in the separate USB response buffer
  state+1 (20000331), exposing read-ready. It does not generate fresh travel.
- FC at08006698..08006796 copies64 cached bytes to USB response and clears
  read-ready. Cache bytes remain. A second FC without a new RF reply returns
  the old bytes while F7 correctly reports read-ready0.
- Generic forwarding080066F2..08006796 copies complete E5/FE requests unchanged
  into the SPI TX buffer; pages0 and3 checked. This fragment does not interpret
  the128-key table or trigger a scan itself.
- F0 at0800661A..08006796 returns0801, not a guessedV903 from the directory.

Incoming SPI validation/framing/checksum, radio silicon/peer, scan/ADC, boot,
interrupts, USB controller and timing are outside execution scope. Wrong incoming
length is ignored at the reviewed reply branch. No actual RF packet-loss rate or
V4S full-matrix freshness guarantee is established by this PASS.

## Newly reproduced HallJoy response ambiguity

The acquired image stages a completed SET_REPORT for a main-loop worker:
08007084..08007092 sets pending and copies command bytes; the worker637C is called
from main loop0800807E and consumes pending at08006470..08006484. GET_REPORT
08007238..0800724E directly exposes the separate current USB response buffer.
Thus command staging and response replacement are distinct operations in this
image. Exact USB/host scheduling has not been measured.

Own portable C++ counterexamples compile against unchanged production headers,
not a rewritten parser:

1. host-response-shape.cpp: a synthetic valid F7 status with battery1 is ALSO
   accepted by production ParsePage as travel depths257,0,257,1,... (units100).
2. host-response-session.cpp: complete production Client.Page sequence
   F7,F6,E5,F7,FC returns success and publishes those prior F7 bytes under an
   explicit delayed-FC-worker model. FC has been staged but not yet copied its
   actual RF reply into the USB response buffer. **REPRODUCED** in both checks.

This is a concrete missing distinction between a command's completion and a
merely plausible response shape. It is NOT evidence that this exact interleaving
occurred in log51 or explains the saved raw16/raw131 residuals. The counterexample
uses a low battery value; the observed physical battery/scheduling are unmeasured.
Previously retained stream zeros still do not appear dropped after parsing.
Do not turn the counterexample into a certainty about the tester's firmware.

A fix must establish response ownership/freshness without rejecting a legitimate
depth array just because it resembles F7, and without a blind delay or invented
release. A local read-only marker/ack could be investigated against exact receiver
semantics; it is not approved here merely from the shared PID. Full control
lifecycle/target selection, delayed staging and concurrent stream/page ordering
need realistic regression coverage before an ordinary EXE change. Existing input
reader remains independent; no artificial cap or zero replacement introduced.

## Exact acquisition still needed

These new sources give a real receiver-family cache implementation and a host
counterexample; they do not fill the exact3793 /Keydous-receiver binary gap.
DfuSe source in the public project can upload flash from a compatible device
ALREADY in its supported boot mode. That does not establish a safe stock V4S entry
sequence, no-erase guarantee or a remote read-only dump from ordinary HallJoy.
No tester was asked to enter DFU, change drivers, reset or flash anything.
Further pinned monsgeek-unbrick/src/main.rs inspection explicitly requires BOOT0
bridged to3.3V for its ROM/DFU path. This is not an ordinary stock HID dump method.
The project's uploader accepts voluntary dumps at its server, but the inspected
public business landing page/frontend expose no firmware-download catalog. No
uploads, private collection queries or external messages were performed.

An unsent, reviewable manufacturer request is stored at
alternate-01/manufacturer-request.txt, addressed to the official
[official support contact](https://www.keydous.com/contactus). It requests keyboard,
radio and receiver images with identity/version metadata and fresh RF snapshot
semantics. The owner explicitly declined sending this request. Do not send it or repeat
this permission request for the investigation. The draft is local-only; it is not
an authorized queued message. No email or external message was sent.
Current ordinary EXE605d4556
and Experimental RF conclusion retained. Checks: two new official package hashes,
six sibling receiver branch scenarios, two production-header counterexamples,
private complete-manifest.json. No full build/native-suite rerun for research-only
files; no new support claim requiring a Sheet change.
