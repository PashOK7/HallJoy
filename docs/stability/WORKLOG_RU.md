# Журнал работ по стабилизации

## 2026-07-30 — Этап 01: план и baseline

Статус: завершён.

Выполнено:

- распакован исходный архив в отдельную локальную рабочую директорию;
- подтверждён SHA-256 исходного архива;
- повторно выполнен `python tools/run_native_backend_checks.py --require-compiler` — PASS;
- сохранён полный вывод проверок;
- сохранены исходный clean package manifest и SHA-256 всех исходных файлов;
- добавлен полный аудит в дерево документации;
- создан master plan, реестр рисков, журнал решений и этот worklog;
- обновлена навигация документации;
- производственные `.cpp/.h/.inc`, project-файлы и build scripts не изменялись.

Regression gate:

- portable/static baseline: PASS;
- сравнение исходных производственных файлов с baseline SHA-256: без изменений;
- новый archive manifest: должен пройти перед упаковкой.

Следующий пакет:

- `S01 — lifecycle contracts and test scaffolding`;
- без миграции существующих runtime-модулей;
- цель — ввести тестируемые типы состояния/результата и fault-injection seam, не меняя поведение сборки.

## 2026-07-30 — S01: lifecycle contracts and test scaffolding

Статус: завершён.

Выполнено:

- добавлен allocation-free контракт `halljoy::lifecycle` с `WorkerState`, `GenerationId`, `StartResult`, `StopResult` и детализированным `LifecycleError`;
- добавлена компактная state machine с монотонным generation id, запретом restart из `Poisoned` и отдельным подтверждением `Joined`;
- добавлен интерфейс `WorkerPrimitives` для детерминированной подмены monotonic time, stop wait/signal и thread start/join/close;
- добавлены unit tests матрицы всех 49 пар состояний, штатного restart, partial-start failure, stale generation, fault cleanup, timeout/poison и запрещённых операций;
- добавлен fake-primitives test для success/failure/timeout веток;
- добавлен static audit, запрещающий подключение новых контрактов к production translation units на S01;
- portable runner расширен двумя новыми тестами;
- MSVC project содержит новые файлы только как `ClInclude`; список 58 `ClCompile` item остался неизменным.

Regression gate:

- pre-change GCC portable/static gate: PASS;
- final GCC portable/static gate: PASS;
- final Clang portable/static gate: PASS;
- GCC 14.2 и Clang 17, `-Werror`, ASan + UBSan для новых тестов: PASS;
- `.vcxproj` и `.filters` XML parse: PASS;
- изменённых существующих production `.cpp/.h/.inc`: 0;
- production translation units, подключающих S01 headers: 0;
- Windows/MSVC executable build: не выполнялся в текущей Linux-среде.

Риски:

- ни один из 45 production-рисков не переведён в `Verified`; S01 только создаёт проверяемый контракт для следующих миграций.

Следующий пакет:

- `S02 — exception barriers`;
- первый подшаг должен затронуть только realtime/debug/overlay worker wrappers без изменения алгоритмов и shutdown-механики.

## 2026-07-30 — S02A: exception barriers for realtime/debug/overlay

Статус: реализован локально; Windows/MSVC gate ожидается.

Выполнено:

- добавлен общий header-only `worker_exception_barrier.h` с fixed-size `WorkerExceptionRecord`;
- barrier ловит `std::exception` и `...`, вызывает только compile-time `noexcept` fault/completion callbacks и возвращает детерминированный exit code;
- realtime worker разделён на неизменённый algorithm body и `noexcept` OS entry; MMCSS, waitable timer и `timeBeginPeriod` теперь освобождаются RAII даже при исключении;
- при realtime fault run/alive publications закрываются, UI snapshots и last reports обнуляются, подключённым ViGEm targets отправляется нейтральный report best-effort;
- repeated `RealtimeLoop_Start()` больше не сообщает успех для уже faulted/terminated generation;
- diagnostic writer закрывает producer gate при fault; `DebugLog_Init()` не может заменить unreaped HANDLE/file generation, а `DebugLog_Shutdown()` всё равно освобождает ресурсы после fault;
- overlay worker закрывает running/port publications и worker-owned sockets; WSA teardown остаётся owner-side и выполняется ровно один раз; restart блокируется до reap старого thread HANDLE;
- существующий diagnostic logger audit обновлён для разделённого body/wrapper;
- добавлены portable unit test и static audit S02A.

Regression gate:

