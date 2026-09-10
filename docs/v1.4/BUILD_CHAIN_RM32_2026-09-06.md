# RM-32 build-chain evidence — 2026-09-06

## Isolated UAP overlay compilation

The native UAP plugin was rebuilt from the pinned Sun and Soup revisions with
the `-ExcludeMad68ProRNative` target. The build recreated only its declared
workspace outputs (`build/obj/UAP/native` and `build/bin/UAP/native`) and
completed both ABI DLL targets successfully after verifying all five locked Soup
overlay hashes.

The rebuilt `AnalogueKeyboard.cpp` normalized SHA-256 is
`1AA6F959E8F4658B44E1B0C8E106A616760AA1FA8BD19EE1CF1B66E76CE7781D`.
It matches both the dependency lock and the post-build pinned Soup cache. This
is a real C++ compilation proof for the UAP DrunkDeer/Keychron/NuPhy/Madlions
overlay edits; it is stronger than the earlier source-only audits.

No HallJoy executable, UAP ABI runtime harness, HID session, controller or ROG
diagnostic was started. In particular, `check_private_uap_abi.py` is intentionally
pending because loading the plugin can enumerate physical analogue HID devices.
Thus this record is a plugin compilation/integrity PASS, not a runtime or
hardware-support claim.

## Production C++ compilation finding

The first no-run MSVC rebuild correctly stopped on an existing invalid worker
entry pattern: one function nested C++ `try/catch` within Windows `__try` SEH.
MSVC rejects that combination because it cannot safely combine the two unwinding
models. The entry now follows the existing project pattern: a C++-only noexcept
body handles C++ exceptions, while a minimal OS entry wraps only that body in
SEH. This preserves both fault boundaries without changing supervisor policy.

The subsequent Release/x64 `MAD68ProRNative` rebuild completed successfully.
The only linker warning was the pre-approved third-party `ViGEmClient.lib`
LNK4099 missing-PDB warning. The compilation log includes the ordinary native
families and intentionally excludes `rog_azoth96he_diagnostic_backend.cpp`;
no ROG diagnostic configuration, executable launch, HID access or controller
test was used.
