# Personal portable copy inside a checkout

The optional `Start.cmd` / `Update.cmd` helpers keep a personal runnable copy in
`.local/portable/`. This directory is ignored by Git. Source files and public
documentation can be committed without committing keyboard settings or logs.
The normal standalone EXE and Windows installer remain available.

## First run and updates

From the repository root, run `Update.cmd` to download the latest stable official
HallJoy EXE. The helper accepts only the upstream GitHub release URL and verifies
the asset's published SHA-256 before installing it. Without a digest it fails.
It creates the `HallJoy.portable` marker so HallJoy stores data beside that EXE.
Run `Start.cmd` to launch this copy with an explicit working directory.

PowerShell 5.1+ is sufficient for these helpers. Check without downloading or
replacing the executable:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/update_portable.ps1 -CheckOnly
powershell -NoProfile -ExecutionPolicy Bypass -File tools/start_portable.ps1 -CheckOnly
```

There is no automatic update on launch. Updating refuses while any HallJoy process
is running and checks again after the download. It replaces only the portable EXE
and release metadata; settings, profiles, layouts and curves are left alone.
Previous EXEs are kept in `.local/portable/backups/update-<id>/`. To roll back,
close HallJoy, back up the current EXE, then copy the selected old EXE and its
release.json (if present) back into `.local/portable/`. Keep your data backup too:
an application downgrade does not reverse settings-format changes.

`Update.cmd` downloads a released application; it does not run `git pull`, build
the source checkout or update a copy installed through the Windows installer.
It never installs a driver or changes keyboard firmware. The launcher and updater
do not modify the permanent PowerShell execution policy.

## Bring existing settings into this checkout

Close all HallJoy copies before copying data. Preserve your original folder.
Back up the destination before overwriting any file. Copy these items from your
existing portable folder into `.local/portable/`:

- `settings.ini` and, for older configurations, `bindings.ini` if present;
- `GlobalProfiles/`, `Layouts/`, and `CurvePresets/`.

Keep or create an empty `.local/portable/HallJoy.portable` marker. Use a matching
or newer HallJoy EXE, then launch through `Start.cmd`. Do not copy logs, crash
dumps or temporary `*.halljoy-new-*` files as configuration. The Default profile
can be stored in settings.ini even if the three data folders are empty.

## Installed copy and portable copy

The installer uses `%LOCALAPPDATA%\HallJoy` for user data. The portable launcher
uses `.local/portable`. They are separate data directories. To transfer settings
in either direction, close both copies, back up both directories, then copy the
same settings/profile items listed above into the destination. The portable
marker belongs beside the portable EXE; do not copy it to the installed folder.
Restart the intended copy and verify its bindings in Gamepad Tester.

There is no background synchronization: merging two independently edited profile
sets automatically could overwrite newer settings. Choose the source explicitly.
Updating your Git checkout leaves ignored personal data in place, but `git clean
-fdx` can delete ignored files. Keep backups outside the checkout before using
clean/reset tools that remove files.

## Contributing

Use the current contribution branch for public scripts/docs. Before committing,
review `git diff --cached --stat` and `git status --short`. `.local/`, legacy
`app/` copies and root-level personal settings are ignored; source/test fixtures
elsewhere remain versioned. Ignore rules do not remove files already committed.
Original project: [HallJoy](https://github.com/PashOK7/HallJoy).

Maintainers can run `powershell -NoProfile -ExecutionPolicy Bypass -File
tools/test_portable_helpers.ps1` for isolated updater/launcher tests. The suite
uses fake GitHub responses and disposable files; it does not access the network,
run HallJoy or touch personal settings.