- pre-change common gate: PASS;
- final GCC common gate: PASS;
- final Clang common gate: PASS;
- новый barrier: GCC/Clang `-Werror`, ASan и UBSan — PASS;
- `.vcxproj`/`.filters` XML parse — PASS;
- realtime scheduling/tick loop, logger processing loop и overlay accept/client loop byte-equal относительно S01;
- `ClCompile` item: 58 до и 58 после; добавлен только один `ClInclude`;
- Windows/MSVC executable build и hardware/ViGEm smoke test: не выполнялись в текущей Linux-среде.

Риски:

- `HJ-AUD-P1-004`: `Partial`; core workers реализованы, native backend workers и UAP/C ABI остаются открытыми.
- `TerminateThread` не удалялся в S02A; это отдельные S04/S05/S08 и не маскируется как исправленное.

Следующий шаг:

- обязательный Windows x64 Release build/smoke gate для S02A;
- после его PASS — S02B, exception barriers для native backend worker entries по одному backend-кластеру.


## 2026-07-30 — S02A.1: deterministic Soup patch build blocker

Статус: локально исправлен; повторный Windows clean build ожидается.

Входное доказательство:

- пользовательский bootstrap Sun: PASS, 0 warnings, 0 errors;
- fresh checkout Soup revision `b02796b0b20276277c8a4b4d3759643eeab43ff7`: PASS;
- patch application: FAIL на внутренней проверке pre-open exclusion до plugin compilation.

Корневая причина:

- `$preOpenMarker` требовался post-patch validation;
- `$preOpenBlock` не эмитил этот marker в генерируемый `hwHid.cpp`;
- прежние static audits проверяли marker во всём `.ps1`, поэтому не обнаруживали рассогласование generator/validator.

Выполнено:

- marker добавлен непосредственно в `$preOpenBlock` перед `CreateFileW`;
- validator переведён на повторное использование `$preOpenMarker`;
- добавлен `plugin_preopen_patch_contract_audit.py`, анализирующий содержимое генерируемого here-string и порядок относительно CreateFileW;
- HallJoy runtime и MSVC project files не изменены.

Regression gate:

- новый audit на исправленном script: PASS;
- common GCC gate: PASS;
- common Clang gate: PASS;
- XML parse: PASS;
- runtime source diff против S02A: 0;
- фактический Windows PowerShell/MSVC build: ожидается.

Следующий шаг:

- повторить clean Windows build из S02A.1;
- после полного build + smoke PASS вернуться к S02B.


## 2026-07-30 — S02A.1 Windows gate

Статус: завершён.

Пользователь выполнил чистую Windows-сборку исправленного архива и подтвердил:

- `BUILD.cmd` завершился успешно;
- HallJoy запускается;
- ViGEm работает;
- overlay включается/выключается и не мешает shutdown;
- крашей, зависаний и заметных регрессий нет.

Build blocker `HJ-DISC-P1-001` переведён в `Verified`.

## 2026-07-30 — S02B.1: MAD68/Hex80 native worker exception barriers

Статус: реализован локально; Windows/MSVC gate ожидается.

Выполнено:

- MAD68 и Hex80 worker algorithms вынесены в отдельные `uint32_t` body-функции;
- thread entries объявлены `noexcept` и проходят через общий allocation-free exception barrier;
- fault callbacks закрывают loop, сбрасывают device/connected/native publications и очищают analog values;
- completion callbacks всегда снимают `g_running`;
- повторный start reap'ит завершившийся joinable `std::thread` до присваивания нового, исключая `std::terminate` после worker fault;
- fault record очищается только после join предыдущего поколения;
- добавлен отдельный static audit S02B.1;
- Addressed, Sayo и SparkLink намеренно не изменялись.

Regression gate:

- GCC common gate: PASS;
- Clang common gate: PASS;
- ASan + UBSan для всех portable tests: PASS;
- MAD68/Hex80 worker bodies относительно S02A.1: BODY_EQUAL=TRUE;
- MSVC project XML parse: PASS;
- Windows/MSVC executable build: ожидается.

Следующий шаг:

- чистая Windows x64 Release сборка S02B.1 и smoke test MAD68/Hex80/ViGEm;
- после PASS — S02B.2 для Addressed reader/main worker с отдельным ownership-safe exceptional cleanup.

## 2026-07-30 — корректировка hardware validation strategy

Статус: принято.

Фактическая доступность устройств:

- SparkLink доступен владельцу проекта и используется как постоянный hardware regression gate;
- MAD68 и Hex80 будут проверены владельцами устройств на едином предфинальном архиве;
- до device-specific gate изменения MAD68/Hex80 не получают статус `Verified`;
- Addressed/Sayo также требуют hardware validation до финального релиза.

