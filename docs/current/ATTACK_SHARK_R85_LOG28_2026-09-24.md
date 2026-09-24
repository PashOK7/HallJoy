# ATTACK SHARK R85 HE log28 — 2026-09-24

Owner reports no analog. Input HallJoy (28).log,4737bytes, SHA256
d692b53a0c4b658b4459d1d5c3d733e42c12605d6c464cb42e60aad3d638e42d.
This is an ordinary1.6.1.0 bounded support report, not a raw transport trace.
Session20537468..20546031 (~8.56s); log utc05:14:24Z. Inventory includes
3151:5029 on interfaces00/01/02, consistent with the catalog's R85 HE, but
VID/PID alone does not prove exact board identity3123. Engine reaches running,
search_complete1 and analogue_connected0. UAP ready1/devices0, no transport
error or restart recorded. Virtual Xbox interface045E:028E is present; that
is not evidence of receiving keyboard depth. Shutdown normal.

No shark identity, eligible-interface count, open error or failed handshake is
in this file. It cannot distinguish exclusive web-driver ownership, collection
shape mismatch, an unrecognized revision, worker startup failure, or failed
feature transactions. Do not assert firmware/user fault or successful analog.
R85 HE exact3123/PID5029 is already admitted by normal native profile table,
with yellow experimental notice. No support conclusion is changed yet; no
README/Sheet promotion/demotion justified by this report alone. K4 custom
support status also unchanged; owner confirms regression fix worked.

Found observability gap: child worker emits these failure records, but ordinary
ReceiveLine discarded them after sending only to compiled-out detailed tracing.
Added structural SupportLog events for worker start/exit, eligible count/vendor
collections, metadata errors, exclusive-open/feature errors, rejected identity,
repeated identity/command numbers and connection state. No key values, serials,
paths, arbitrary child text, forced logging or protocol behavior change. Existing
bounded-history/optional-file policy applies; no per-sample logging added.

Next tester run: latest candidate, enable ordinary logging, close/reopen HallJoy,
reproduce for~10seconds and return HallJoy.log. Closing manufacturer web/desktop
configurator first removes a possible exclusive-access conflict but is not a
claim that such conflict caused this case. Same evidence will then show whether
the native worker started, found an eligible interface and proved board identity.
No need to flash/reset the keyboard or collect user keypress text.


## Ordinary-file verification and delivery

Owner explicitly rejected a report that omits failure reasons. Added collection
PID/bcd, usage page/usage and feature length/receiver classification, in addition
to the failure-stage records above. New ordinary Shark self-test runs the REAL
ReceiveLine parser and SupportLog writer into an isolated temporary directory,
injecting no-eligible-interface, exclusive-open32, GetFeature5, mismatched board
IDs and descriptor shape. Reads the resulting HallJoy.log and asserts these
reasons survived; arbitrary key/path sentinel payloads must be absent. It does
not enable detailed tracing or open the target keyboard. Test file removed.

Normal release build and all6 linked gates PASS (including this actual-file
assertion and required K4 onboard catalog). Existing known ViGEm PDB warning.
Build log .local/r85-probe-log-verified-build.log. EXE:
build/bin/Release/x64/HallJoy.exe SHA256 bbb1a64a4f81e1b4709122f3347d9f82a5390a66e24f78f7e3585e459f1dacc1.
R85 input failure itself remains undiagnosed until a new log from this image;
do not claim a protocol fix or physical success. No forced logging, UI run,
firmware/settings changes or GitHub publication.


## Targeted R85 diagnostic, superseding the ordinary-log-only next step

Owner requested a separate detailed diagnostic build, not another generic log.
Changes are diagnostic-only behind HallJoyAttackSharkR85Diagnostic (also enables
existing detailed/single-log flags). Ordinary general support logging above is
retained. Backup before changes: .local/backups/before-r85-diagnostic-20260924-083003.zip,
including the prior ordinary EXE. No support status, README, Sheet or GitHub change.

