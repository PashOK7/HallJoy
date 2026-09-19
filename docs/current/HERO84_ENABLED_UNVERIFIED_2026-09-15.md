# HERO84 HE enabled with unverified-support notice — 2026-09-15

## Owner authorization

The owner explicitly requested HERO84 HE to work like NA87: enable analog in the
ordinary HallJoy executable, keep the support frozen pending further evidence,
and display the amber testing notice. This supersedes the disabled-build decision
in RM-29/RM-30 and the preceding September 15 frozen-notice registry for HERO84 only.
NA87 Pro, ND75, ROG Azoth 96 HE and Attack Shark X68 HE remain disabled.

## Runtime changes

The retained HERO84 native backend is compiled into the normal project and entered
before generic Addressed discovery. Exact 372E:103E, FF60:0061, report-09/64-byte
interface and the HERO84 UUID 11:00:00:00:00:05 remain mandatory before claiming
its endpoint. Existing identity, assignment and direct-sample read commands remain.
The diagnostic-only catalog still selects its separate diagnostic descriptor.

Reviewed and corrected two pre-existing publication defects before enabling:
- Owned mapped keys retain analog ownership when samples expire; reads become
  neutral rather than falling back to a full digital press.
- Duplicate live assignments now use per-physical-key samples and observed ranges,
  aggregating aliases by maximum fresh depth. Releasing one alias cannot clear a
  held alias. Demand is reset between sessions; connectivity closes before reset.

Confirmed native HERO84 presence raises its amber notice even with a generic USB
product name or another supported keyboard attached. HERO84 and NA87 use the
available-but-incompletely-tested text; disabled models retain unavailable text.

## Limits that must not be presented as tested behavior

No HERO84 is connected locally and no new tester log is available. The retained
backend normalizes using its observed sensor range, not a verified millimetre
scale or stored calibration endpoints. At startup it stays neutral until a span
is observed; initial shallow presses can establish an underestimated full range.
This limitation is unchanged and is a reason for the experimental notice.

The backend reads the live layer-0 assignment map. A dedicated HERO84 geometry,
automatic layout token and the automatic/manual assignment split used by newer
NA87/IPI implementations are not added by this enablement. Generic selected
layouts may not represent all physical keys. Fn/compound/macro assignments are
not advertised as supported; ordinary HID assignments through E7 are accepted.
The retained factory physical-ID set is the previously reviewed 83-key table.

Backup: .local/backups/hero84-enable-20260915-173719/.
Validation PASS: complete native/static/portable suite with --require-compiler;
production-linked simulator profile/event suite with backend initialization
forbidden; direct HERO84 publication test covers partial depth, per-position
ranges, duplicate aliases, release, stale neutral with retained ownership, reset.
Simulator and ordinary delivery Release builds PASS. Existing ViGEm PDB LNK4099
and pre-existing portable compiler warnings remain; no new build errors.
Evidence: .local/hero84-enable-checks.txt, .local/hero84-enable-profile/,
.local/hero84-enable-simulator-final.txt, .local/hero84-enable-release.txt.
Delivery: build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe.
SHA256: d18c10dad48b0c741be1487debb052a1d42d4a63e0175d554925fcfacf4ad3ea.
No visual application run or physical HERO84 test was performed.
