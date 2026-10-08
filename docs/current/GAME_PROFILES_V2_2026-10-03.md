# Game profiles v2 — design (2026-10-03)

The owner asked to finish game profiles: profiles that follow the game in focus,
designed to be convenient in daily use. This document records the design and
the owner's decisions. It supersedes the behaviour section of
`GAME_PROFILES_2026-09-27.md`. The hidden v1 page is replaced.

## Owner decisions (2026-10-03)

- When a game loses focus (alt-tab) but keeps running, the behaviour is a
  setting. Default: keep the game's profile until the game exits or another
  assigned game takes focus. Alternative: follow focus strictly.
- Notification on profile change: off by default; an option shows a small popup.
- First version also includes:
  - profile shortcuts;
  - import/export;
  - matching by window title.

  There is no tray quick-switch menu.
- Default profile stays as today; existing users see no change.
- Storage may be improved if useful. The profile file format is kept: migrating
  user data has more risk than benefit. Export builds one validated file from
  any profile kind.

## Concepts

- **Profile**: the gameplay configuration: bindings, curves and deadzones,
  virtual gamepad on/off and count, mouse-to-stick and Block Bound Keys.
  Layout, tray, overlay, logging and Pause/Resume shortcuts stay global.
- **Default**: the profile used when no assigned game is active.
- **Game rule**: executable plus optional window-title text, mapped to a
  profile.
  - The executable is matched by file name (`cs2.exe`), so moved or updated
    games keep working.
  - A full path is used only when the user picks it to tell apart two programs
    with the same name.
  - Title text is a case-insensitive "contains" match, for one EXE that runs
    several games (emulators, `javaw.exe`).
  - The most specific rule wins: path beats name, and title beats no title.
- **Mode**:
  - Automatic: the focused window chooses the profile.
  - Manual: the user picked a profile. It lasts until "Return to automatic"
    or until HallJoy restarts.

## Switching rules (automatic mode)

- HallJoy's own windows never switch profiles.
- An unknown or inaccessible process keeps the current choice.
- A focused window that matches a rule selects that rule's profile and becomes
  the current game.
- A focused window without a match:
  - "keep while running" (default): keep the current game's profile while that
    game process is alive. When the game exits, re-evaluate the focused window
    (normally Default);
  - "follow focus": select Default.
- Title rules also react to title changes of the focused window (an emulator
  loading a game). This title-change hook exists only while title rules exist.
- Switching is event driven, with no periodic process scan:
  - foreground events are coalesced for 80 ms;
  - process exit uses a wait registration.
- A switch saves the previous profile and commits the new one under the
  existing commit gate. The runtime publishes a neutral snapshot first, and no
  backend is restarted.

## Shortcuts

They use the existing shortcut engine (digital or analog presses, chords):

- **Next profile:** cycles through all profiles and enters Manual mode.
- **Return to automatic.**
- **Per-profile shortcut:** at most 12 profiles can have one. It enters Manual
  mode.

Conflicts with Block Bound Keys and Pause/Resume are rejected with the
existing message.

## Notification

When the option is on, a small, non-activating, topmost popup near the
taskbar shows "Profile: CS2" for about two seconds. It appears on automatic
and shortcut switches, not on switches made in the window. It is not visible
over exclusive-fullscreen games.

## Interface

**Profile selector**, always visible at the right end of the tab strip:

- the profile dropdown;
- a mode chip, "Auto" or "Manual";
- clicking "Manual" returns to automatic.

If the window is too narrow, the selector is hidden; the Profiles tab still
works.

**Profiles tab**, custom drawn like Global settings.

Left column:

- profile cards: name, active marker, number of games and the shortcut;
- "New profile" (from current settings or factory defaults);
- "Import...".

Right column, for the selected profile:

- actions: Activate, Duplicate, Rename (inline), Export, Delete;
- the games list: EXE icon, EXE name, optional title text (editable) and
  remove;
- adding games:
  - "Add running app" (visible windows with icons and titles);
  - "Browse...";
  - one-click suggestions from recently focused applications;
- the profile shortcut: set and clear.

Automatic switching section:

- enable checkbox;
- "When the game loses focus" setting;
- notification checkbox;
- "Next profile" and "Return to automatic" shortcuts.

Status line: active profile and mode, "Return to automatic", "Undo edits"
(restores the settings from when the profile was activated).

## Import / export

- `.hjprofile` is one INI bundle with the profile settings, all bindings
  sections and a `[HallJoyExport]` section: format version, profile name and
  the game rules (EXE names and titles, never full paths).
- Export works for any profile without activating it.
  - Active or bundled profiles are written as is.
  - Default and legacy pairs are composed from their two files. Global-only
    sections (`Window`, `InputOverlay`) are dropped, and the bindings come from
    the validated snapshot.

  The result is validated before it is written.
