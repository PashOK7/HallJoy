# Global settings label cleanup — 2026-09-08

User requested removing the automatic-saving notice and Device access heading.
Removed both from Global_RenderContent and removed the obsolete hidden native
notice control, field and references. Reduced the space between UI refresh
slider and engine pause/resume button to 18 logical pixels; following content
and scroll extent follow the same layout calculation (34px shorter).

No settings persistence, engine button behavior, factory reset warning or
device driver changes. Release x64 build passed (existing ViGEm PDB warning);
all static audits passed: `.local/global-label-removal-static.log`.
Source inventory regenerated. Replaced build/release/HallJoy.exe and restarted
through Explorer, retaining normal user launch. SHA256SUMS.txt updated.

Backups: `.local/backups/keyboard_subpages-before-global-label-removal-20260908.cpp`
and `.local/backups/HallJoy-before-global-label-removal-20260908.exe`.
