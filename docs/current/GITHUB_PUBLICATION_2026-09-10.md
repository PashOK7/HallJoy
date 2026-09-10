# GitHub publication — 2026-09-10

## Published

HallJoy v1.5.0 is the latest non-prerelease:
https://github.com/PashOK7/HallJoy/releases/tag/v1.5.0.
Tag/source revision: 31470d22db96111095e5625a5707f44bfcd053c3.
Both Linux and full Windows checks PASS:
https://github.com/PashOK7/HallJoy/actions/runs/34511013964.
The uploaded ZIP is the unchanged four-user-tested candidate, not a replacement
CI build. GitHub asset SHA256 matches
6EF62BE7DB0ABBE861D4595E1E9A74163C989B8862D8C625BAEFECA4EF8B7026;
contained EXE SHA256 remains
F6CF016FA3D8B15D80EB2AF83BCAE1D8E16F989FE142EBC92D96CA454232079B.
ZIP and checksum were verified while draft before publication. Tag readback
confirms the CI-passing revision. No older release was changed.

## Publication fixes and evidence

Owner authorized pushing current sources and publishing 1.5.0. First source
commit abd0eeb69a004acb74edb3448ee7882cf0253127 reached main. CI run 34510146142
caught two clean-checkout problems hidden by local working files:

- A historical nested ignore rule excluded three frozen IROK implementation/test
  files required by source audits. Explicit exceptions ship them without enabling
  the frozen backend.
- Windows Git checkout rewrote line endings of byte-hashed manufacturer sources.
  .gitattributes now preserves exact bytes across platforms. Integrity checks
  remain strict; hashes were not weakened or blindly regenerated.

These publication corrections do not change the validated release EXE. A clean
Git checkout, not only the original working tree, must pass checks before release.

Actions uploads now occur only for workflow_dispatch and expire after three days.
Automatic push/PR compilation and tests still run. No paid budget changed and no
old artifact/cache/release deleted. Account billing and other private repository
details are kept in local notes, not in this public release report.

Next CI run 34510415822 passes the corrected source checks but reveals the
ViGEm child transport ABI test was incorrectly scheduled on Linux. It includes
the actual Windows SDK header (pshpack1.h), even with fake device calls. The test
now runs in the mandatory Windows test group; portable protocol/state tests stay
on Linux. No fake replacement SDK, skipped Windows coverage or EXE change.

Run 34510636301 also identifies bounded_ini_numeric as Windows-only: its newer
tests exercise GetPrivateProfileString/temporary files, not just numeric parsing.
Moved that entire test to the same mandatory Windows group, retaining all cases.

The same dependency review classifies key_settings_domain as Windows-linked
(production settings.h uses Win32 types); pure curve_math remains portable.
Windows CI also revealed profile simulator compilation preceded generation of
its embedded UAP resource DLLs. Reordered the official build: generate/validate
runtime first, then compile/run profile tests, then compile the production EXE.
No cached DLL assumption or test omission remains at this boundary.
