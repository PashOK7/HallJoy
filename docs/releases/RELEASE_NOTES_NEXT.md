# Next release (unreleased)

- Informational keyboard/testing banners can be closed for the current run after
  one minute using a button beside the QR code. Restarting restores the banner
  and delay. Operational and limited-firmware warnings remain visible.
- Improved Profiles shortcut/Clear button spacing before the scrollbar, localized
  mode-label sizing and profile-selector insets. Dropdown fills now follow both
  rounded corners, including when hovering over the arrow.
- Korean, Japanese and Simplified Chinese UI text requests installed Windows
  script-appropriate fonts; the EXE does not bundle or download font files.

- Added a Language dropdown above Factory reset in Global settings. English
  remains the default; Korean, Japanese, Russian, Spanish, Italian, French,
  German and Simplified Chinese are available for common interface controls.
- The language applies immediately, persists independently of game profiles and
  falls back to English for unknown values. Failed saves retain the prior choice.
- Added delayed explanations for selected Global settings and Configuration
  controls in the eight additional languages. Detailed diagnostics and some
  specialist editor/error messages remain in English. See the
  [language guide](../development/INTERFACE_LANGUAGES.md) for scope and contribution
  instructions.
