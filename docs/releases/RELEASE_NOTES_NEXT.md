# Next release (unreleased)

- Game profiles: each game can have its own profile, selected automatically
  while the game is in focus.
  - The active profile is always visible at the right of the tab strip.
  - The Profiles tab manages profiles and their games. Add a running game,
    browse for its EXE or click a recently used program. A window-title
    filter covers programs that run several games.
  - When a game loses focus, HallJoy keeps its profile while the game runs,
    or switches to Default if you choose that.
  - Shortcuts: next profile, return to automatic, and one per profile.
  - Profiles can be exported to a `.hjprofile` file and imported.
  - An optional notification shows when the profile changes.
- Switching tabs now slides smoothly between them, and the tab highlight moves
  along. All tabs are drawn the same way, without flicker.
- Remap: dragged icons have motion blur and move at the full refresh rate of
  the monitor they are on, also with several monitors at different rates.
- MCHOSE Ace 68 Air III is now Supported (no testing notice).
- Redragon K686 HE (K686BG-RGB-M, K686RGB-M): experimental support with automatic layouts.
- WLMOUSE Ying75 is Supported, with automatic layout.
- Logitech G PRO X TKL RAPID and PRO X2 RAPID: experimental analog support (0.1 mm steps).
- Fixed: with the Keychron K4 HE onboard mode active, a profile whose bindings
  the onboard mode cannot use (for example Default) could not be activated at
  all. It now activates, and HallJoy explains what to change.
- Fixed: HallJoy could stay paused right after start, with Resume not working
  until a restart. A failed start now recovers automatically and says so.
