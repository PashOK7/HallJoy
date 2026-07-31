# Результат S02B.1 — exception barriers MAD68/Hex80

Дата: 30 июля 2026 г.  
Исходная точка: `HallJoy_v3_9_0_STABILITY_S02A1_SOUP_PATCH_HOTFIX.zip`.  
Статус: локальная реализация завершена; Windows/MSVC x64 Release gate обязателен перед S02B.2.

## 1. Граница пакета

S02B.1 затрагивает только два native backend worker без вложенных thread generations:

- MAD68 Pro R;
- Hex80.

Не изменены Addressed, Sayo, SparkLink, UAP/C ABI, shutdown timeouts, протоколы, USB framing, polling cadence и ViGEm scheduling.

## 2. Реализация

Для каждого backend:

- normal worker algorithm сохранён в отдельной `uint32_t` body-функции;
- thread entry объявлен `noexcept` и вызывает общий `RunWorkerEntryBarrier`;
- `std::exception` и `...` преобразуются в фиксированный `WorkerExceptionRecord`;
- fault path переводит worker в безопасное опубликованное состояние;
- completion всегда снимает `g_running`;
- причина fault выводится через allocation-free `OutputDebugStringA`;
- новый start сначала join'ит завершившийся joinable `std::thread`, затем освобождает старый wake HANDLE и только после этого создаёт новое поколение.

Последний пункт исключает `std::terminate` при повторном запуске после неожиданного завершения worker.

## 3. Fail-safe состояние

MAD68 при fault:

- закрывает stop loop;
- очищает physical/digital down state;
- сбрасывает stream ownership, publish mode, coverage и analog publications;
- сбрасывает device/firmware/product publications;
- переводит UI state в `Stopped`.

Hex80 при fault:

- закрывает stop loop;
- снимает connected/present;
- сбрасывает detected/active PID и version;
- очищает опубликованные analog values и инициирует общий realtime wake при фактическом изменении.

## 4. Защита от регрессий

- MAD68 worker body: посимвольно равен S02A.1 после удаления нового финального `return 0u`;
- Hex80 worker body: посимвольно равен S02A.1 после удаления нового финального `return 0u`;
- polling loops, timeouts, protocol calls и reconnect waits не менялись;
- новый static audit проверяет barrier wiring, safe-state publication и порядок join-before-thread-replacement;
- Addressed nested reader явно отмечен как deferred, чтобы сложное I/O ownership не было скрыто в этом пакете.

## 5. Проверки

- полный common gate GCC — PASS;
- полный common gate Clang — PASS;
- все portable tests под ASan + UBSan — PASS;
- MSVC `.vcxproj`/`.filters` XML parse — PASS;
- MAD68/Hex80 hot-path comparison — PASS;
- clean package manifest — формируется перед упаковкой;
- Windows/MSVC executable build — ожидается.

Логи находятся в `docs/stability/tests/`.

## 6. Обязательный Windows gate

Перед S02B.2:

1. распаковать архив в новый чистый каталог;
2. выполнить `BUILD.cmd` / clean x64 Release build;
3. подтвердить отсутствие ошибок и новых warnings;
4. запустить HallJoy и проверить ViGEm;
5. проверить подключение/отключение доступной MAD68 или Hex80 клавиатуры;
6. несколько раз закрыть и повторно запустить HallJoy;
7. подтвердить отсутствие краша, зависания и зависших analog values.

Если соответствующего устройства нет, минимальный gate — сборка, запуск, ViGEm, отсутствие crash/hang и неизменная работа доступных устройств; hardware-specific verification останется отмеченной как недоступная.
