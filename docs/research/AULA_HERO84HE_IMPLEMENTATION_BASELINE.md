# AULA HERO84 HE diagnostic implementation baseline

Captured: 2026-09-01, immediately before the first integration edits to shared
HallJoy registration/build/raw-input files.  This is the recovery/integrity
baseline required for the diagnostic branch; unrelated work must not be
reverted to these versions.

| Path | SHA-256 before HERO84 integration |
| --- | --- |
| `src/HallJoyProject/HallJoy/native_analog_routing.h` | `BE04B9F368F95F987740DEB78C12B3EBC4E64E8A26131BD0698A1E30F8ECE69A` |
| `src/HallJoyProject/HallJoy/native_analog_backends.def` | `071BF7C304BE94D2CD2370E2FFEC710058FAF82C8BDFE56DCC225BA4BE1BD05F` |
| `src/HallJoyProject/HallJoy/app.cpp` | `7FFF1B43E45CE3786043BB52C263A036D9ADA84BB0C0F77231328F2DFC774087` |
| `src/HallJoyProject/HallJoy/HallJoy.vcxproj` | `0C73EF65C3F098F122D951421D59F72C5047E942B5DFFEB0C1D841D531C0051B` |
| `tools/build_mchose_ace68_diagnostic.ps1` | `9850CEB2CBB7E617616B1326B84A6C4E7CF0C54028B36AA81B380E376773FEEE` |
| `tools/run_native_backend_checks.py` | `F4B4FE1C62E55212C717105374938BF40DB85A946BD7B887378D5F669D8648A7` |

The dedicated diagnostic additions are isolated behind
`HallJoyAulaHero84HeDiagnostic=true`.  They must not be compiled into a normal
release image, and may not alter existing native backend registration unless
that property is true.
