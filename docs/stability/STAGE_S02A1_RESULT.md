# S02A.1 result: clean-build UAP/Soup hotfix

- Date: July 30, 2026
- Starting point: `HallJoy_v3_9_0_STABILITY_S02A_EXCEPTION_BARRIERS_CORE.zip`
- Status: Completed; clean Windows build and runtime smoke passed

## Observed failure

The custom Windows build bootstrapped Sun with zero warnings/errors and fetched
the pinned Soup revision `b02796b0b20276277c8a4b4d3759643eeab43ff7`, but
stopped in `Apply-Soup-Madlions-Fix.ps1` before compiling plugin DLLs:

```text
native analogue exclusion is not applied before CreateFileW
```

The S02A runtime code had therefore not yet been built or exercised.

## Root cause

The patcher defined the marker:

```text
HallJoy native analogue pre-open exclusion
```

Post-patch validation required that marker before
`hid.handle = CreateFileW`. However, the generated `$preOpenBlock` inserted into
`hwHid.cpp` did not contain it; the text existed only elsewhere in the
PowerShell script. A clean, previously unpatched Soup tree therefore failed
deterministically. Old static audits produced a false PASS because they searched
the whole script rather than the generated C++ here-string.

## Correction

1. Add the exact marker to the comment inside `$preOpenBlock`.
2. Make post-patch validation reuse `$preOpenMarker` instead of duplicating the
   literal.
3. Add `plugin_preopen_patch_contract_audit.py`.
4. Require the generated block to contain the marker,
   `UAP_EXCLUDE_HALLJOY_NATIVE`, `HALLJOY_UAP_NATIVE_HID_IDS`, a pre-open
   `continue`, and insertion at the beginning of the enumeration-loop body.
5. Simulate final ordering and prove that the marker precedes
   `hid.handle = CreateFileW`.

No HallJoy runtime source, S02A barrier, protocol algorithm, Soup revision, Sun
flag, plugin ABI, project file, timing, polling, or shutdown behavior changed.

## Validation

- original script missing the marker in `$preOpenBlock`: defect reproduced;
- corrected generated block: PASS;
- pre-open contract audit: PASS;
- full GCC and Clang common gates: PASS;
- optimized Python audit run: PASS;
- MSVC project XML parse: PASS;
- `HallJoy.vcxproj` byte-identical to S02A;
- runtime source diff relative to S02A: zero files.

The user then repeated a clean Windows build. `BUILD.cmd`, HallJoy startup,
ViGEm, overlay start/stop, and shutdown all passed without a crash, freeze, or
functional regression. Evidence:
`tests/S02A1_WINDOWS_GATE_2026-07-30.txt`.