Exact reviewed vendor entry: id3123, VID3151/PID5029, Common79_SG9047,
Ry5088_sg9047_1m_8k, 128 matrix slots/79 physical keys, Fn65, WASD14/9/15/21.
Pinned manufacturer model f46676c2.js SHA256
f7fda810605e8924dac0a9391ba840bdc561c2642de5d3a4b57b95b50285c463.
It inherits f9b6af43.js, SHA256
1811416dc2213bfc0041bff89cc8e254e542f322762146223d1872dcee8c3870.
Exact matrix checked against recorded family profile. Vendor E5 FE page requests,
8F identity, 80 version, LE16 depth decoding and timing match HallJoy: offline
review_attackshark_wire_protocol.py PASS. Evidence:
.local/research/attackshark-r85-20260924/review.json.

Acquisition boundary: POST dev_id3123 to official api2.rongyuan.tech:3816/api/v2/get_fw_version
again returns Record not found; qmk/api3 aliases did not supply metadata. Official
product https://attackshark.com/products/attack-shark-r85he-wired-magnetic-switch-rapid-trigger-keyboard
links https://download.attackshark.pro/ATTACKSHARK/R85HE/ATTACKSHARKR85HE.zip.
Also tried manufacturer's download.attackshark.com mirror, headers and a fresh URL.
Advertised archive101813504bytes; transfers stall after15993bytes. A successful
4KiB remote ZIP tail shows one ATTACK SHARK R85HE SOFT.exe installer entry, NOT a
standalone firmware image. Partial files are unusable and must not be treated as
firmware or a successfully acquired driver. Existing pinned multi-model client
contains the exact R85 class reviewed above; it is not the newly downloaded package.

Re-ran real MCU component emulation review_attackshark_bank_init_20260920.py:
PASS for pinned X68MAX2755v504 and X82Pro2935v503 scatter loading, bank selection,
publication and getters. Evidence .local/r85-family-component-emulation.json.
These are related-model component checks, NOT R85 firmware emulation or a proof
of R85's failure cause. No R85 image exists locally, so exact firmware execution
and hardware root cause remain unresolved. Do not substitute a similar image.

Diagnostics now capture:
- R85-only USB command access (PID5029, repeated exact identity3123); other Shark
  PIDs are inventoried but excluded from this diagnostic probe.
- Descriptor/caps, collection ordinal, feature ReportID/bit size/count and the
  exact exclusive-open result. Paths/serial numbers remain excluded.
- Full65-byte TX/RX identity/version and bounded depth samples, command/page/
  timing, malformed replies, Windows errors, active I/O stage on worker exit.
  First8 exchanges per page, timing transitions and then1 pair/second/page;
  rejected depth frames are retained. No unbounded per-sample file writes.
- R85 digital press/release counts. Old diagnostic only observed502f/5030;
  now5029 is observed, with no ordered typing log. Coverage derives from the
  actual R85 map. Old X65 UI hints and stale six-profile startup label removed.
- Page/key min/max/change statistics, mapped/unmapped positive slots, and explicit
  no-valid-pages / all-zero-with-digital / all-zero-without-digital / nonzero-static /
  varying-depth evidence. These classify observations, not automatically causes.
- USB/RF firmware versions, selected units/mm, and declared3.3mm versus current
  provisional3.5mm normalization. This range difference could change scaling;
  it does not explain failure to connect an analog source. No speculative range fix.

No stream-enable, calibration, reset, settings or firmware writes were added.
The application still provides analog/gamepad for an admitted working response;
K4 onboard and existing backends remain compiled. This is a diagnostic EXE with
logging automatically enabled, not the next ordinary release. HallJoy.log next
to the EXE is the requested artifact; writable folder required.

Builder: tools/build_attackshark_r85_diagnostic.ps1. Six existing linked gates,
including required K4 catalog, plus an R85 actual-file log gate. The last gate
checks raw identity/depth report retention, exact model/target admission, wrong
report/echo rejection, exclusive-open32/GetFeature5, mismatch and rejected-page
records in the real file. Synthetic checks do not open a physical keyboard.