Добавлен `VALIDATION_MATRIX_RU.md` с V0–V3 gates и правилами продолжения boundary-only изменений при недоступном устройстве.

S02B.1: пользователь подтвердил корректную работу полного пакета на доступном SparkLink. Это фиксируется только как regression smoke пакета, а не как hardware verification MAD68/Hex80.

## 2026-07-30 — S02B.2: SparkLink C++/SEH exception boundary

Статус: реализован локально; Windows/MSVC + SparkLink gate ожидается.

Выполнено:

- SparkLink worker algorithm оставлен в `SparkPollThreadProcImpl`;
- добавлен отдельный `noexcept` C++ entry через общий `RunWorkerEntryBarrier`;
- внешний SEH wrapper сохранён;
- C++/SEH fault paths публикуют neutral analog state и закрывают write-capable/connected publications;
- добавлена allocation-free диагностика и отдельное сохранение SEH code;
- completion публикует `g_sparkWorkerExited`;
- `SparkStart()` дважды проверяет ранний exit worker, чтобы не опубликовать dead generation как connected;
- `TerminateThread` не изменён и остаётся S08.

Regression gate:

- GCC common gate: PASS;
- Clang common gate: PASS;
- ASan + UBSan: PASS;
- SparkLink targeted static audit: PASS;
- Spark polling loop equality: PASS;
- Windows/MSVC + SparkLink hardware gate: ожидается.


## 2026-07-30 — S02B.2 Windows + SparkLink gate

Статус: завершён.

Пользователь подтвердил clean build и идеальную работу SparkLink: analog input, ViGEm, polling modes, row limit, hotplug/reconnect и shutdown без регрессий. S02B.2 переведён в `Verified on SparkLink`.

## 2026-07-30 — S02B.3: Addressed main/reader exception boundaries

Статус: реализован локально; Addressed device gate deferred; Windows + SparkLink regression gate ожидается.

Выполнено:

- main Addressed worker вынесен в `uint32_t` body и запускается через `noexcept` common barrier;
- вложенный HID reader получил отдельную exception boundary;
- reader HANDLE зарегистрирован в RAII scope и освобождается при unwinding;
- session owner защищает thread construction и всегда join'ит reader до rethrow;
- exceptional cleanup очищает stack-backed scheduler и pending response token;
- fault paths публикуют neutral analog state;
- repeated start reap'ит завершившееся main generation;
- startup отклоняет worker, завершившийся до возврата Start.

Regression gate:

- targeted Addressed static audit: PASS;
- GCC/Clang common gates: PASS;
- ASan + UBSan: PASS;
- session polling loop token equality: PASS;
- reader I/O loop token equality: PASS;
- packet constructors token equality: PASS.

Ограничение: Addressed hardware отсутствует; `ForceCloseReaderHandle` и overlapped ownership остаются S06.

## 2026-07-30 — S02B.3 ручной Windows + SparkLink regression smoke

Статус: пройден как пользовательское наблюдение без машинного evidence.

Пользователь сообщил, что clean build, запуск, SparkLink, ViGEm и общий runtime работают без видимых регрессий. Результат зафиксирован, но после принятия новой политики S02V1 не используется как достаточное доказательство для будущих пакетов.

## 2026-07-30 — S02V1: bounded machine-verifiable stability trace

Статус: локальный V0 завершён; Windows/MSVC + SparkLink evidence bundle ожидается.

Выполнено:

- добавлен compile-time `HALLJOY_STABILITY_TRACE`;
- current/previous traces ограничены 1 МиБ;
- runtime append переведён на preallocated memory mapping без `WriteFile` и flush в worker path;
- sequence назначается под writer-lock и совпадает с порядком строк;
- overflow формирует структурированный `trace.capped` и отключает trace;
- instrumented main/realtime/overlay/backend/ViGEm/Spark/MAD68/Hex80/Addressed boundaries;
- Spark generation публикует итоговые route/input counters;
- изменения poll mode/row limit фиксируются только как settings transitions;
- добавлены deterministic analyzer и synthetic PASS/WARN/FAIL tests;
- добавлен collector, запрещающий bundle до закрытия процесса;
- build/output получает analyzer, collector и точный тестовый сценарий.

Локальные проверки:

- GCC common gate: PASS;
- Clang common gate: PASS;
- static trace audit: PASS;
- Python syntax: PASS;
- MSVC project XML: PASS;
- GCC/Clang ASan + UBSan с `-Werror`: PASS; clean-unpack package gate выполняется при финальной упаковке.

