# Stabilization test evidence

This directory contains test logs and acceptance evidence for the S01-S21 work
packages. The untouched v3.9.0 baseline is stored separately in `../baseline/`.

## S01 and S02

- `S01_PRECHANGE_GCC_CHECKS_2026-07-30.txt`: pre-change gate.
- `S01_FINAL_GCC_CHECKS_2026-07-30.txt` and
  `S01_FINAL_CLANG_CHECKS_2026-07-30.txt`: final compiler gates.
- `S01_SANITIZERS_GCC_CLANG_2026-07-30.txt`: `-Werror`, ASan, and UBSan.
- `S01_BASELINE_COMPARISON_2026-07-30.txt`: proof that S01 did not enter the
  existing runtime.
- `S01_PROJECT_VALIDATION_2026-07-30.txt`: project/XML/Python checks and the
  recorded Windows-build limitation.
- `S02A_*`: realtime/debug/overlay barrier compiler, sanitizer, hot-path,
  package, and clean-unpack gates.
- `S02B1_SPARKLINK_AVAILABLE_GATE_2026-07-30.txt`: available-device regression;
  MAD68/Hex80 hardware gates remained explicitly deferred.
- `S02B2_*`: SparkLink C++/SEH compiler, sanitizer, hot-path, and package gates.
- `S02B3_*`: Addressed compiler, sanitizer, polling/reader/packet hot-path, and
  package gates.

## V14-11

- `V14-11A_UAP_POLL_PACING_2026-08-01.txt`: deadline pacing and failure
  backoff.
- `V14-11B_UAP_DEVICE_IDENTITY_2026-08-01.txt`: stable identity for identical
  devices.
- `V14-11C_UAP_SNAPSHOT_PINNING_2026-08-01.txt`: bounded registry pins,
  blocked-reader removal, lifetime/coherence stress, and ABI evidence.
- `V14-11D_EXACT_HID_INTERFACE_OWNERSHIP_2026-08-01.txt`: exact-path claims,
  same-VID/PID reorder/collision stress, Soup pre-open behavior, and available
  Irok regression.

## V14-12

- `V14-12A_AULA_WIN60HE_FIRMWARE_PROVEN_2026-08-01.txt`: firmware oracle,
  read-only protocol, parser/end-to-end/session tests, three-toolchain gate,
  official build, and Irok regression; status remained hardware-unvalidated.
- `V14-12B_S06_ADDRESSED_IO_OWNERSHIP_2026-08-01.txt`: reader-owned overlapped
  I/O, bounded outer join, retained-generation injection, unchanged packet core,
  and Irok regression.
- `V14-12C_S07_HEX80_LIFECYCLE_2026-08-01.txt`: worker-owned HID lifetime,
  bounded joins, late-publication guard, timeout containment, unchanged protocol,
  and Irok regression.
- `V14-12D_S07_MAD68_LIFECYCLE_2026-08-01.txt`: persistent-read ownership,
  no post-stop A8, final A9, timeout containment, unchanged protocol core, and
  Irok regression.
- `V14-12E_RELEASE_CYCLES_2026-08-01.txt`: post-build 25-cycle normal-operation
  gate and the remaining 1000-cycle/soak/hardware boundaries.
- `V14-12F_S18_INSTALLER_REMOVAL_2026-08-01.txt`: removal of automatic
  download/elevation/wait, pinned manual ViGEm guidance, and build regression.
- `V14-12G_S20_BUILD_DOCS_2026-08-01.txt`: current build documentation,
  unified Addressed gate, x64-only `/W4` configurations, official build, and
  Irok regression.
- `V14-12H_S21_QUALIFICATION_AUTOMATION_2026-08-01.txt`: persistent lifecycle
  runner, production soak, full gates, and post-build pilot.
- `V14-12I_S21_1000_CYCLES_2026-08-01.txt`: final 1000/1000 lifecycle,
  trace-hash, shutdown-distribution, resource, and state proof.
- `V14-12J_S21_SOAK_SLEEP_PREVENTION_2026-08-01.txt`: Windows power request,
  guaranteed cleanup, and corrected unattended-soak pilot.
- `V14-12K_S21_SPARK_AGE_FIX_2026-08-01.txt`: attended-soak result, SparkLink
  timestamp fix, full gates, and targeted production regression.