Tester action: close the old HallJoy and vendor configurator, run this EXE from
its writable folder, reproduce with several W/A/S/D presses and releases for
20-30seconds, close normally and return HallJoy.log. No reset/flash needed.
If the failure remains, classify its recorded stage before changing protocol.

Final targeted build and all7 linked/log gates PASS. Delivered EXE SHA256
b8449ac709a70e5f51b2b7ee4950ad992da7d67e903070a46fef934b519f6a43. Build evidence .local/r85-diagnostic-delivery-build.log; candidate and
delivery hashes match. Only known ViGEm missing-PDB warning. No agent GUI or
physical R85 test; failure itself remains unresolved pending this diagnostic log.


## Log29: failure is before the first protocol request; revision2 delivered

Input HallJoy (29).log:29554bytes SHA256
66b5f23da1be03389d717f9707d4d3d8fc4eb827fa6d45c9d5e4261bd21ed8e9.
17.58s session,6 failed exclusive opens. Exact expected collection found each
pass: PID5029 bcd0511, usageFFFF:2, Feature65, ReportID0,64 eight-bit values,
Input0/Output0. No metadata errors. Digital60presses/60releases. ZERO wire
records; worker last_command=0. Therefore neither firmware identity nor depth
was read. This is an OPEN-stage failure, not evidence of unsupported analog,
wrong scale, stale replies or a calibration problem. USB bcd0511 alone does
not prove the firmware identity/version command response.

Confirmed HallJoy diagnostic defect: Session initialized device first, then
PreciseTimer, then read GetLastError in constructor body. Timer creation can
clear the saved Windows error, yielding exclusive_open_failed error=0.
Revision1's hand-injected logging test did not exercise the real Session open
failure; that gap is now covered with actual Windows file handles.

Revision2 moves CreateFile into constructor body after member initialization
and captures its error immediately. Logs explicit success/access/share/error
for every attempt. Ordinary path retains exclusive opening with corrected
error capture. Only R85 diagnostic adds shared RW then zero-access shared
Feature-only fallback after access-denied/sharing-violation, matching the
opening convention used by HIDAPI Windows:
https://github.com/libusb/hidapi/blob/master/windows/hid.c (open_device/hid_open_path).
This is a compatibility attempt, NOT proof of which Windows error occurred in
log29 or that R85 now works. Vendor configurator should still be closed to
avoid competing feature transactions. No changes to firmware/settings commands,
identity admission, decoder or calibration. Identity logs report actual access
mode rather than hardcoded exclusive=1. Factory-map field renamed factory_usage
because privacy sanitization redacted hid= in log29; it is static map data.

New real Win32 regression creates a temporary file held in shared RW mode;
Session must preserve ERROR_SHARING_VIOLATION32 and diagnostic shared fallback
must succeed. After file deletion, Session must preserve FILE_NOT_FOUND2 even
with timer creation. Tests use no keyboard or HID I/O. Both regular Shark gate
and actual-file R85 gate run this test; all7 build gates PASS. Ordinary-build
branch is covered by the same test when an ordinary build is next run; no new
ordinary EXE is claimed delivered here.

Backup .local/backups/r85-before-log29-20260924-084331.zip.
Build .local/r85-log29-fix-build.log. Delivered revision2 EXE SHA256
92be3ead1377ce8d5ee32e625b28936e4ec44e9ed5f1a807156ef7b0a851bf46 (candidate/delivery match). Known ViGEm PDB warning only.
Next required hardware evidence: repeat20-30s WASD test with revision2 and return
HallJoy.log. Root Windows refusal code was lost in revision1; do not invent it.
No support-status change, Sheet/README edit, firmware write or GitHub publication.


## Pre-send audit requested by owner: revision3

