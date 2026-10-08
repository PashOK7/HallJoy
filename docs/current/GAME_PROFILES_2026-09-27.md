> Superseded 2026-10-03 by GAME_PROFILES_V2_2026-10-03.md (v2 page, service, shortcuts, import/export).
# Game profiles — local implementation, 2026-09-27

Owner follow-up: Profiles temporarily hidden; production page and foreground service creation disabled. Code and saved data retained. See MAD68_V2_DUAL_REVIEW_2026-09-27.md.

Owner approved a dedicated Profiles tab and intuitive game associations. No publication requested.
Backup before implementation: `.local/backups/profiles-20260927-115121.zip`.

## Behaviour

- Profiles tab: browse independently from activation; active profile also appears in the window title.
- Create from current settings or factory defaults with empty bindings; duplicate any valid profile without activating it.
- Rename preserves associations. Default is protected; activate another profile before deleting the current one.
- Deletion retains a recovery copy in the data root under `.internal/DeletedProfiles`.
- Associate exact executable paths using a file chooser or a list of running applications. Duplicate assignment to another profile is rejected with an explanation.
- Automatic mode is opt-in and persisted. Foreground-window events trigger an 80ms coalescing timer; no periodic process enumeration.
- Assigned foreground game selects its profile; an unrelated accessible application selects Default.
- HallJoy's own windows/dialogs retain the current profile. Unknown/inaccessible process identity retains it too.
- Manual Activate temporarily overrides automatic mode until Return to automatic; override ends when HallJoy restarts.
- Edits use existing debounced autosave. Undo edits restores the snapshot taken on activation (or startup); it is not a per-action undo stack. Autosave does not overwrite that in-memory snapshot.
- Existing bundled and legacy paired profiles remain the sole storage format. Associations and the automatic preference use separately validated, atomic `GameProfiles.ini` in AppPaths_DataRoot.
- Corrupt association documents are retained read-only for the session, with a visible error. Invalid profiles or save failure retain the prior active profile.
- Global tray/logging/window/layout/overlay preferences remain outside gameplay profiles.

## Runtime

Profile commits increment a shared revision under the existing commit gate. Realtime consumes the revision, clears old filter/controller/mouse history and publishes a neutral snapshot before evaluating the new mapping. No HID/backend restart is introduced for profile selection.
Onboard profile capture notices a changed revision without waiting for its ordinary 100ms settings check. Firmware already validates commits and resets mapper history; no firmware change or flash in this task.
The Profiles UI preflights the current onboard route before activating a profile that needs unsupported inputs. Transport/hardware failure is still handled by existing runtime diagnostics; profile activation cannot guarantee a healthy physical device.

## Validation

Initial production-linked profile/recovery suite passed. Added catalog roundtrip, case-insensitive executable conflict, focus policy, atomic replace failure, factory creation, duplicate safety, manual activation, undo after autosave, compatibility rejection, rename/association consistency and deletion recovery checks. Private-desktop control test checks browse does not activate and scroll containment. No visual inspection: owner evaluates the interface.
Final results:
- `.local/game-profiles-final-tests.log`: production-linked tests PASS; private-desktop browse, fixed error header, scroll containment and hidden redraw deferral PASS; 18 startup recovery scenarios PASS.
- `.local/game-profiles-static.log`: full static suite passed except the obsolete manual-save UI assertion. Updated that assertion for the approved autosave/undo UI; `pre_release_ui_static_audit.py` and engine owner audit now PASS.
- `.local/game-profiles-release-final.log`: ordinary Release and all six candidate gates PASS. Installed `build/bin/Release/x64/HallJoy.exe`, SHA256 `690114cab1d26ed20a5ef1392c2f22f9e485023e17ad1e5404a0e0720c6ed63a`.
- User profile backup: `.local/backups/user-profiles-20260927-120831.zip` (settings and bindings; no named profiles existed).
- No GitHub publication, firmware flash, visual run or physical game-profile test claimed.

Hidden/minimized profile switches defer configuration/keyboard visual rebuilding until restoration. Keyboard blocking hooks refresh immediately on an applied profile. The existing held-key routing policy preserves each press/release pair across changed blocking settings.
