# Input overlay toolbar — 2026-09-08

Moved Start/Stop, Open overlay and Copy URL ahead of appearance settings in the
active retained page, including item/tab order. Compact buttons wrap by available
width; status has a green running marker and a muted stopped marker. URL uses
the shared chip renderer, followed by a shorter OBS instruction. Server errors
remain visible in a separately measured wrapped row. Port and server operations,
settings persistence, URL construction and disabled-Open behavior are unchanged.
Removed duplicate footer controls and captions. Text now uses the same stock
system font as the shared Global/Configuration retained content.

Backup: `.local/backups/overlay-toolbar-20260908/keyboard_subpages.cpp`.
New audit checks ordering, action availability, retained error reporting and
wrapping geometry at six widths and four scales. No screenshots, visual probes
or visual acceptance checks: the user explicitly owns visual evaluation.

All static audits and Release x64 build passed (existing ViGEm PDB warning).
Logs: `.local/overlay-toolbar-static.log`, `.local/overlay-toolbar-build.log`.
Release deployed and relaunched through Explorer. Previous executable backed up
alongside the source; SHA256SUMS.txt updated. Dropdowns use the same font as text.
