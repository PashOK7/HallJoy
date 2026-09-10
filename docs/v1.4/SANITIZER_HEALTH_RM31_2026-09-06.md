# RM-31 sanitizer health control — 2026-09-06

Both portable sanitizer runners now first compile an isolated test-only executable
with the same AddressSanitizer/UndefinedBehaviorSanitizer flags as the parser
corpus. Its one-byte volatile heap overflow is intentional. The runner succeeds
only when that child exits unsuccessfully and its captured output contains an
`AddressSanitizer` diagnostic; it then runs the normal parser corpus separately.

This distinguishes three results that were previously too easy to conflate:

- `PASS`: the known-bad control was detected and the production parser corpus
  completed cleanly;
- `FAIL`: the control did not trigger the loaded sanitizer or the corpus failed;
- `UNSUPPORTED`: compiler/runtime prerequisites are absent, reported as such by
  the runner before any claim of sanitizer coverage.

The health control is not linked into HallJoy, the plugin, or ordinary portable
tests. One shared check prevents the parser-fuzz and Aula-specific gates from
silently drifting to different sanitizer-health assumptions. The local parser
execution detected the intended failure and then completed the 250,000-iteration
Aula/Hex80/MAD68 parser fuzz corpus cleanly. This is coverage evidence for
those three pure protocol modules only; it is not a Windows TSan, HID, firmware
or whole-product claim.