Reviewed Find -> Session opening -> Identify -> Exchange -> HidD_SetFeature.
Found another pre-send failure path: Delay returned false on timer arm/wait
failure without its own reason. No evidence this happened in log29 (which
failed earlier at open). Changed timer failures to explicitly logged fallback
to ordinary cancellable waiting; absence/failure of high-resolution timing
must not prevent a read-only protocol request. Actual shutdown cancellation
still aborts. TX trace now occurs after the pre-send wait, immediately before
the SetFeature attempt; TX alone does not prove USB delivery.

New same-image Windows test passes an event handle in place of a timer,
forcing actual SetWaitableTimer failure; verifies continued wait completion,
null timer fallback and cancellation in both paths. Actual-file log gate also
requires this test's PASS. All7 gates PASS, no keyboard I/O or GUI test.
Revision3 delivered SHA256 a138069c94edec81b22b3ac26000ae2d868aa32c6acb944df84c077a19716478; candidate matches.
Build .local/r85-send-audit-build.log. Backup
.local/backups/r85-before-send-audit-20260924-084913.zip.

No honest guarantee that Windows will open the remote physical interface:
that still requires tester evidence. No physical R85 success or firmware
root-cause claim. There is no prerequisite of pressing keys, elapsed20seconds,
calibration, or a valid analog sample before sending initial identity request.
Normal close/cancel, failed interface opening and later identity mismatch
remain explicit legitimate stops; do not remove identity safety checks.
Use revision3 instead of the prior EXE for the next tester run. No publishing.


## Log30: real device communication and changing analog confirmed

Input HallJoy (30).log,122288bytes,SHA256
98319ed554f752a91cf5486f5480249f31983ac6f449b1279a72ad668dbbaa66.
Revision3,22.875s session. Exclusive RW opening explicitly fails with Windows
ERROR_SHARING_VIOLATION32. Immediately following shared RW opening succeeds.
Feature-only fallback and timer fallback are not needed in this run. The other
handle/process responsible for sharing conflict is UNKNOWN; do not blame the
user/vendor application or rule out HallJoy's own enumeration without evidence.

Two real8F responses identify3123 with USB firmware0x0511; RF80 yields0.
E5 FE pages then provide changing depths. Final summary4727 valid pages,
page0/1/2/3 counts2364/1145/1181/37,14 varying slots,214 digital chord frames,
109 independent changes,0 transport I/O errors. One stale identity reply was
recognized/rejected (malformed1); it is not a transport failure. No timing
escalation; delay1ms.122 digital presses/releases; clean shutdown, worker not killed.
W slot14 max720, A9 max730, S15 max724, D21 max719; all have intermediate
changes and return to0. Native publication active (4245 updates at20.468s),
playback enabled1, sufficient-data state3. This proves actual device reading
and publication, not merely valid synthetic parser input. It does NOT by itself
prove every key/Fn/layout/game binding or physical latency. Max exchange27.132ms
includes host wait26.411ms; no microsecond/1kHz guarantee should be inferred.

Root cause of this run's initial failure is the exclusive-open requirement,
resolved by shared opening in diagnostic revision3. There is no evidence the
firmware lacks analog or requires calibration/stream enable. Legacy log29's
lost code is consistent with this but cannot be recovered retroactively.

Remaining implementation issue: shared fallback is STILL R85-diagnostic-only;
ordinary build would retain exclusive-only policy. Do not claim production fix,
publish a support promotion or remove yellow banner merely from this log.
Record successful targeted diagnostic hardware evidence here; ordinary
integration, existing range uncertainty (observed max730 vs normalization700),
and corresponding support-status reconciliation belong to the next support
change task. Current README/hardware/Sheet/runtime status remains experimental,
with no promotion/demotion in this log-analysis task. No EXE changes or publication.


## Owner-approved ordinary integration (supersedes diagnostic-only next step)

2026-09-24: R85 HE promoted to Supported at owner request. Shared access is now
enabled in the ordinary family backend; exact notice distinguishes HE from Ultra.
README/hardware/Sheet synchronized; all49 yellow entries reconciled. Ordinary
1.6.2 delivered with AJAZZ retained, no forced logging. See
RELEASE_1.6.2_PREPARATION_2026-09-24.md for complete evidence/hash. No publication.
