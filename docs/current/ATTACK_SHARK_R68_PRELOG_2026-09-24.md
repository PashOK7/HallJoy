# ATTACK SHARK R68 HE — preliminary report, 2026-09-24

Owner forwarded a tester screenshot: HallJoy says analog keyboard not detected.
Tester log has been requested but is not available. App version, USB identity,
firmware version, collection descriptors and transport failures are unknown.
Do not treat the screenshot as proof of unsupported firmware or user error.

Verified in local source AND published1.6.2 commit
395fdbe44351d88e2090f71bb51b7a9166f88a13 (publication mirror git object):
R68HE dev3650 / VID3151 PID502D is present. Generic family admission requires
FFFF:0002 and65-byte Feature reports, two matching8F responses with exact
board/PID match, then nonzero USB version before depth publication. Physical
factory/Fn mapping and automatic visual geometry already exist. Missing visual
layout is not a prerequisite blocking this R68 path. Generator audit PASS:
python tools/generate_attackshark_family.py --check (37 exact profiles).

Published1.6.2 includes exclusive -> shared RW -> feature-only opening fallback
for sharing/access errors, plus the timer fallback from R85 work. It is not
R85-only in that release. Earlier releases may retain the exclusive-open defect.
Official release page was read back during this investigation; local published
commit provides the code evidence. Current official R68 HE product page calls
it wired; do not tell the owner that wireless mode is the likely cause.

Possible, not established: older HallJoy; different physical R68 revision than
our single known tuple; missing/filtered collection; OS handle-access failure;
vendor client/browser competing for Feature replies; failed/mismatched8F;
zero USB-version field; failures during depth exchanges; worker startup failure.
No probability ranking is justified yet. No new reproducible defect found in
this preliminary source audit. Known R85 fix does not rule out other failures.
Exact R68 firmware was unavailable in the earlier API corpus (Record not found),
so other-model component emulation must not be represented as an R68 test.

On receipt of log: establish version/build flags, inspect shark.worker_start,
vendor collections, descriptor/usage/feature rejection, eligible count,
exclusive_open_failed and actual subsequent I/O, probe identities and commands,
identity_rejected/unsupported_revision, then depth counters/worker exit.
An exclusive-open error alone is not failure if the shared fallback succeeded.
If no eligible collection and no target VID is recorded, use generic HID
inventory rather than loosening the model allowlist blindly.

Optional low-cost tester check while awaiting log: current1.6.2, close vendor
app and its browser configurator, restart HallJoy. This is a conflict test,
not a conclusion that the tester's settings caused the problem.

No code/build/status/Sheet changes; no firmware writes, device I/O or publication.
Sources: https://github.com/PashOK7/HallJoy/releases/tag/v1.6.2
https://attackshark.com/products/attack-shark-r68he-carbon-fiber-wired-rapid-trigger-magnetic-switch-keyboard


## Fresh official-source recheck (same day)

Downloaded official driver-download HTML and current qmk.top entry/client into
`.local/research/r68-fresh-20260924/`. The official R68 HE card links to qmk.top
and `https://download.attackshark.pro/ATTACKSHARK/R68HE/ATTACKSHARKR68HE.zip`.
Current web entry uses `js/index.CUcPl2Vx.js` (3,089,203 bytes). Exact model
markers R68HE, ry5088_r68he and id:3650 are absent from this main bundle.
This does NOT prove the whole service cannot load/support R68; it establishes
that this fresh main bundle supplies no replacement exact-model evidence.
No remote model-catalog endpoint was established. Do not describe local
IndexedDB getConfigs or desktop CONFIG.json as a downloaded device catalog.

Fresh read-only POST /api/v2/get_fw_version with dev_id3650:
api2.rongyuan.tech:3816 returned HTTP500 `Record not found`;
api2.qmk.top:3816 and api3.rongyuan.tech:3816 returned HTTP404.
No exact R68 image obtained; firmware behavior/emulation remains unverified.

Desktop download HEAD returned403, but browser-header GET/range requests did
succeed. ZIP is106,870,070 bytes; final1KB contains a valid central directory
with one member `Attack Shark_R68HE SOFT.exe`. Larger body downloads stalled
and timed out (urllib and curl). Partial ZIPs are NOT usable archives or
reviewed driver versions. No installer executed. A403 alone must not be
reported as the final reason this archive was not analyzed.

Rechecked saved exact vendor record in index.2e5bd916.js: R68HE dev3650,
VID12625/PID20525, ry5088_r68he_8k_dm, Common66_X68. Transport record uses
FFFF:0002/interface2. Model427b7c3f.js extends f9b6af43.js and overrides only
factory/Fn matrices, not communication/initialization. Vendor USB-version
bytes7/8 (without report ID) match native bytes8/9 (with report ID).
Vendor live depth reader calls E5/FE, paged, and decodes little-endian uint16;
our backend uses the same command family. No mismatching R68-specific
transport override was found in these saved sources.

