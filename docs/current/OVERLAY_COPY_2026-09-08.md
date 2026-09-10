# Overlay address interaction — 2026-09-08

Address is now a full-width copy action rather than a passive chip. Left-aligned
URL, right-aligned Click to copy affordance, hover border and hand cursor. Both
the address and Copy URL use one clipboard handler. Feedback says Copied only
after SetClipboardData succeeds; failure is reported separately. A two-second
one-shot timer clears feedback and is cancelled on destruction. Copy does not
save settings. This is click-to-copy, not a text-selection editor.

Source backup: `.local/backups/overlay-copy-20260908/`. Static regression checks
cover shared action, success/failure feedback and timer cleanup. No visual tests.

All static audits and Release x64 build passed; logs are
`.local/overlay-copy-static.log` and `.local/overlay-copy-build.log`.
Previous executable backed up, release replaced and relaunched via Explorer.

Follow-up: removed the redundant Copy URL button from the active toolbar at the
user's request. Start/Stop and Open overlay remain; address click-to-copy and its
success/failure feedback are unchanged. Updated toolbar regression expectations.

Next follow-up: address shares the actions row when at least 350 logical pixels
remain; otherwise it wraps below the buttons. Geometry checks cover both modes
and prevent button/address overlap at the tested widths and DPI scales.
