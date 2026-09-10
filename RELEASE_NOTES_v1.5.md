# HallJoy 1.5.0

Windows x64. This update focuses on configuration reliability, keyboard layouts,
the layout editor, and everyday usability. Download and run
[HallJoy.exe](https://github.com/PashOK7/HallJoy/releases/download/v1.5.0/HallJoy.exe).
No archive extraction is needed. Close the previous HallJoy before replacing it.
Keep your existing HallJoy data folder; do not delete it to update.

**Only HallJoy.exe is needed to run the app.**
[Third-party notices](https://github.com/PashOK7/HallJoy/releases/download/v1.5.0/THIRD_PARTY_NOTICES.md)
and [LICENSE](https://github.com/PashOK7/HallJoy/releases/download/v1.5.0/LICENSE)
are accompanying license documents, not additional installation files.
[Source code for this release](https://github.com/PashOK7/HallJoy/tree/v1.5.0)
is available separately.

## Highlights

- A HallJoy Discord community is now available for help, feedback, keyboard
  support requests, and release updates, with direct links inside the app.
- Brand / Model / conditional ANSI-ISO-JIS selection shared across settings,
  Input Overlay and the layout editor. Exact duplicate model layouts are combined;
  existing saved names still resolve and customized legacy layouts remain separate.
- 68 manufacturer model/physical-variant entries represented by 55 shared layouts,
  plus two technical presets. Layout additions do not imply new analog protocols.
- Manufacturer-derived Keychron, Lemokey, DrunkDeer, Aula, Redragon, Razer, NuPhy
  and Wooting layouts, including compound ISO/JIS Enter geometry.
- Pixel-precise layout editing, larger zoom range, edge resizing, rulers/guides,
  snapping, and Save / Discard / Cancel when leaving an unsaved editor draft.
- Independent Input Overlay layout, with Same as main layout as the default.
  Select this option under Brand; Model and Variant are hidden while it is selected.
  Compact server controls and an address field with copy feedback.
  Keys present only in the overlay layout (such as a numpad) receive analog input
  independently of the main preview layout.
- Block Bound Keys user-configurable toggle binding and optional Alt/Tab exclusion
  (enabled by default). A detected elevated-window input limitation recommends
  restarting HallJoy as administrator; the warning remains for the session.
- Clear pause/resume feedback, missing-keyboard diagnostics and Discord access.
  Pausing releases the keyboard for its web configurator. A softly pulsing card
  over the keyboard preview offers Resume; Global settings keeps its own control.
  Ordinary keyboard presses no longer operate main-window buttons or dropdowns;
  explicit text editing and user-assigned bindings still work.
- Configuration persistence fixes and quieter diagnostics: normal continuous
  logging is off by default; crash and missing-keyboard reports remain automatic.
- Mouse settings remains disabled; unfinished keyboard routes remain frozen.

## Installation and support

ViGEmBus 1.22.0 is the system driver dependency. Its pinned installer is embedded
in HallJoy, verified before elevation, and offered if the driver is missing.
Do not install a separate UAP or Wooting Analog SDK for HallJoy: its private
runtime is embedded. Close connected keyboard web-driver tabs, which commonly
conflict with HallJoy. Desktop software such as Razer Synapse may coexist and
only needs closing when troubleshooting a suspected conflict.

Use [HallJoy Discord](https://discord.gg/5FQ297yZh) and the
[support report template](docs/SUPPORT_REPORT.md). Do not send passwords or typed
text. Open HallJoy folder provides access to the diagnostic log.

## Known limitations

- The 18 newly added Razer/NuPhy/Wooting physical variants require manual layout
  selection; no region is guessed from VID/PID. Prior exact first-run matches remain.
- Razer layouts added here cover Huntsman V3 Pro Mini ANSI/ISO/JIS. Layouts for
  Huntsman V2 Analog, Mini Analog, V3 Pro and V3 Pro TKL are deferred; their existing
  analog support is not disabled by this limitation.
- NuPhy Air75 HE's screenshot-macro key has no published UAP HID counterpart and
  is omitted (83 represented positions); Air60 HE is also included.
- Wooting layouts cover One, Two, Two HE, 60HE, 60HE+ (ANSI/ISO), and 80HE
  (ANSI/ISO/JIS). 60HE v2, split-space variants and UwU are not part of this batch.
- Keychron HE analog support discussed here uses custom UAP-compatible firmware;
  this is not a promise that every stock firmware exposes analog input.
- ROG Azoth 96 HE, Aula HERO84 HE, IROK ND75 and Attack Shark X68 HE remain frozen.
- HallJoy itself is not Authenticode-signed; Windows may show an unknown publisher.
  SHA-256 verifies file integrity, not publisher authentication.

New layout geometry has automated checks, not a claim of physical testing on
every listed keyboard. See [supported hardware](SUPPORTED_HARDWARE.md).
