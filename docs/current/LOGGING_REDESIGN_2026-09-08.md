# Production logging redesign — 2026-09-08

## Contract agreed with user

Continuous normal-operation logging defaults off. A completed search with no
working analogue source (the exact red-banner predicate) is an automatic support
incident even when logging is off. Absence does not prove unsupported hardware:
discovery, access, transport, provider or protocol failure may be responsible.
Crashes remain independently reportable. No text typed by the user is collected.

## Audit and implementation

- Production DebugLog and StabilityTrace call sites were compiled out. Simply
  adding a checkbox there would not recover the prior failure context. Keep
  high-volume/raw debug call sites compiled out; introduce a structural support
  recorder instead, with nonblocking fixed-capacity producers and bounded RAM
  history. Engine transitions are recorded as they happen, not inferred later.
- Overlay wrote a separate unbounded synchronous overlay_perf.log every five
  seconds. Remove that sink and gate aggregate formatting on continuous logging;
  route enabled summaries into the support writer.
- Per-backend and analog-host routine file sinks are already diagnostic-build
  gated in production. Do not turn on their potentially input-bearing raw dumps.
  Specialist firmware diagnostic builds remain separate explicit workflows.
- One production support file: LocalAppData/HallJoy/HallJoy.log. No per-device,
  per-reconnect or timestamped support files. LocalAppData avoids requiring admin
  rights or write access beside an installed executable. Existing old logs are
  not deleted. Existing crash handlers/emergency keyboard recovery remain intact.
- The file is bounded to 4 MiB; reset/incident history is published through one
  transient .tmp and replacement, preserving the prior report on write failure.
  A new incident includes the RAM history; reconnect resolution is written once.
- The worker performs passive Windows HID metadata enumeration only; IDs and
  interface numbers are included, not serials, instance paths or editable names.
  Inventory candidates include non-keyboards and are explicitly labelled as such.
  No unknown-device opens, calibration, feature requests or experimental commands.
- Evidence includes engine state/native error, support predicate, SDK state,
  analogue counts/read errors, UAP availability/status/transport errors/restarts,
  and native protocol identity, capabilities, successful/failed update counters.
  Raw keys, depths and typed text are deliberately excluded. This is triage
  evidence, not a substitute for firmware/protocol reverse engineering.
- Global checkbox is a machine/user preference, persisted only in full settings
  and ignored when loading gameplay profiles. Open logs folder exposes the file.
  Disk errors are surfaced in the page, not silently presented as success.

The banner event is latched as well as sampled, so a transient visible banner
cannot disappear before the background writer notices. Search status is now
published atomically because the recorder adds a cross-thread reader. UAP parent
start/failure/exit and poisoned shutdown events are recorded immediately; fatal
engine/UAP failures also force a report. Intentional host shutdown is not treated
as a new failure. Portable and isolated test data-root policies are respected.

Real writer Windows test passed: normal/off creates no file, mandatory incident
contains prehistory, reconnect resolution, opt-in on/off, and I/O failure state.
Real settings/profile serializer tests passed for enabled/disabled persistence
and profile independence, alongside existing configuration/curve regression tests.
No screenshot or visual checks were performed.

## Global settings wording and folder action

User requested simpler naming and removal of the permanent explanatory hint.
Checkbox now reads `Enable logging`. The independent `Open HallJoy folder`
button is alongside the other folder/layout utilities, before the timing
sliders, and opens AppPaths_DataRoot directly rather than the logging API.
This is the user-data directory (settings, profiles, layouts and logs), including
portable-mode routing. Logging behavior, persistence and automatic incidents
are unchanged. The permanent hint and its reserved blank space are removed;
an actual log-write failure still receives an error-only line.
Release build and updated static checks passed. Deployed SHA256:
`6481117DCF6338B49E63A58C7E19D2AC007A11F15151D4A689876C50F9A6D47A`.
Previous EXE: `.local/backups/HallJoy-before-logging-ui-20260908.exe`.

Follow-up: removed the redundant Open Layouts Folder action (including legacy
child control, drawing, hit target, handler and reserved spacing). Open HallJoy
folder remains; Layouts is accessible inside that data directory. Layout editor
and layout storage are unchanged. Storage UI audit now checks the common folder
action instead of requiring the removed shortcut.

Validation completed: full static/portable suite passed (`.local/logging-full-tests.log`),
latest static suite passed (`.local/logging-static.log`), actual writer failure and
recovery test passed (`.local/logging-writer-test.log`), real settings/profile tests
passed (`.local/logging-persistence-tests.log`), Release x64 build passed
(`.local/logging-build.log`, existing ViGEm PDB warning). Failed reports retry after
five seconds rather than being dropped or busy-looping. Release replaced and
relaunched through Explorer; previous executable retained in the backup folder.
Backup: `.local/backups/logging-redesign-20260908/`.

## Release regression correction

The final validation-list edit incorrectly made DiagnosticLogging mandatory in
complete gameplay bundles. The profile suite predated that final edit, so its
pass did not validate the shipped source. Existing profiles were rejected without
being modified. Removed the preference from the gameplay schema; validate it only
as an optional 0/1 application setting, defaulting off when absent, and capture
its parsed value before the apply closure. Added pre-upgrade settings/malformed
optional-field regressions and a static guard against adding it to required fields.
Process existence is not startup success; subsequent launch checks must verify
the actual main-window class and absence of startup error dialogs (no screenshots).

Corrected-source validation: `.local/logging-compat-static.log`,
`.local/logging-compat-build.log`, `.local/logging-compat-profile-rerun.log` passed.
The first profile runner was blocked while the failing production instance held
the singleton; reran successfully after closing its error dialog. Deployed the
corrected binary. Programmatic startup check confirmed WootingVigemGui visible,
PID 24272, no visible error dialog. No user profile/configuration files were
manually altered or reset. Regression binary retained in the backup folder.
