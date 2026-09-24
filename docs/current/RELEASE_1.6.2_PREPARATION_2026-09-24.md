# HallJoy 1.6.2 local release candidate — 2026-09-24

Owner authorized Supported for ATTACK SHARK R85 HE after log30 and requested
ordinary Release, AJAZZ inclusion, Sheet/README/hardware sync and patch notes.
No GitHub publication requested or performed.

## Delivered behavior

- Shared RW / feature-only fallback after access/sharing errors now belongs to
  the common ATTACK SHARK native Session in ordinary builds, not an R85-only
  diagnostic branch. Exact identity and sample validation remain mandatory.
- Immediate Windows error capture and cancellable timer fallback retained.
- R85 HE supported without yellow notice. Distinguish the confirmed PID5029
  from R85 Ultra PID5030 even though both share one layout token. Generated
  notice predicate now accepts productId; unknown PID remains conservative.
  Shared geometry/automatic layout is preserved. No speculative range change.
- AJAZZ AK820 MAX RGB ordinary raw analog support, calibrated per-key ranges
  and existing Fn behavior retained. This does not promote base MAX HE/Ultra.
- Custom K4 onboard flag retained with mandatory linked gate. Existing three
  experimental MonsGeek/Chilkey/EPOMAKER implementations retained.
- Version1.6.2.0; ordinary optional logging/crash-history policy, no forced
  diagnostic recording. Builder explicitly disables R85/PRO/AJAZZ diagnostics.
  Actual production CL flags checked: HALLJOY_PRODUCTION/native/K4 present,
  no diagnostic, simulator, stability-trace or device-support-log flags.

## Synchronization

README normal table includes R85 HE (USB) and AJAZZ AK820 MAX RGB; experimental
R85 HE entry removed while R85 Ultra retained. Hardware inventory/evidence and
runtime notices reconciled. Patch notes: docs/releases/RELEASE_NOTES_v1.6.2.md;
NEXT points there. No Known limits section or internal identity clutter added.

Live Google Sheet metadata read: Main sheetId0,1056x26. Read A1:C650 before
and after. Only C83 changed Implemented; awaiting hardware testing -> Supported.
Readback: B83:C83 green RGB .65882355/.8666667/.70980394, validation unchanged.
C14 AJAZZ AK820 MAX RGB remains Supported/green; C13 MAX HE remains Research
incomplete, C15 Ultra HE remains Not investigated. C84 R85 Ultra remains yellow.
A82:C85 and A13:C15 neighbors/validation checked. No comments/notes, no style
or dropdown changes. All49 yellow entries match the generated runtime catalog:
python tools/support_notice_catalog.py --sheet .local/release162-sheet-readback.json
PASS. Snapshot contains all live yellow entries plus affected neighboring rows,
selected from the full bounded read. API effective colors verified; no GUI run.

## Validation and artifact

- Ordinary release build and all6 linked gates PASS: required K4 catalog,
  support self-test, Shark, MINI60, NA87/AJAZZ and embedded ViGEm installer.
- Shark gate covers real Windows sharing32/file-not-found2 error retention,
  common shared fallback and timer-failure/cancellation; ordinary log writer
  verifies structural failure evidence without detailed trace.
- Portable keyboard_support_status test PASS including R85 HE vs Ultra sharing
  a layout token, unknown PID still yellow, existing model notice regressions.
- No physical test newly performed by agent; R85 evidence is tester log30 and
  AJAZZ evidence is existing tester log27. No claim of hardware coverage for
  every family model or GUI visual verification.
- Existing known missing ViGEmClient.pdb warning only.

Build evidence: .local/release162-final-build.log.
Delivered build/bin/Release/x64/HallJoy.exe; candidate/destination hash equal.
SHA256: 7865a3980287375a355d7f4834859bafe080853c2dcca84168e5a73c20d268a1

Ready for owner review/publication; GitHub code/release remains unchanged.


## Follow-up: cross-brand open-policy audit (read-only)

Owner asked whether the same refusal could occur on other brands. Source review:
AJAZZ raw stream uses shared opening; IROK NA87 and W669 select a proved mode
with shared paths; DrunkDeer, ATK Hex80, MINI60, HERO84 and addressed/MAD68
backends have shared access. Exclusive-only command paths remain for WIN60HE,
IROK MG75 Pro, Chilkey Slice75, RongYuan snapshots (MonsGeek M1 V5 HE and
EPOMAKER G84 HE), and custom Keychron onboard channel. They can encounter a
sharing refusal if a conflicting handle exists. Some explicitly require
exclusive ownership to prevent interleaved untagged responses/session commands;
do not blindly apply shared access to all protocols. No hardware failure on
these brands established by this audit. No code/build/status changes. The
confirmed sharing32 case remains R85 log30; another handle's owner is unknown.


## Updated candidate: automatic layout startup cache

Added last automatic geometry persistence and patch-note bullet; see
AUTOMATIC_LAYOUT_STARTUP_CACHE_2026-09-24.md. Earlier artifact hash above is
superseded by 5e0b2097ef340b49fb415df76056a163cfc19a09d05f9100a180cecde816fcb6. All6 release gates and
production-linked profile/layout/recovery tests PASS. Support statuses unchanged.

## Layout reconnect follow-up

Startup-only cache was insufficient for K4 gamepad USB reenumeration. Preserve
last automatic geometry during Searching/Missing, revalidate live mappings.
Linked layout/reconnect/remap and18 recovery tests PASS. See
AUTOMATIC_LAYOUT_STARTUP_CACHE_2026-09-24.md.

Ordinary1.6.2 rebuild and all6 linked release gates PASS; delivered
build/bin/Release/x64/HallJoy.exe SHA256
ceb47c21bfb7ff12efccd6e15853c8fddc6a2793e8dfc95f39bc121dbe4a0183.
Candidate and delivered hashes match. Build: .local/layout-reconnect-release-build.log.
No forced logging, firmware change or GitHub upload. Owner visual check pending.


## Publication authorized — 2026-09-24

Owner accepted the layout fix and authorized stable release; prior local-only
publication restriction is superseded. Do not run hosted Windows CI. Workflow
now runs Windows only on manual workflow_dispatch; portable CI remains automatic.
Full local build script aligned with incremental release builder: retain K4
onboard and require the same linked protocol gates. Workflow artifact paths
corrected to the current build/bin/Release/x64 location.

Live Sheet Main A1:C650 re-read with validation/effective colors: AJAZZ AK820 MAX
RGB row14 and R85 HE row83 Supported/green; R85 Ultra row84, Slice75 HE row143,
G84 HE row210 and M1 V5 HE row407 remain yellow. All49 yellow entries match
runtime notices; dropdowns intact, no Sheet changes needed. Fresh snapshot:
.local/release162-publication-sheet.json. All support changes since1.6.1 reviewed.
Source publication excludes generated outputs and private correspondence.

## Published

Stable/latest1.6.2 published2026-09-24T08:05:53Z. Local checks and portable CI
PASS, Windows CI SKIPPED. See RELEASE_1.6.2_PUBLICATION_2026-09-24.md.