- Import validates settings and bindings in staging and creates a new profile
  with a unique name. It offers the file's game rules; conflicting rules are
  skipped and reported.

## Storage

`GameProfiles.ini` (data root) version 2:

- `[Profiles]`: Automatic, FocusLoss, Notify, NextShortcut, AutoShortcut,
  Count, ShortcutCount;
- `[GameN]`: Exe, Title, Profile;
- `[ShortcutN]`: Profile, Shortcut.

Version 1 files are read and upgraded on the next save:

- full paths become file names when that is unambiguous;
- otherwise the full path is kept.

Unknown versions are preserved read-only, as in v1. Profile files are
unchanged.

## Implementation (2026-10-03, local)

- `game_profile_rules.h`: pure rules and switching policy (portable). Test:
  `tests/game_profile_rules_test.cpp` in the portable gate.
- `game_profiles.h`:
  - `GameProfiles.ini` v2 with v1 read and upgrade;
  - `Session`;
  - duplicate and archive;
  - `Export` and `Import` (`.hjprofile`).

  `profile_ini.cpp` gains `Profile_WriteBindingsSnapshot`, so the bindings
  of an inactive profile are exported without activating it.
- `game_profile_service.h`: the application-wide service, started with the
  keyboard page and independent of the tab. It owns:
  - the foreground and title WinEvent hooks;
  - the game-exit wait registration;
  - manual mode;
  - recent applications;
  - shortcut publishing;
  - the notification popup;
  - the K4 onboard activation preflight.
- `input_shortcuts.h`:
  - actions NextProfile, AutoProfiles and 12 profile slots;
  - `CurrentBindings()` includes them, so conflicts with Block Bound Keys and
    Pause/Resume are rejected both ways (`App_ValidateCommandShortcut`,
    `App_ValidatePauseShortcut`);
  - presses are posted to the UI thread (`WM_APP_PROFILE_SHORTCUT`).
- `profiles_page.cpp`: the custom-drawn Profiles tab and the header selector
  (PremiumCombo plus the Auto/Manual chip). It replaces the hidden v1
  `profiles_page.h` page.
- `keyboard_page_main.cpp`: `kProfilesPageEnabled = true`; places the selector
  at the right of the tab strip; runs deferred visual refresh after automatic
  switches while hidden.

## Validation

- `build_release.ps1`: PASS. Production-linked profile tests:
  `game_profiles_management_foreground_policy_atomic_failure_undo=PASS`. The
  game profiles test covers:
  - rules with titles and shortcuts;
  - v1 upgrade;
  - export/import of a bundle profile and of Default;
  - stripping of global sections;
  - rejection of invalid files;
  - rename and delete carrying shortcuts.
- The private-desktop page test covers:
  - browse without activation;
  - stacked layout when narrow;
  - selector hidden when there is no room.
- `run_native_backend_checks.py --require-compiler`: PASS
  (`GAME_PROFILE_RULES=PASS`, `INPUT_SHORTCUTS=PASS`). The UI audit now reads
  `profiles_page.cpp`.
- EXE `build/bin/Release/x64/HallJoy.exe` SHA-256
  `1e95ccfe1e2cb8a7a1186760d967de4c2138c1858a7941c868e3ab3d7b39504a`.
