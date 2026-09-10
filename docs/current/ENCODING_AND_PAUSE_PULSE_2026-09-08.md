# UTF-8 build contract and paused pulse — 2026-09-08

Supersedes the static-only glow decision in PAUSE_BUTTON_UI_2026-09-08.md.

The Pausing/Resuming ellipsis was valid UTF-8 source, but MSVC had no explicit
source charset and interpreted it using the Windows ANSI code page. Set /utf-8
for all project configurations, with invalid-source warning C4828 as an error.
Added .editorconfig and a strict UTF-8 / common-mojibake audit, executed both by
the standard static runner and directly before MSVC compilation. Python (`py`)
is therefore required for direct project builds as well as BUILD.cmd.

Audit found six CP1251 remap source/header files: bumpers, dpad, guide,
startselect, sticks and triggers. Converted losslessly to UTF-8; no code logic
changes. ViGEm Client.h had CP1252 author-name comments and was likewise converted.
Replaced one already irrecoverable remap_icons.cpp comment with a neutral English
comment. Original bytes and previous UI/project source are backed up in
`.local/backups/encoding-pulse-20260908/`.

Pause pulse is a 2.4-second cosine cycle, sampled at 33 ms. Timer is enabled only
for Paused, visible/nonminimized page and an on-screen button; stopped when hidden,
offscreen, resumed, faulted or destroyed. Paint/show/state events reevaluate it.
Each tick invalidates only the halo rectangle, without dirtying the retained
page cache. Pulse is composited after cached content and excludes button text.
Global page double buffering now allocates only the invalidated rectangle; other
pages retain their existing behavior. No input polling interval dependency.

The resource editor's existing BOM-marked UTF-16LE HallJoy.rc is explicitly
preserved and checked as UTF-16, rather than misconverted; .editorconfig records
this resource-file exception.

Validation: 442 authored files passed encoding audit; all static audits passed
(`.local/encoding-pulse-static.log`); source inventory passed (466 records).
Release x64 MSVC build passed with the existing ViGEm PDB warning. Binary scan
confirmed both UTF-16 Pausing/Resuming labels contain U+2026. The audit also
demonstrated rejection during a direct MSBuild invocation before compilation.

Real release PID 20432: pause/resume without hover succeeded. Two captures 800 ms
apart changed exactly 3202 pixels, confined to glow bounds (34,696)-(256,740);
the rest of the window was identical. Artifacts: `.local/pulse-a.png` and
`.local/pulse-b.png`. Resumed after verification; HallJoy remains active.
Release SHA256: `612197A79F946CAF8682CFBDE87BA95E64D8EB6C902765B600D992AF9C115E39`.
Previous executable is also retained in the backup directory above.
