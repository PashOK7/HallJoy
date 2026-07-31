# Результат S02A — exception barriers for realtime/debug/overlay

Дата: 30 июля 2026 г.  
Исходная точка: архив `HallJoy_v3_9_0_STABILITY_S01_LIFECYCLE_CONTRACTS.zip`.  
Статус: локальная реализация и portable gates завершены; Windows/MSVC gate обязателен перед следующим runtime-пакетом.

## 1. Граница пакета

Изменены только верхние C++ exception boundaries трёх worker-кластеров:

1. realtime loop;
2. diagnostic log writer;
3. overlay HTTP worker.

Native protocol workers, UAP plugin exports, cooperative shutdown и удаление `TerminateThread` намеренно не входят в S02A.

## 2. Общий контракт

Добавлен `worker_exception_barrier.h`:

- header-only, без production object/static initializer;
- fixed-size, trivially-copyable `WorkerExceptionRecord`;
- ловит `std::exception` и неизвестные C++ exceptions;
- не выделяет память при формировании fault record;
- fault/completion callbacks проверяются через `std::is_nothrow_invocable_v`;
- completion вызывается ровно один раз и на success, и на fault;
- fault получает отдельный детерминированный thread exit code.

## 3. Realtime worker

- normal scheduling/tick loop не изменён;
- OS entry объявлен `noexcept` и вызывает общий barrier;
- timer, MMCSS registration и multimedia timer period перенесены под `RealtimeThreadResources` с `noexcept` destructor;
- при fault `g_run=false`, alive publication завершается completion callback;
- UI analog/raw snapshots, physical/button publication и last XUSB reports сбрасываются в neutral;
- существующим ViGEm targets выполняется best-effort neutral update без allocation;
- повторный `RealtimeLoop_Start()` не возвращает `true`, если старый generation уже завершился с fault, но owner ещё не вызвал `Stop()`.

## 4. Diagnostic writer

- normal queue/drain/flush algorithm не изменён;
- exception закрывает `g_logReady` producer gate и ставит stop flag;
- producers не продолжают наращивать очередь после смерти writer;
- `DebugLog_Init()` блокирует replacement, пока unreaped writer HANDLE/event/file ещё принадлежат старому поколению;
- `DebugLog_Shutdown()` освобождает эти ресурсы даже если `g_logReady` уже был сброшен fault callback.

## 5. Overlay worker

- normal accept/request loop не изменён;
- exception сбрасывает running/port publications;
- текущий client и listen sockets закрываются worker fault path;
- WSA ownership остаётся у owner и освобождается через атомарный one-shot cleanup в `Stop()` или startup rollback;
- новый start блокируется, пока старый thread HANDLE не reaped.

## 6. Regression evidence

- common static/portable gate, GCC: PASS;
- common static/portable gate, Clang: PASS;
- barrier test с `-Werror`: PASS на GCC и Clang;
- ASan: PASS;
- UBSan: PASS;
- MSVC project XML parse: PASS;
- `ClCompile`: 58 до / 58 после;
- новый production object: 0;
- normal realtime hot loop: byte-equal S01;
- normal logger algorithm: byte-equal S01;
- normal overlay algorithm: byte-equal S01.

## 7. Известные ограничения

- C++ barrier не является защитой от access violation, heap corruption, stack overflow или `std::terminate`;
- `TerminateThread` пока остаётся в timeout fallback и будет удаляться отдельными lifecycle-пакетами;
- startup handshake всё ещё не подтверждает полностью готовое worker generation — это S03/S11;
- native backend worker entries и UAP/C ABI ещё не покрыты;
- Windows SDK/MSVC/ViGEm runtime в Linux-среде не доступны.

## 8. Обязательный пользовательский gate

Перед S02B требуется на Windows:

1. clean x64 Release build в Visual Studio/MSVC;
2. запуск HallJoy и проверка обычного ввода/ViGEm;
3. включение/выключение overlay несколько раз;
4. штатное закрытие приложения с включённым overlay;
5. проверка отсутствия новых предупреждений, crash dialog и зависания при выходе.

Аппаратный fault injection исключения пока не требуется; достаточно compile + smoke, потому что fault paths дополнительно покрыты portable tests и static audit.