Important unresolved distinction: vendor setJiaoZhunKaiGuan couples its
calibration UI to minimum/maximum calibration commands before the depth loop.
HallJoy deliberately does not send those state-changing commands. The reader
itself does not issue them, but this source alone cannot prove every R68
firmware serves live depths outside calibration. Do not claim exact R68
firmware independence based only on another board's image. This is a future
check ONLY if the tester log shows discovery/identity succeeded; it does not
explain an earlier HID enumeration/open/identity rejection. Do not recommend
calibrating or changing tester settings speculatively.

Conclusion: additional exact sources checked; no demonstrated native defect,
new revision or justified code fix yet. Preserve support status. Incoming
log must locate the failing stage before choosing the next probe.

Fresh artifacts SHA256:
- downloads.html: 0d8f96f40f0441cdbdb25c27c1e0668d5cddd77fe39cc8909e040d35e1bc6221
- qmk.html: 0d03061afcc67bbb0e7262fd0a5621909cb8b85d22a4fe0b69a8dd0d1dd3101f
- index.CUcPl2Vx.js: d2c75924fb08618afa11934f0b7f32c1b61452ad4af88d896126bb1063e33494
- zip-last1k.bin: a2758a1bd01a6dfa07ca90aa1337122c0eefaf0b69a2ee885a7dcb1bdc65008a


## Log31: confirmed admission defect and local fix

Log SHA256:4e660399a0b75608da7812bf2c36a0a617bd1a1414fbb1125361676cc03488df.
HallJoy1.6.2.0, UTC2026-09-24T16:16:07Z. The device actually enumerates as
3151:5029, not the catalog's3151:502D. FFFF:0002, feature65 is eligible;
exclusive open error32 is followed by successful Feature exchanges. There
are six paired identity3650/3650 responses with command143/143, yet all five
scan sessions terminate identity_rejected. Known(3650,0x5029) was false.
This establishes a HallJoy admission defect; no calibration hypothesis is
needed to explain this log. Depth reads never start.

Some other replies have command13; origin is unknown (do not assert another
app caused it). The log does not contain a validated depth run or establish
an additional analog failure. Firmware scaling/version and depth operation
remain for the tester's next run.

Local fix in attackshark_pro_diagnostic_model.h admits ONLY the additional
board3650/PID5029 pair, preserving3650/PID502D and all original identities.
No global relaxation, calibration commands, firmware writes, forced logging
or new diagnostic mode. Existing linked self-test covers both accepted pairs
and rejected other/unknown pairs. Original generated37 profiles/maps unchanged.

Validation: generate_attackshark_family.py --check PASS; ordinary build_release.ps1
PASS including all six linked executable gates. Normal EXE:
build/bin/Release/x64/HallJoy.exe
SHA256:eb4d6cc93c8f79863c03a1c10e8b7fed271a5f6753fe823ddb2856e889892265.
No GitHub publication. Send this EXE for a new log/analog check; do not claim
hardware-tested support before receiving it.

Status reconciliation2026-09-24: README experimental listing and runtime notice
unchanged; SUPPORTED_HARDWARE and next notes document the local fix. Live Sheet
metadata Main/sheetId0/1275rows confirmed; located by brand/model then reread
Main!A90:C105. R68 HE row97 retains Implemented; awaiting hardware testing,
strict ONE_OF_LIST dropdown, both base/effective B:C yellow RGB1/.9019608/.6392157.
No Sheet write needed. No status promotion or model-count change.


## Log32: admission fixed, periodic identity recheck fails

UTC2026-09-24T16:28:33Z; SHA256:5d4f6c98bc6097fd827cfedffcbcc5dbd6868baa72cf28b9fe2302b6dc8cb6ba.
The alternate pair now passes: identity3650/state2 and connected=1. Accepted
sample counts reach page0=2397/page1=1166/page2=1205 in the first session;
native_updates reaches6135 across sessions. These counters establish accepted
sample publication, NOT nonzero/varying depth or successful gameplay.
No Set/Get I/O errors recorded. Two workers exit8, which source maps exactly
to the periodic identity recheck (every1024 cycles). First session capture
25453ms, second9688ms; this is cycle-based, not a20/30second time limit.
After the first exit, app reports disconnected, restarts and reconnects.
Initial probes again include command13; periodic failed replies themselves
are NOT logged, so their bytes and cause cannot be established from this log.
A competing configurator/client is plausible, NOT proven. Exclusive error32
alone is insufficient; shared access actually works.

Code weakness to address after isolating contention: periodic check sends two
8F requests once and validates only the final reply; startup uses up to three
attempts. Do not simply disable identity checking or claim unplugging; there
is no reported transport error. Need bounded retry plus explicit recheck reply
diagnostics if failure reproduces with vendor apps/browser HID sessions closed.
Also telemetry reports profile PID502D although actual collection PID5029;
this is metadata reporting, not renewed admission failure.
Expired-pages51 is aggregate across banks; cannot attribute input stalls to it
without per-bank evidence (sparse page3 polling can expire naturally).

No further code/build/status changes from this log alone; experimental status
retained. Next tester information: whether vendor app/web configurator was open,
and whether disconnects reproduce after fully closing those clients. Avoid
calling current behavior stable or promoting to Supported from packet counters.
