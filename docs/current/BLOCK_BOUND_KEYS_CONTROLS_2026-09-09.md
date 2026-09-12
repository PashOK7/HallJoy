# Block Bound Keys controls

## 2026-09-12 — keypad identity and input-pump correction

Supersedes the WM_HOTKEY-driven toggle mechanism described below. Capture and
runtime normalize non-E0 keypad scan codes to VK_NUMPAD0..9 / VK_DECIMAL, independent
of Num Lock; E0 navigation keys stay distinct. Labels explicitly distinguish keypad
digits and navigation keys. Legacy Num-Lock-off bindings only stored a navigation
VK, so cannot safely be migrated (it could mean a real arrow): rebind once.

The low-level keyboard hook now owns a dedicated message-pump thread. UI saving
was synchronous on the old hook thread; moving only the toggle or changing the
350 ms save delay would not prevent release events waiting behind disk work.
The shortcut is matched once per physical down and changes the atomic blocking
setting inside the hook before the next keyboard event. Only UI refresh, dirty
marking and the existing debounced save are posted to the UI. RegisterHotKey is
retained for conflict detection/reservation and capture; WM_HOTKEY never performs
a second toggle. Injected events do not toggle. Repeats and releases belong to the
original shortcut press, including modifier/capture/settings changes while held.

PressRoutes remains hook-thread-owned and preserves down/up routing across changes.
Hook installation remains alive until shutdown to avoid losing held-key ownership.
UI privilege detection receives hook timestamps through an atomic mailbox; the
UI-only Detector is not accessed concurrently. IPC publishing stays on the UI.
Pause passes new presses while completing already-owned releases consistently.

Checkpoint: `.local/backups/block-keys-input-20260912/` (source and previous EXE).
Expanded portable tests cover all eleven Num Lock aliases, separate E0 navigation,
repeat suppression, capture/assignment changes and toggle-then-W press/release.
Windows integration test installs the real hook on the production thread owner,
and confirms its message pump responds while the calling thread does not pump.
These tests send no keyboard input to user applications and are not a physical
keyboard/game verification. Source guards prevent returning the hook to the UI.
Also corrected an obsolete README-heading assertion to accept the owner's existing
heading-free introduction; README itself was not changed.

Validation: complete native/static/portable suite PASS; final direct policy and
Windows hook-thread tests PASS; source integration guards PASS. The initial
production-linked profile run failed the overlay-edit event test; an unchanged
simulator rerun passed all profile/event checks and startup recovery. This is an
unresolved intermittent test failure, not a proven fix to that test. Evidence:
`.local/block-keys-input-build.log`, `.local/block-keys-profile-recheck.log`.
Production build/packaging resumed at the unchanged build script's MSBuild stage
after those gates completed; embedded ViGEm and telemetry checks PASS, no unexpected
production warnings (existing ViGEm PDB LNK4099 only). Local release SHA256:
`E2C10BDF4738565443090E51D79A6EBFA27FD92A847B78B403572CCCAD85C54C`.
At that checkpoint, no GitHub publication or physical gameplay verification had
been performed. The owner subsequently confirmed both fixes work and authorized
release 1.5.2 with a short English patch note and no README changes. The release
rebuild changes only version identity to 1.5.2; no further input logic changes.

## Collapsible child group

The exception and shortcut controls are visible only while Block Bound Keys is
enabled. An inset and theme-colored connecting rail identify them as its child
settings, using the existing font/buttons. Collapsing removes their layout space
and hit targets, rejects queued child commands, and cancels shortcut capture.
Checkbox, global hotkey and profile application refresh layout explicitly.
The registered shortcut and saved preferences remain active while collapsed,
so the shortcut can turn blocking back on. The latched privilege advisory is
independent and is not hidden with the optional settings.
Checkpoint: `.local/backups/block-keys-group-20260909/`.
Source integration guard: `py tools/test_block_keys_group.py`.
It is also included in the native/static runner. Group source guards, static
suite and Release build PASS. No visual inspection performed. Previous EXE
preserved in the checkpoint. Updated release SHA256:
`15C61B8BE964C1CC781370904D21695F7D603FA093165F73CB2320F784CB038D`.

## Contract

Configuration keeps its existing retained drawing, typography and button styles.
Below Block Bound Keys are Keep Alt and Tab unblocked (default ON), a shortcut
assignment button and Clear. Click the assignment button, press a key/chord;
Escape, loss of focus or leaving the page cancels capture. No default shortcut
is claimed from another application. Errors are displayed only when relevant.

The exception passes ordinary left Alt, right Alt and Tab events, independently
of each other. It does not emulate Alt+Tab, synthesize input or alter analog
gamepad bindings. Changes take effect on the next press: an already-held key
retains its first-down routing through repeats and release. This prevents an
already-delivered down from losing its up when focus/settings change.

The shortcut toggles the same setting as the checkbox, marks the gameplay
profile dirty, requests the normal settings save, and explicitly invalidates
Configuration rather than waiting for mouse movement. RegisterHotKey owns the
shortcut globally with MOD_NOREPEAT. Replacement is registered before the old
registration is released, so a conflict preserves the working shortcut.
Startup registration failure leaves the saved preference intact and displays
an error; there is no repeated registration attempt every UI tick.

Windows must see the shortcut modifiers: modifiers used by the assigned chord
are also exempt from blocking while that shortcut is registered. Its trigger
key is exempt only with the matching modifier mask; outside the chord, bound
letters retain normal blocking. Prefer an unused function key if no additional
modifier exemptions are wanted. No attempt to bypass Windows privilege/security
boundaries is made. This is not per-device blocking.

BlockKeysAllowAltTab and BlockKeysHotkey are optional Main settings, not gameplay
profile fields. Old files default to true/zero. Invalid values reject the
prepared load without applying partial settings. These fields are deliberately
not added to mandatory profile validation.

## Validation and recovery

Source checkpoint: `.local/backups/block-keys-controls-20260909/`.
Portable tests exercise both Alt keys/Tab, shortcut validation, invalid usages,
unknown releases, repeated downs, changing routing while held, and hook seeding.
A Windows integration test registers actual hotkeys on a message-only window
and a competing thread, checking conflicts, replacement and release. It sends
no keyboard input to user applications.
Production-linked isolated serializer tests cover roundtrip, profile isolation,
legacy defaults and malformed shortcuts, plus the existing configuration and
transaction-failure regressions. Visual appearance and physical key behavior
are left for the owner's assessment; no visual verification is claimed.

Results: Release x64 build PASS; complete native runner reports all static and
portable C++ tests passed, including the Windows hotkey test. Production-linked
profile suite and rejected-startup preservation PASS. Inventory check PASS
(475 records). Evidence: `.local/block-keys-{build,native-tests,profile-tests}.log`.
Previous release EXE is backed up byte-for-byte in the source checkpoint above.
Deployed release SHA256:
`4B11F5272E71CF4A7BB2F29155EEEF34112A03E8F8CC1FD82D9D2BFF7AAC304C`.
