# Результат S02B.2 — SparkLink C++/SEH exception boundary

Дата: 30 июля 2026 г.

## Цель

Закрыть выход C++-исключений из SparkLink worker и сделать аварийный выход fail-safe, сохранив существующую SEH-защиту, HID-протокол, polling loop и performance-настройки.

## Изменения production-кода

Изменён только `src/HallJoyProject/HallJoy/backend_sparklink.inc`.

Добавлено:

- общий `RunWorkerEntryBarrier` внутри отдельного `noexcept` C++ entry;
- сохранение фиксированного `WorkerExceptionRecord` без allocation;
- сохранение native SEH code в отдельном atomic;
- единый idempotent `SparkReleasePublishedInput()`;
- обнуление всех опубликованных analog values при любом выходе worker;
- немедленный wake realtime loop при публикации neutral snapshot;
- закрытие `write-capable` publication при fault;
- completion publication `g_sparkWorkerExited`;
- двойная startup-проверка раннего завершения worker до и после `connected=true`.

Последняя проверка закрывает race, при котором worker мог завершиться во время `SparkStart()`, после чего стартующий поток записывал `connected=true` поверх уже опубликованного disconnect.

## Что намеренно не менялось

- SparkLink vendor commands и packet layout;
- device-info/layout discovery;
- row routing и normalization;
- `SparkPollMode` и row limit;
- polling loop, Sleep/SwitchToThread policy и transaction cadence;
- VID/PID claim policy;
- HID I/O implementation;
- `TerminateThread` fallback в `SparkStop()` — остаётся открытым до S08.

Извлечённый polling loop посимвольно совпадает с S02B.1:

```text
SPARK_POLL_LOOP_EQUAL=TRUE
SHA-256=6ff9632ed43a492fc582c6578936c18cba0556b35065cb9b137257eede7c8954
```

## Тесты

Локально пройдено:

- SparkLink targeted static audit;
- все static audits проекта;
- все portable C++ tests;
- GCC common gate;
- Clang common gate;
- ASan + UBSan;
- XML parse MSVC project files;
- hot-path comparison.

## Ограничения

Linux-среда не компилирует Win32 translation unit. Для принятия S02B.2 обязательны:

- чистая MSVC x64 Release сборка;
- запуск на SparkLink;
- hotplug/unplug/reconnect;
- проверка neutral release после unplug;
- несколько циклов restart/shutdown;
- ViGEm smoke.

MAD68/Hex80 hardware gate не требуется для этого пакета, поскольку их production-файлы не изменены. Их S02B.1 device-specific проверка остаётся отложенной до предфинального внешнего пакета.

## Статус

`Implemented; Windows + SparkLink gate pending`.

## Последующая Windows/SparkLink проверка

Пользователь выполнил clean Windows/MSVC build и полный доступный SparkLink smoke. Подтверждено: analog input, ViGEm, Safe/Fast Yield/Max Burst, row limit, hotplug/reconnect, neutral release и повторный startup/shutdown работают без регрессий.

Итоговый статус S02B.2: `Verified on SparkLink`.
