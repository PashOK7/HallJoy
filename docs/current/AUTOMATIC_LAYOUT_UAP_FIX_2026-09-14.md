# Automatic layout: UAP identification fix — 2026-09-14

The owner reported that the locally connected Keychron K4 HE was not selected.
Read-only Windows PnP metadata confirmed VID3434/PID0E40, usageFF60:0061.
No keyboard configuration or firmware changes were made.

## General root cause

KeyboardLayout_UpdateAutomatic and the historical first-run selector rejected
DeviceFlag_DuplicateSafeId. That flag is set by the real UAP producer whenever
MakeDeviceIdentity receives a HID path; it denotes an identity that distinguishes
identical physical devices. It does NOT mean another keyboard is connected.
Consequently ordinary identified UAP keyboards could all be rejected. The prior
synthetic layout tests omitted the flag and therefore missed this production case.

Both selectors now accept this flag. Actual source/device counts still reject
multiple keyboards, and exact VID/PID/interface/matrix checks remain in place.
Production-linked regression inputs now carry the real UAP flag combination and
exercise all36 Keychron identities, Lemokey ISO and all seven verified DrunkDeer
models, plus existing manual/automatic/multiple-device transitions.

## Related matrix metadata gap

The UAP constructor had a separate partial PID list for Keychron dimensions.
Several ISO/JIS and later catalog variants could therefore report zero dimensions
and fail an otherwise exact match. UAP now consumes keychron_layout_identities.h,
the same generated reviewed table used by HallJoy. The plugin build stages that
header explicitly. This changes metadata, not analog protocol admission or
stock-firmware support. The K4 topology audit follows the shared data source.

Layout transitions now log automatic.selection with status, counts, VID/PID,
usage, dimensions and flags. No path, serial, key assignment or input value is
included. The existing single HallJoy.log remains the diagnostic artifact.

## Validation

- Simulator production profile/control test PASS, including the expanded stable-ID
  cases: .local/automatic-layout-uap-profile/profile-test-result.txt.
- Native plugin ABI0/ABI1 rebuilt; embedded ABI1 bytes equal both build/runtime and
  the newly built DLL. The legacy optional HallJoy build chained by the plugin
  script failed on its obsolete source path; the current project was then built
  directly with MSBuild successfully. That legacy command is not a successful check.
- New automatic_layout_uap_static_audit covers producer identity semantics,
  selector acceptance, the shared catalog and reason logging. Full final static
  suite: .local/automatic-layout-uap-static-complete.txt.
- Diagnostic Release and simulator builds PASS. No visual run or live analog
  acquisition was performed; the owner's next ordinary launch checks runtime UI.

Backup: .local/backups/automatic-layout-uap-flags-20260914/.
Updated existing EXE: build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe.
SHA256: efbdcb6eda127acf9a2253bb019e9c6c72683c278bb7eaa45081d166600a318b