- Backup: `.local/backups/before-game-profiles-v2-2026-10-03.tgz`.
- Not done by the agent:
  - visual evaluation (the owner's);
  - physical game-switching run;
  - publication.

## Flicker fix and one paint standard (2026-10-03)

The owner reported that the Profiles tab flickered, including its scrollbar,
while the other tabs were fine.

Cause: `CustomPageSurface_Present` clears the client before drawing. The other
pages call it on an off-screen buffer. The new page called it on the window
DC, because the double-buffer helper was a private `static` function inside
`keyboard_subpages.cpp`, and the new file re-implemented painting without it.

Fix:

- The helpers moved verbatim to `custom_page_surface.cpp` as
  `CustomPage_BeginBufferedPaint` and `CustomPage_EndBufferedPaint`. The six
  existing call sites in `keyboard_subpages.cpp` use them unchanged.
- New `CustomPageSurface_Paint` is the complete standard WM_PAINT for a
  retained page; the Profiles tab uses it.
- `tests/custom_page_paint_static_audit.py` fails the build when:
  - any `Present` call lacks an off-screen buffer created just before it;
  - a file keeps a private double-buffer helper;
  - the Profiles page does not use the standard.
- Configuration and Remap still compose with their own buffers (special
  clipping and live overlays). They pass the audit and were not refactored in
  this change.
- `build_release.ps1` and the native suite: PASS. EXE SHA-256
  `176b565b4fe3c6378004ca46aecebae1c0b004c4902861b277ef661e00a43fa1`.
  The owner evaluates the visual result.

## Owner UI feedback fixes (2026-10-03)

- **Dark area around the "Auto off" chip.** The selector filled its
  background with the window colour; the tab strip uses the panel colour.
  Fixed. The selector also paints through `CustomPagePaintScope` now.
- **"Recently in focus" did not update.**
  - The service recorded focused programs but did not tell the UI.
  - `Remember` now reports a change (a new program or a new order), and
    `EvaluateFocus` then refreshes the selector and the page.
  - Repeated focus of the same program only updates its title.
- **White system menus.** New component `ui_popup_menu.h/.cpp`
  (`halljoy::ui_menu::Track`):
  - **Style:** the dark PremiumCombo drop-down look (control background,
    border, accent hover row), optional 16 px icons and muted right-aligned
    detail text, a drop shadow, and rounded corners on Windows 11.
  - **Behaviour:** opens above the anchor when there is no room below, and
    scrolls with the wheel when long.
  - **Input:** mouse only plus Esc, so held game keys cannot pick an item
    (same intent as `ModalKeyboardBlock`).
  - **Used by:**
    - "New profile" (with the active profile name);
    - "Add running app" (program icons, window title, EXE name as detail);
    - the Input Overlay text field menu (with its real Ctrl shortcuts as
      detail);
    - the icon selector menu.
  - **Not converted:** the tray menu stays the system menu (it runs while the
    window is hidden; its focus handling is covered by tray tests); a
    separate change if wanted.
- **Selector never hides.** It shrinks with the room right of the last tab:
  - Full (at least 200 px): drop-down plus "Auto" / "Manual" / "Auto off"
    chip;
  - Compact (at least 110 px): drop-down plus status dot (accent = auto,
    amber = manual, grey = off);
  - Icon: one square button with the profile initial and the status dot. It
    opens the profile menu: profiles, "Return to automatic",
    "Manage profiles...".

  The page test covers all three modes and that the selector never overlaps
  the tabs.
- Also fixed the pre-existing C4456 warning in the overlay test
  (`key` shadowing).
- Build (no warnings) and native suite: PASS. EXE SHA-256
  `10ccda2ad75c4ea6907cc734efe0a683a1a60caec433a11a028bf6683c096469`.

## Mode button and persistent game icons (owner feedback, 2026-10-03)

- **Mode button.** It works by itself in every size; it no longer opens the
  Profiles tab.
  - Auto: click turns automatic switching off.
  - Auto off / Off: click turns it on.
  - Manual: click returns to automatic.
  - Full: "Auto" / "Manual" / "Auto off". Compact: a 56 px button "Auto" /
    "Manual" / "Off".
  - Icon: a square mode button with the status dot, next to the profile
    button (initial). The profile menu also has "Automatic switching on/off".
- **Game icons survive restarts.**
  - Rules have `path`, the last known full image path, used for the icon only
    and never for matching. It is stored in `GameProfiles.ini` v2
    (`[GameN] Path=`, optional; missing means empty).
  - It is set when a game is added (running app, browse, suggestion).
  - For rules without it (added before, or imported) the service learns it
    the first time the game is in focus (`LearnIconPath`). It is updated if
    the game moves.
  - v1 upgrades keep the old full path as the icon path.
  - Exports carry EXE names only, never full paths (they contain user
    names).
- `Rule` has a constructor with an optional path, so `{exe, title, profile}`
  stays warning-free. Tests cover the path round trip and the v1 icon path.
- Build and native suite: PASS. EXE SHA-256
  `ea13e981f5d8b331cbc02ba28541a1b6498e0722541543ab870592b5d34b117b`.

## Onboard limits never block a profile — 2026-10-06

Owner report: with the Keychron K4 HE onboard route active, Default could not
be activated ("This profile requires inputs unavailable in the current onboard
mode. The previous profile is still active."), so the user could neither go
back to Default nor reach it to fix its bindings.

- `Session::canActivate` (a veto checked before the switch) is replaced by
  `activationNotice`: advisory only, evaluated after a successful switch and
  shown as the Profiles status line.
- The onboard backend already handles a profile it cannot express: it closes
  the onboard session and reports "profile requires one pad and K4 keys, no
  mouse" (state 10+), exactly as when the same bindings are made by editing.
  The notice now says the profile was activated and the onboard controller
  stays off until it uses one gamepad, only K4 keys and no mouse-to-stick.
- Test: `game_profiles_test.h` checks that a notice does not block activation
  and is reported, and that the next activation clears it.
