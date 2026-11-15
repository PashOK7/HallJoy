# Interface languages

Open **Global settings**, scroll to **Language** immediately above **Factory
reset**, and choose a language. The selection applies immediately and is saved
for the whole application. English is the default, including when an older
settings file has no language key or a language code is unrecognized.

| Language shown in the dropdown | Settings code |
| --- | --- |
| English | `en` |
| 한국어 | `ko` |
| 日本語 | `ja` |
| Русский | `ru` |
| Español | `es` |
| Italiano | `it` |
| Français | `fr` |
| Deutsch | `de` |
| 简体中文 | `zh-CN` |

The dropdown uses HallJoy's existing PremiumCombo and dark retained-page
renderer. A language change does not restart the engine or recreate the pages.
It refreshes tab captions, marked control labels, combo display text and retained
page caches. The language names always use their own script, so English can be
selected even when another language is active.

With a language other than English selected, hold the pointer still over these
settings for about two seconds to see an explanation:

- Global settings: language, automatic layout, engine pause/resume, logging,
  minimize/close to tray and factory reset.
- Configuration: Snap Stick, Last Key Priority, Block Bound Keys, axis inversion
  and the response-curve graph.

Help uses a native tooltip with a 1,800 ms hover delay and no repeating app timer.
It hides when the pointer leaves, a button is pressed, the page scrolls or hides,
capture changes, or the language changes. English retains the existing interface
without these additional explanatory tooltips.

## Translation boundaries

The embedded catalog covers main tabs and common controls, profile-management
controls and descriptions, Global settings, curve controls, overlay controls,
Remap instructions and the factory-reset confirmation. Detailed protocol
telemetry, some uncommon editor/error messages, legal text and diagnostic logs
remain in English. Unknown UI strings fall back to their original English text.
The translations have not been reviewed by native speakers; corrections are
welcome. This is an initial localization catalog, not a claim of complete
native-language coverage of every diagnostic or specialist editor.

## System fonts and readability

For Korean, Japanese and Simplified Chinese, the UI requests **Malgun Gothic**,
**Yu Gothic UI** and **Microsoft YaHei UI**, respectively, from the fonts already
installed in Windows. If the requested family is unavailable, it requests Segoe
UI and leaves script fallback to Windows. No font is downloaded, installed,
embedded in the EXE or copied into the distribution. These Windows fonts are
proprietary system resources, not fonts relicensed under HallJoy's source license.

Microsoft's [font redistribution FAQ](https://learn.microsoft.com/en-us/typography/fonts/font-faq)
permits Windows applications to use system fonts for screen rendering and
distinguishes that from redistribution or embedding, which requires separate
rights. The families are documented in the
[Windows 11 font list](https://learn.microsoft.com/en-us/typography/fonts/windows_11_font_list).
Do not copy font files from Windows or another application into HallJoy.

The GDI UI font cache retains explicit heading sizes and provides CJK body text
with a minimum 13-pixel character height at 96 DPI, scaled for the target window.
It preserves the existing fonts in the other six languages. CJK UI text drawn
with GDI+ requests the same installed family. Key legends and model identities
are not translated. Native font tests exercise 96/144/192 DPI, cache reuse,
family selection, line metrics and sample glyph coverage when the family exists.

## Dismissing the keyboard community banner

An informational keyboard/testing invitation exposes a close button to the right
of its QR code after the first 60 seconds of each application process. Closing it
reclaims the banner's space for the pages below. The choice lasts only for that
process: reconnecting a keyboard or changing language does not reset it, while
restarting HallJoy restores the banner and the one-minute delay. The timer wakes
once; there is no persistent preference or repeating countdown repaint.

Communication errors, a keyboard held by another program, unsupported hardware,
research-only or unavailable support, and known limited-firmware notices remain
visible. They can replace a previously dismissed informational invitation.
The permanent Discord card in Global settings remains available.

Profiles shortcut rows allocate Clear buttons from the content's right edge,
reserve a gap before the scrollbar, and measure the localized button caption.
The profile selector measures its mode label and keeps both controls inset;
dropdown backgrounds and arrow hover fills follow the rounded outer boundary.

Device/model names, ANSI/ISO/JIS variants, physical key legends, user-created
profile/layout/preset names, paths, URLs and configuration identifiers keep their
original values. Changing the language does not alter keyboard compatibility or
hardware support status.

## Persistence and failure behavior

`settings_ini.cpp` stores `Language` in the application's `[Window]` section.
The ordinary atomic window-settings transaction writes and reads it back before
replacement. If saving fails, the previous language and dropdown selection are
restored. Profile-only loads ignore this key, including a manually inserted one.
Factory reset removes the preference together with the other app settings and
therefore returns to English on restart.

## Adding or correcting UI text

The source files are under `src/HallJoyProject/HallJoy/`:

- `ui_language.h`: locale metadata, English fallback and refresh revision.
- `ui_translations.inc`: UTF-8 catalog, one row per message, with nine columns in
  the order declared at the top. `Help.*` keys identify the localized explanations.
- `ui_language_win32.h`: explicit native-label registration, refresh broadcast
  and retained-page hover help.

Use `UiText` explicitly at presentation boundaries. Do not apply it to settings
keys, names stored by the user, protocol values or serialized data. Use
`PremiumCombo::AddUiString` for fixed UI options: it translates their appearance
while keeping canonical item text and indices for existing logic. Continue using
`AddString` for user names, hardware and language names. Retained item models keep
English source text and translate when painting, so switching back to English
does not retain a translation from an earlier language.

Use `UiLabel` or `UiCreateWindow` only for fixed UI captions that need live
updates. Never register edit-box contents or user-created names. Keep closed
combo faces in the retained page; show a combo HWND only for its existing popup
interaction. Help and localization do not introduce another scroll architecture.

`ui_language_test.cpp` checks the nine language codes, default/fallback,
representative translations, catalog uniqueness/completeness and printf argument
contracts. The production-linked private-desktop picker test also exercises
selection, saved preferences, old/unknown values, profile isolation, failed-write
rollback and native tooltip visibility. Run the repository's normal native suite
and release build; see the [validation record](../validation/INTERFACE_LANGUAGES_2026-11-15.md).
