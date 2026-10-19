# AULA 6x21: keyboard found but held by another program (2026-10-19)

## Reports

Two HallJoy 1.6.9.1 users sent a screenshot of the red "No supported analogue
keyboard detected" banner and a support log:

| Log | Keyboard | USB | Backend row |
|---|---|---|---|
| `HallJoy (5).log` | AULA WIN 60 HE PRO | 1CA2:1902 | `aula-sparkplayjoy-6x21` present=1 connected=0 failures=0 |
| `HallJoy (6).log` | AULA WIN 68 HE MAX | 1CA2:1901 | same |

Both are exact RM 6x21 identities ("Implemented; awaiting hardware testing").
`failures=0` for the whole session means the read-only protocol proof never
ran: the backend stopped before it (exclusive open, HID fingerprint or device
selection). The 1.6.9.1 support log could not say which stage, because the
backend wrote only to DebugLog/StabilityTrace and the snapshot row carries
neither its status text nor its last open error.

The WIN 68 HE MAX user then reported that the AULA web driver was open in the
browser when HallJoy started. WebHID keeps the vendor interface open, and the
6x21 backend opens it exclusively (the protocol has no transaction ID), so the
open fails with `ERROR_SHARING_VIOLATION`. The same case was seen on a WIN 60
HE MAX in August 2026 (`docs/v1.4/AULA_PHYSICAL_DIAGNOSTIC_2026-08-02.md`).

## Defect

HallJoy itself had found a supported keyboard and knew why it could not open
it, but the UI said that no supported keyboard was present. That points the
user at the wrong conclusion.

No fix existed in unreleased code: the workspace differs from the 1.6.9.1
publication clone (`.local/release-1.6.9.1`) only in MADE68 Air, RongYuan
stream and the 64-bit support-flag snapshot; the AULA 6x21 files were unchanged
since 2026-10-01.

## Change (local build, not released)

- New notice `KeyboardBusy` (support flag bit 31): a native backend reports
  `accessBusy` when its keyboard is present, not connected, and the exclusive
  open failed with a sharing or lock violation. The banner reads "Keyboard
  found, but another app is using it" and tells the user to close the web
  driver tab or configuration app; HallJoy connects automatically (the worker
  retries once per second, late route claims are allowed). Amber style, Open
  log button. It is advisory: not an auto-saved log incident by itself.
- The AULA 6x21 backend sets `accessBusy`; other AULA backends (W669, HERO84,
  MINI60) already open shared and are not affected.
- Exclusive ownership is unchanged. A shared fallback was rejected: replies
  carry no transaction ID, and HallJoy's replies would also reach the web
  driver, which could misread them while editing settings.
- HallJoy.log (ordinary build) now has `evidence.aula6x21` lines, written only
  when their content changes:
  - `discovery`: exact-identity paths, family interfaces with VID:PID, HID
    usage, report lengths and fingerprint verdict, metadata rejections,
    interfaces claimed by another backend;
  - `selection`: none/ambiguous/invalid/retained-missing, or `open_failed` with
    stage, Win32 error and reason (`busy_other_program`, ...), or `opened`;
  - `proof`: phase, failure stage/command/selector/index, mismatch mask, board
    id vs expected, firmware app/build label, precision/max travel, key counts,
    last error label and the first 8 bytes of the last request/reply (the sync
    serial starts at byte 13 and is never written);
  - `claim=blocked` reasons, `session=connected` and `session=ended`.
- Snapshot rows: `native ... access_busy=0/1`; `snapshot.begin ... ui_notices=
  <flags> ui_communication_warning=0/1` (what the banner showed).

Tests: `keyboard_support_status_test` (BusyNotice, bit 31 survives the
snapshot, advisory-only), `support_log_windows_test` (access_busy and
ui_notices fields), `aula_win60he_backend_static_audit.py` (evidence stages and
the busy path end to end).

## Extension to other exclusive-open backends (2026-10-19, owner)

The owner confirmed the same problem for the other backends that open their
vendor interface exclusively. `NativeAnalog_IsAccessBusyError` (32/33) in
`native_analog_backend.h`; MG75 Pro / JingTai V1, Chilkey Slice75, Neo65,
SteelSeries Apex and RongYuan snapshot now set `accessBusy`, clear it on stop,
and log their open error once per distinct value (`slice75.open_failed`,
`neo65.open_failed`, `apex.open_failed`, `rongyuan_snapshot.open_failed`,
MG75 keeps `mg75.admission_failed`). Already shared: W669, HERO84, MINI60,
MADE68 Air, Attack Shark (fallback), RongYuan stream (fallback).

## Release hygiene for 1.6.10

MADE68 Air key lines (`press`, `key_miss`, `release_stuck`, `key_confirmed`)
are written only for keys bound to the gamepad, and live stream frames are no
longer dumped as `rx` lines (statistics only), so ordinary typing never enters
the support log. RongYuan press lines were already bound-key only.

## Build

Native checks (`run_native_backend_checks.py --require-compiler`) and
`build_release.ps1`: PASS (before the extension above; the release run covers it). `support_log_static_audit.py` pinned the old
advisory-only line and was updated with it. Local EXE
`build/bin/Release/x64/HallJoy.exe`, SHA-256
`dd56f39302a180b3cba646f317257e2a4a7499445149853f881a6d495f2d4c66`, not released.

## Expected tester result

Either the keyboard connects once the web driver is closed (the banner clears
by itself), or the log names the failing stage with its data.
