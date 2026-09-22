# HallJoy 1.5.1

Fixes a startup failure that could prevent HallJoy from opening with older,
incomplete or damaged profile data, or a missing active profile.

## Download

Download and run [HallJoy.exe](https://github.com/PashOK7/HallJoy/releases/download/v1.5.1/HallJoy.exe).
Close the previous HallJoy before replacing it. **Do not delete your settings to update.**
Only the EXE is needed to run HallJoy; there is no archive to extract.
If ViGEmBus is missing, HallJoy offers its embedded installer.

## What changed

- Automatically tries valid settings, bindings and known backup files instead of
  closing with "HallJoy profile could not be loaded".
- Keeps recoverable data and resets unavailable parts when recovery is needed.
- Preserves original settings and bindings in `.internal/ProfileRecovery` inside
  the HallJoy data folder before replacing settings. Identical retries reuse the
  same backup instead of creating duplicate copies.
- Leaves other saved profiles, custom layouts and curve presets untouched.
- If safe backup or saving is blocked, continues in memory without saving changes
  and explains the limitation. Storage-folder initialization failures remain a
  separate issue; this update does not bypass Windows permissions.
- Adds 16 startup recovery regression scenarios, including repeated starts and
  failures at all five atomic-save stages. Existing settings/profile tests remain.

The affected-PC user confirmed the corrected executable starts successfully.
After automatic recovery, check your bindings before playing.

## License documents and source

[Third-party notices](https://github.com/PashOK7/HallJoy/releases/download/v1.5.1/THIRD_PARTY_NOTICES.md)
and [LICENSE](https://github.com/PashOK7/HallJoy/releases/download/v1.5.1/LICENSE)
are accompanying license documents, not additional installation files.
[Source code](https://github.com/PashOK7/HallJoy/tree/v1.5.1) is available separately.

SHA-256 of HallJoy.exe:
`4BA1CD93D1F1BED775B67DB9B65DBB9EE294F03D47AFE6EC948147303DF776C4`

HallJoy is not Authenticode-signed; Windows may show an unknown publisher.
For help and feedback, join [HallJoy Discord](https://discord.gg/5FQ297yZh).
