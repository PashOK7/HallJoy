# Madlions V6 SafeHID diagnostic support

Build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_all.ps1
```

Tester folder:

```text
HallJoyProject\x64\MadlionsDiagnostic\SEND_TO_MADLIONS_TESTER\
```

Run the isolation self-test first:

```powershell
.\HallJoyMadlionsSafeHID.exe --diagnostic-analog-host-crash-test
```

Delete the intentional self-test dump, then perform the 60–120 minute PnP stress
test described in `README_TEST.txt`.

A real V6 pass means:
- no HallJoy termination;
- no stuck analog snapshot;
- no new real `HallJoyAnalogHostCrash_*.txt/.dmp`;
- Steam/controller connect-disconnect does not interrupt normal polling.

The plugin is loaded directly in the isolated child. Wooting SDK is not loaded,
Program Files is not modified, and no UAC request is expected.

`HallJoyDiagnosticCrash.txt` concerns a crash of the parent HallJoy process.
`HallJoyAnalogHostCrash*.txt/.dmp` concerns a child analog-host crash.
