# Block Bound Keys controls

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
