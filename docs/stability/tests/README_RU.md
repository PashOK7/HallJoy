# Test logs стабилизации

В этот каталог помещаются журналы новых тестов и acceptance gates для кодовых пакетов S01–S21.

Baseline-лог исходной версии хранится отдельно в `../baseline/`, поскольку он относится к неизменённой v3.9.0, а не к исправлению.

## S01

- `S01_PRECHANGE_GCC_CHECKS_2026-07-30.txt` — gate до изменений.
- `S01_FINAL_GCC_CHECKS_2026-07-30.txt` — полный финальный GCC gate.
- `S01_FINAL_CLANG_CHECKS_2026-07-30.txt` — полный финальный Clang gate.
- `S01_SANITIZERS_GCC_CLANG_2026-07-30.txt` — новые тесты с `-Werror`, ASan и UBSan на двух компиляторах.
- `S01_BASELINE_COMPARISON_2026-07-30.txt` — доказательство изоляции от существующего runtime.
- `S01_PROJECT_VALIDATION_2026-07-30.txt` — XML/Python/project checks и ограничение Windows build.

## S02A

- `S02_PRECHANGE_GCC_CHECKS_2026-07-30.txt` — gate до production изменений.
- `S02A_FINAL_GCC_CHECKS_2026-07-30.txt` — финальный полный GCC gate.
- `S02A_FINAL_CLANG_CHECKS_2026-07-30.txt` — финальный полный Clang gate.
- `S02_BARRIER_SANITIZERS_2026-07-30.txt` — новый barrier с `-Werror`, ASan и UBSan на GCC/Clang.
- `S02A_HOT_PATH_COMPARISON_2026-07-30.txt` — byte-equivalence normal processing loops и MSVC item counts.
- `S02A_PACKAGE_VERIFICATION_2026-07-30.txt` — clean-unpack manifest и полный gate первого прохода упаковки.
- `S02B1_SPARKLINK_AVAILABLE_GATE_2026-07-30.txt` — пользовательский regression smoke полного S02B.1 пакета на доступном SparkLink; MAD68/Hex80 gates явно отложены.
- `S02B2_FINAL_GCC_CHECKS_2026-07-30.txt` — полный GCC gate S02B.2.
- `S02B2_FINAL_CLANG_CHECKS_2026-07-30.txt` — полный Clang gate S02B.2.
- `S02B2_SANITIZERS_2026-07-30.txt` — ASan/UBSan portable suite S02B.2.
- `S02B2_HOT_PATH_COMPARISON_2026-07-30.txt` — посимвольное сравнение SparkLink polling loop с S02B.1.
- `S02B2_PACKAGE_VERIFICATION_2026-07-30.txt` — manifest/clean-unpack/final gate текущего архива.


- `S02B3_FINAL_GCC_CHECKS_2026-07-30.txt` — полный GCC gate S02B.3.
- `S02B3_FINAL_CLANG_CHECKS_2026-07-30.txt` — полный Clang gate S02B.3.
- `S02B3_SANITIZERS_2026-07-30.txt` — ASan/UBSan portable suite S02B.3.
- `S02B3_HOT_PATH_COMPARISON_2026-07-30.txt` — token-level сравнение Addressed polling/reader/packet hot path.
- `S02B3_PACKAGE_VERIFICATION_2026-07-30.txt` — manifest/clean-unpack/final gate архива.

## V14-11

- `V14-11A_UAP_POLL_PACING_2026-08-01.txt` — deadline pacing и failure backoff.
- `V14-11B_UAP_DEVICE_IDENTITY_2026-08-01.txt` — стабильная identity одинаковых устройств.
- `V14-11C_UAP_SNAPSHOT_PINNING_2026-08-01.txt` — bounded registry pins,
  blocked-reader removal, lifetime/coherence stress, ABI/build/runtime evidence.
- `V14-11D_EXACT_HID_INTERFACE_OWNERSHIP_2026-08-01.txt` — exact path claims,
  same-VID/PID sibling/reorder/collision stress, Soup pre-open, ABI/build and
  available native Irok regression evidence.

## V14-12

- `V14-12A_AULA_WIN60HE_FIRMWARE_PROVEN_2026-08-01.txt` — независимая
  перепроверка firmware/oracle, exact read-only protocol, parser/end-to-end/
  session-policy tests, три toolchain-gate, официальный build и Irok regression.
  Статус Aula явно ограничен `hardware-unvalidated`.
- `V14-12B_S06_ADDRESSED_IO_OWNERSHIP_2026-08-01.txt` — reader-owned HID/
  `OVERLAPPED` lifetime, bounded outer join, retained-generation timeout
  injection, unchanged packet/polling core и реальный Irok regression.
- `V14-12C_S07_HEX80_LIFECYCLE_2026-08-01.txt` — worker-owned HID lifetime,
  bounded generation join, late-publication guard, retained-generation timeout
  containment, unchanged Hex80 protocol files и реальный Irok regression.
- `V14-12D_S07_MAD68_LIFECYCLE_2026-08-01.txt` — persistent-read ownership,
  no-post-stop A8, mandatory final A9, bounded timeout containment, unchanged
  protocol/command core и реальный Irok regression.
