# Baseline HallJoy v3.9.0 перед стабилизацией

Дата фиксации: 30 июля 2026 г.

## Исходный объект

- Архив: `HallJoy_UniversalAnalog_v3_9_0_EXTENSIBLE_FINAL_Source(1).zip`
- SHA-256: `1fad157abac5ad0d39b8be13095f2c274d235075eebb4baf50e957a3413ae8a1`
- Производственный код этапом 01 не изменён.
- Исходный package manifest сохранён как `baseline/ORIGINAL_CLEAN_PACKAGE_MANIFEST_V3_9_0.json`.
- SHA-256 всех исходных файлов до добавления документации сохранены в `baseline/ORIGINAL_SOURCE_SHA256SUMS.txt`.

## Воспроизводимая базовая проверка

Команда из корня проекта:

```text
python tools/run_native_backend_checks.py --require-compiler
```

Результат 30 июля 2026 г.: PASS.

Проверены:

- 8 Python static audits;
- `addressed_poll_scheduler_test`;
- `hid_io_operation_lifecycle_test`;
- `vigem_output_scheduler_test`;
- `native_analog_backend_contract_test`;
- `hex80_protocol_test`;
- `mad68pr_protocol_test`.

Полный вывод: `baseline/PORTABLE_CHECKS_2026-07-30.txt`.

## Что baseline не доказывает

Текущая локальная среда не выполняет:

- MSVC/Windows Release x64 link;
- реальные HID, ViGEm, Raw Input, WinHTTP, WinTrust и Job Object сценарии;
- hardware hotplug/reconnect/sleep-resume;
- fault injection Windows API;
- Application Verifier/PageHeap;
- длительные soak-тесты.

Поэтому каждый Windows-зависимый пакет имеет отдельный обязательный Windows gate. До его прохождения проблема может считаться исправленной в коде, но не полностью подтверждённой на платформе.

## Инварианты, которые нельзя нарушать

1. Формат и семантика MAD68, Hex80 и Addressed пакетов не меняются без отдельного протокольного доказательства.
2. Native/UAP arbitration остаётся fail-closed и выполняется до открытия HID в UAP.
3. Digital HID не превращается в фиктивную аналоговую глубину.
4. Multi-device aggregation сохраняет max-семантику по реальным аналоговым источникам.
5. Curves, SOCD, UI telemetry и ViGEm получают те же значения при тех же входах.
6. Никакой этап не может скрывать ошибку остановки, старта или сохранения как успех.
7. Тайм-аут не считается успешным завершением.
8. Производительность оценивается только вместе с correctness и shutdown safety.
