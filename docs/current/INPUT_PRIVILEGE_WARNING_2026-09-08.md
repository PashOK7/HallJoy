# Input privilege warning

User-approved English advisory appears directly below Configuration / Block
Bound Keys. No popup, automatic elevation, focus stealing, or permanent hint.
The same page font is used with wrapping and space for following controls.

The realtime producer retains deep-press timestamps in a fixed atomic mailbox;
short taps survive even if both edges occur between 100 ms UI checks. There is
no extra HID polling, allocation or locking. Foreground HWND/PID are checked
at that cadence; token queries occur only when
the foreground changes. A confirmed higher integrity level or actual UIPI
message-access denial is required. A failed token query alone is not evidence
of elevation. UIAccess exempts the caller.
No per-key training or minimum hold duration is required. Each candidate
requires release below 10%, then depth >=90%, with 250 ms grace for digital
delivery. Standard keyboard usages only; vendor/Fn codes are excluded.
Three missing qualified
presses within ten seconds trigger the advisory. One held key cannot accumulate
evidence. Hook input is checked when Block Bound Keys applies; otherwise Raw
Input is checked. A working hook may itself suppress Raw Input, so that case
must not produce a privilege warning.

Focus changes reset incomplete evidence. Once shown, the advisory is latched
until HallJoy restarts, including across healthy input, pause/resume and device
disconnect/reconnect. It is not saved to settings. Further permission probing
stops after the latch is set. No typed text is persisted. Page cache invalidation
is sent only on the initial warning visibility transition.

Limitations: this is evidence-based inference, not an OS rejection receipt.
Shallow presses, untracked keys,
unknown process tokens, and absent analog data cannot establish this condition.
No promise of detecting every missed input; no attempt to bypass Windows UIPI.

Validation: portable detector C++ tests PASS (baseline, three-event threshold,
recovery, normal integrity, expiry, held/shallow keys, hook self-suppression,
missing hook, pause). Added to the normal native test runner. Static suite and
Release x64 build PASS. No visual checks performed and no real elevated-target
keypress test claimed. Deployed SHA256:
`B0BC915F750A6A57A088490711229342730615F9977AE1810433C1E3F934E0B5`.
Source/executable checkpoint: `.local/backups/input-privilege-warning-20260908/`.

## Follow-up: real typing did not trigger the first implementation

The initial tests reproduced an overly restrictive specification: per-key
training, >=200 ms deep holds, and a latest-value UI snapshot. Ordinary typing
could never satisfy training/hold conditions or be lost between UI checks.
Removed both requirements and connected a retained press mailbox to the real
Backend_Tick raw-depth publication. New tests drive the producer through a
30 ms tap, release it before the first UI sample, then verify cold-start warning
after three missing taps. They retain negative tests for shallow travel, held
keys, ordinary privilege level, duplicate mailbox reads, expiry and successful
blocking-hook suppression. Unit and static tests and Release build passed.
Read-only process-token probe confirmed Code PID 17816 at high integrity (12288)
and HallJoy PID 19480 at medium integrity (8192). This verifies the actual rights
mismatch, not end-to-end physical-key detection; the latter remains user-tested.
Repair checkpoint: `.local/backups/input-privilege-fix-20260908/`.
Updated release SHA256:
`817F883C28F0D9348A5CA441ED0969BDF16D6542C664977D88C54038D80155E5`.
Programmatic startup check passed: real main window, no visible error dialog.

## Follow-up: elevated VS Code denies token access

Live read-only PDB-guided inspection of the exact deployed build showed healthy
backend/SDK/device state and retained analog presses, but `known=0` whenever
Code PID 17816 was foreground. A helper launched through Explorer (verified
medium integrity 8192) reproduced: OpenProcess limited-query succeeds;
OpenProcessToken TOKEN_QUERY fails with ERROR_ACCESS_DENIED (5). READ_CONTROL
also fails. Earlier elevated diagnostics could read the high token, so they
did not test the actual caller's permission boundary.

Added input_privilege_windows.h fallback: only when foreign-token reading fails
and own identity is known/non-UIAccess, post a GUID-named registered message with
zero payload once per foreground transition. This has no keyboard/window command,
does not wait, change focus, modify message filters, or request elevation.
Only PostMessage failure ERROR_ACCESS_DENIED confirms the UIPI barrier; other
failures remain unknown. WM_NULL is unsuitable: it succeeds across this boundary.
Reference: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew

The exact production fallback was exercised in the medium-integrity helper:
Code -> blocked=1; helper's own hidden test window -> blocked=0. Portable tests
cover successful posting with stale error, timeout, invalid HWND and queue quota;
none count as elevation. Build/static tests passed. Source/EXE checkpoint:
`.local/backups/input-privilege-access-20260908/`.
Release SHA256: `9A238D1156EE3F298FB1EF0AFE92FF57709092BFFCF6FD0DB7C76A1002A20A81`.

End-to-end physical-key test CONFIRMED by user: the banner appeared. Read-only
inspection of running HallJoy PID 18572 while Code PID 17816 was foreground:
known=1, higher/barrier=1, backend/SDK ready, one device, 20 then 24 analog keys
observed and consumed, raw/hook counts=0, missing evidence accumulated,
warning=1. This verifies the actual producer -> permission probe -> detector ->
Configuration display path, not only an isolated policy test.

## Retention change requested by user

Removed recovery-based and pause/disconnect dismissal. ResetSession now clears
only provisional evidence and has no option to clear the advisory. Tests verify
healthy input and session resets preserve the warning, while a fresh detector
(new process) starts without it. Earlier recovery observations above describe
the superseded behavior.
Detector tests, static suite and Release build passed. Updated release SHA256:
`0508F8E06422B494AEE86E310A8492ABB13D081B5E824447E064E46B40B47F68`.
Previous EXE: `.local/backups/HallJoy-before-privilege-latch-20260908.exe`.
