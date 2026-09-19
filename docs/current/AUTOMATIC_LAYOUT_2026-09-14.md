# Automatic layout — 2026-09-14

> Follow-up: [telemetry coherence and preview stability](AUTOMATIC_LAYOUT_COHERENCE_2026-09-14.md).
> Transient failed reads previously triggered false K4/manual/K4 transitions;
> staged publication and explicit validity now prevent these rebuilds.

> Follow-up: [general UAP identification fix](AUTOMATIC_LAYOUT_UAP_FIX_2026-09-14.md).
> The original stable-device flag rejection was incorrect; regression fixtures
> now match real UAP flags. The original delivery hash below is historical.

## Owner decisions

Automatic layout is enabled by default. If more than one supported device is
reported, keep manual choice and show an English explanation. A successful exact
match locks Brand, Model, Variant and the layout editor button. Disabled or failed
automatic selection restores the independently saved manual preset. Manual mode
uses factory assignments, ignoring device remaps. UI, logs and code comments
remain English. This replaces FIRST_RUN_LAYOUT.md's historical one-shot policy.

## Implementation

KeyboardLayout/Automatic persists independently of PresetName. Missing preference
means enabled; malformed numeric values are rejected before changing state.
PresetName remains the manual fallback even while another layout is automatic.
The existing UI tick consumes cached backend telemetry and complete session maps;
it performs no HID enumeration, protocol request or settings save. Unchanged
snapshots do not rebuild geometry. Selection continues while the main window is
minimized. The existing layout-change notification refreshes the retained controls.

Exact Keychron/Lemokey/DrunkDeer identities and verified native layout tokens use
the existing reviewed catalog. NA87 remains ANSI as confirmed by the owner.
ATK Hex80 now supplies HEX80-ANSI only after the existing native session proof;
no other ATK models or variants are inferred. Native layouts with no implemented
remap reader display that limitation and use their factory geometry.

NA87 and IPI already read complete device maps when establishing a session. The
new shared snapshot associates factory HID with assigned HID and keeps these
assignments separate from persistent geometry. A complete association is checked
before any key changes. Missing geometry associations restore manual mode.
Assignments and labels move at fixed physical coordinates; the factory preset
and its file remain unchanged. Duplicate assignments and unassigned actions are
supported. Layer switching, macros and continuous monitoring of edits in another
vendor application are not implemented; maps are refreshed on native reconnect.

The two backend data paths now preserve both factory and assigned publication.
Switching automatic/manual mode selects the matching path atomically. Physical
alias chains prevent releasing one key from clearing another key assigned to the
same HID. IPI bound-key polling priority follows the selected interpretation.
NA87 subscriptions include the factory mask so an unassigned live action does
not suppress physical input in manual mode. NA87 event-driven holds keep their
existing stream-liveness semantics; IPI retains its existing freshness policy.

## Physical device ambiguity

Existing native backends expose one active session per protocol. Automatic
selection also consults a background Windows metadata inventory for active native
VID/PID identities. Container ID deduplicates the HID collections belonging to
one physical keyboard. Two matching physical containers leave manual choice
available, even when only one native session is active. Shared VID/PID metadata
is used conservatively to detect ambiguity, not as new protocol/model proof.
Unrelated VID/PID devices are ignored. Missing/ambiguous metadata never authorizes
automatic choice. Plugin counts and mixed-source counts still apply independently.

The Windows thread-pool worker opens no HID device and sends no protocol request.
Cached counts refresh at most once per second; topology changes invalidate them
immediately. Callers only read the cache and enqueue work. Requests coalesce while
a scan is running, stale generations cannot publish, and shutdown joins callbacks.
Container identifiers stay in memory and are not included in logs or settings.

## Validation and delivery

- Full static and portable C++ suite PASS: .local/automatic-layout-checks-2.txt.
  After the physical-inventory addition, final static suite PASS in
  .local/automatic-layout-static-final.txt; affected focused C++ tests PASS.
- Native physical-container fixtures cover duplicate interfaces, distinct devices,
  missing metadata and ambiguous results. Windows background-inventory test PASS
  with concurrent refresh/invalidation and nonexistent identities; metadata only.
- New native_layout_state test: coherent concurrent snapshots, atomic rejection,
  replacing a token after an earlier slot was freed, manual/automatic switching,
  eight IPI factory maps, NA87's90 distinct mapped positions, duplicate aliases,
  neutralization and held-event freshness PASS.
- Production-linked simulator profile/control test PASS:
  .local/automatic-layout-profile-test-4/profile-test-result.txt. Covers default,
  persistence, hotplug/disconnect, multiple-source fallback, locked picker,
  checkbox click, remap geometry, incomplete map rejection and all eight IPI maps.
- Diagnostic release native NA87 self-test exit0, including actual publication
  with duplicate assignments and switching back to factory values.
- Release/simulator builds PASS. Existing local-variable shadow and third-party
  ViGEm PDB warnings remain. No physical NA87/IPI/ATK validation claimed.
- A NA87 self-test invocation on the simulator variant was not recognized because
  that variant omits HALLJOY_IROK_NA87_NATIVE. Its ordinary local process and child
  processes were stopped; that invocation is not counted as a passed test. The
  actual NA87 test above ran on the diagnostic release and completed immediately.
  No visual assessment was performed.

Backup: .local/backups/automatic-layout-20260914/; generated ATK identity/report
backup: .local/backups/layout-integrate-ietaqhfj/.
Delivery uses the existing build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe.
SHA256: f24c682d516c803b2e2504fc92e48dc911ba4f47b419807b2a6250ff852c741c
