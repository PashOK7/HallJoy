# RM-20 bounded external-number and INI contract (2026-09-06)

The common INI reader now has strict bounded unsigned and signed decimal
parsers. Unsigned values retain legacy `0x` notation but reject signs, junk
suffixes, and overflow. Signed values accept one optional sign and reject
hex-like or malformed input, including overflow at both 32-bit limits. Missing
optional values retain the declared default; malformed values do not silently
become zero or a valid clamped value.

Settings, curve presets, layout presets, and binding validation use the shared
contract for persisted numeric fields. Their policy is explicit: an optional
missing field uses its existing default; an invalid field rejects the staged
load (layout/validation) or retains the caller's declared default (legacy
settings/curve compatibility), without mutating runtime state before the whole
profile preparation succeeds.

The guarded input handle now rejects directories and reparse points before
Win32 INI parsing, keeps the file below the existing 16 MiB budget, and denies
concurrent replacement/write sharing for the duration of staging. Curve preset
loading now takes that same handle lease.

Focused portable parser tests cover signs, hex compatibility, junk, and both
overflow boundaries. Structural audits and source-only syntax checks passed.
No HallJoy runtime, HID, controller, output child, or ROG diagnostic was
started.
