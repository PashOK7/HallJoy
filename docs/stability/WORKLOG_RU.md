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

## 2026-08-01 — S06 / V14-12B: Addressed overlapped I/O ownership

Статус: `Implemented / Addressed hardware gate deferred`.

- удалён `ForceCloseReaderHandle`: внешний поток больше не закрывает HID HANDLE
  при живом stack `OVERLAPPED`;
- reader владеет handle, buffer, event и операцией до `CancelAndDrain`/terminal
  completion, затем сам закрывает handle через RAII;
- позднее успешное завершение read после stop не может повторно опубликовать
  ненулевой input;
- main Addressed worker переведён на waitable `_beginthreadex` HANDLE;
- внешний stop имеет общий трёхсекундный deadline, возвращает точный
  generation-scoped `StopResult`, сохраняет ресурсы при timeout и блокирует
  зависимый teardown через registry poison;
- simulator-only timeout принудительно оставляет worker живым и подтвердил
  process containment с exit code 2.

Проверки: полный gate 39 static + 26 portable PASS; Addressed MSVC `/W4 /WX`
PASS; официальный `BUILD.cmd` PASS (0 ошибок, только разрешённый LNK4099);
packet constructors и session polling core token-identical; 15-секундный Irok
smoke — 57 161 successful routes, balanced shutdown, zero trace ERROR, 11
пользовательских файлов без изменений. Physical Addressed hardware отсутствует.

## 2026-08-01 — S07 / V14-12C: Hex80 bounded lifecycle

Статус: `Implemented / Hex80 hardware gate deferred`.

- Hex80 worker переведён со `std::thread` на waitable `_beginthreadex` HANDLE;
- start/stop сериализованы, повторный start не заменяет незавершённое поколение;
- stop публикует cooperative flag, будит event и запрашивает `CancelIoEx` активной
  session, но HID HANDLE закрывает только владеющий им worker после terminal reap;
- после каждого завершившегося request stop повторно проверяется до decode и
  публикации, поэтому late completion не возвращает ненулевой input;
- внешний join ограничен 3000 ms; timeout сохраняет thread/event/active-HID
  ownership, возвращает точный result, poison'ит registry и требует process
  containment;
- simulator-only зависание подтвердило exit code 2, retained resources,
  restart block и skipped dependent cleanup.

Проверки: 40/40 static audits и 26/26 portable C++ tests PASS; Hex80 MSVC 19.44
`/W4 /WX` PASS; официальный `BUILD.cmd` PASS (0 ошибок, только разрешённый
LNK4099); Hex80 protocol `.cpp/.h` без Git diff; 15-секундный Irok smoke —
57 276/57 276 successful routes, 315 us average / 878 us maximum interval,
balanced shutdown, zero trace ERROR, 11 пользовательских файлов без изменений.
`HallJoy.exe`: 2 271 232 bytes, SHA-256
`540D4EB764FAD57E7431CA320F3E47420D7F0212C37A8D66EFFC1AAD3A6F6FAF`.
Physical Hex80 hardware отсутствует; MAD68 остаётся следующим отдельным S07
пакетом.

## 2026-08-01 — S07 / V14-12D: MAD68 bounded lifecycle

Статус: `Implemented / MAD68 hardware gate deferred`.

- MAD68 worker переведён на waitable `_beginthreadex`; start/stop сериализованы;
- live `Session` регистрирует только persistent overlapped read. Owner вызывает
  `CancelIoEx`, но unregister, terminal reap и закрытие read/write/control HANDLE
  выполняет worker;
- после stop завершившиеся reads отбрасываются до decode/publication, A8 больше
  не может начаться, а прямой финальный A9 после выхода из loop сохранён;
- join ограничен 3000 ms; timeout удерживает поколение, сообщает точный result,
  poison'ит registry и требует process containment;
- simulator-only зависание прошло с ожидаемым exit code 2.

Проверки: 41/41 static audits, 26/26 portable C++ tests и MAD68 MSVC 19.44
`/W4 /WX` PASS; `BUILD.cmd` PASS (0 ошибок, только разрешённый LNK4099).
Protocol `.cpp/.h`, `Session::Send`, interrupt/control transports,
`BestEffortRestore`, `RunStrategy` и `ProcessPayload` сохранены относительно
backup. Irok smoke: 57 247 successful / 1 shutdown cancellation, 250 us average
и 498 us max interval, balanced shutdown, zero trace ERROR, 11 user files без
изменений. `HallJoy.exe`: 2 272 768 bytes, SHA-256
`79F9E2509D56A80E71C17701F8FAB3DD65A39530E2F7F42C3E40E731AB139020`.
Physical MAD68 hardware отсутствует; кодовая часть S07 завершена.

## 2026-08-01 — V14-12E / S21 normal start-stop pilot

Добавлен `tools/run_release_qualification.ps1`: 1-1000 обычных production
циклов без fault injection, pre-existing-process guard, bounded `WM_CLOSE`,
exit/trace/process/state/hash assertions и JSON evidence. Статический контракт
включён в repository gate, runner включён в обязательные build assets.

Pilot 5/5 PASS. После `BUILD.cmd` (0 ошибок, только разрешённый LNK4099) основной
gate 25/25 PASS на `HallJoy.exe`: shutdown min 138 ms, max 326 ms, avg 245.1 ms;
обычный peak 218 HANDLE, один transient 225 с возвратом к 218; ноль оставшихся
процессов после каждого цикла. 11/11 LocalAppData файлов hash-identical. Последний
Irok trace: 1 830 successful routes, одна shutdown-window cancellation, balanced
shutdown, zero ERROR. 42 static audit и 26 portable tests PASS. EXE SHA-256:
`D0CCF7EF1743EDB301EA00FB8615E7AC2F3055E1B9BF612EFF046530E7F814EB`.

Это Verified pilot контура, но не финальные 1000 циклов/8-24 часа и не замена
ручному input/reconnect либо отсутствующим device-owner gates.

## 2026-08-01 — reconciliation статусов risk register

Сопоставлены все 45 общих `HJ-AUD-*` ID в `docs/stability/RISK_REGISTER_RU.md`
и авторитетном release-регистре `docs/v1.4/RISK_REGISTER.md`. Исправлены 23
устаревших `Open`, для которых уже существовали принятые evidence V14-07C,
V14-08A/B, V14-09A-C, V14-10A-C и V14-04. Production-код и статус hardware/
soak gates не менялись. Итог машинного пересчёта: 8 Open, 3 Implemented,
1 Partial, 33 Verified; расхождений между двумя таблицами по 45 ID нет.

## 2026-08-01 — V14-12F / S18 удаление dependency installer

Из `app_deps.cpp` удалены GitHub `latest` resolution, WinHTTP/URLMon download,
predictable temp, Authenticode-before-execute, `runas`, `msiexec` и бесконечный
wait. Новый контракт только показывает manual guidance: exact official
ViGEmBus 1.22.0 URL, установка пользователем и обязательный restart; HallJoy
не заявляет ложный `Installed` и остаётся degraded.

Pure policy test покрывает четыре комбинации ViGEm/private-runtime issues.
Central dependency lock и production constant обязаны совпадать по version,
URL и `manual-only`. Negative installer/static lock audits PASS; 43 static и 27
portable tests PASS; policy и production TU MSVC `/W4 /WX` PASS; `BUILD.cmd`
PASS с 0 ошибок. Post-build Irok 3/3: exit 0, shutdown 112-241 ms, 209 HANDLE,
0 процессов, 11/11 файлов неизменны, 6 309/6 309 routes, trace ERROR 0.
`HallJoy.exe`: 2 206 208 bytes, SHA-256
`6B5A3FB1009C1DB0C1916A3843A411EADA90DFAABFF5AE1D4EE08D2CC90E6C83`.

`HJ-AUD-P1-013` и `HJ-AUD-P1-014` Verified. Modal guidance вручную не открывался,
так как ViGEm установлен; это явно не hardware/device evidence.

## 2026-08-01 — V14-12G / S20 build, local gates и документация

Устаревшие V6/V11 project README/BUILD/TESTING заменены актуальными v1.4
инструкциями. Addressed validator переписан под central catalog, exact-path
ownership и bounded retained-generation shutdown и теперь обязательно запускается
unified runner. V3.9 validation package явно помечен историческим.

Фиктивные Win32/x86-конфигурации удалены: поставляемые ViGEm/UAP artifacts
x64-only. Debug/Release x64 переведены на W4 с узким объяснённым baseline
C4100/C4127/C4324/C4505; содержательные type-conversion warnings исправлены.
Первый прямой Debug build обнаружил старый debug-CRT/release-ViGEm mismatch;
Debug сохраняет symbols/checks/`/Od` и project-local feature macro, но использует
совместимый `/MT` release CRT.
S20 audit фиксирует эти контракты. Старые проверки четырёх конфигураций теперь
сравнивают Synchronization.lib с фактически поддерживаемыми link configs.

Проверки: current Addressed validator, 44/44 static audits, 27/27 portable tests,
официальный Release и прямой Debug|x64 rebuild PASS. Обе MSVC-конфигурации:
0 errors, 0 compiler warnings, только разрешённый внешний LNK4099. Post-build
Irok 3/3: exit 0, shutdown 139-254 ms,
max 209 HANDLE, 0 процессов, 11/11 файлов без изменений, zero trace ERROR;
SparkLink 17 674 successful плюс одна ожидаемая shutdown-window cancellation.
EXE SHA-256:
`01A046A667DA012237E12C597ED84BE531AF20BC6338F55881C1C5197272559A`.

S20 завершён; общий risk count: 0 Open, 3 Implemented, 1 Partial, 41 Verified.
Дальше S21 qualification. Aula может тестироваться асинхронно, но без её
физического результата релиз не утверждается.

## 2026-08-01 — V14-12H / S21 qualification automation

Normal-cycle runner получил checkpoint после каждого завершённого цикла,
terminal failure evidence, before/after manifests пользовательского состояния,
редкий progress output и Spark route counters. Добавлен 8-часовой по умолчанию
production soak: 10-секундный warm-up baseline, CSV HANDLE/thread/GDI/USER/
memory/CPU, повторные overlay responsiveness probes, bounded `WM_CLOSE`, trace
analyzer и фиксированные leak gates. Fault/simulator arguments не используются.

Первый минутный pilot обнаружил ложный gate самого теста: startup allocation
156 -> 211 HANDLE ошибочно считался leak; baseline перенесён после warm-up.
Второй обнаружил PowerShell empty-array -> null при неизменном state; результат
нормализован через `@(...)`. Третий pilot PASS: 53 samples, HANDLE 210 -> 210,
private growth -86 016 bytes, 234 846/234 846 Spark routes, zero trace ERROR,
zero remaining process, 11/11 files unchanged. Analyzer WARN относится только к
ручным input/reconnect/mode сценариям.

Current Addressed validator, 45 static audits и 27 portable tests PASS.
`BUILD.cmd`: 0 errors, 0 compiler warnings, только allowlisted external LNK4099.
`HallJoy.exe`: 2 206 208 bytes, SHA-256
`6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`.
Post-build Irok 3/3 PASS: shutdown 259–325 ms, max 209 HANDLE, 16 229 successful
routes плюс две expected shutdown-window cancellations, zero ERROR, zero
survivors, 11 files unchanged.

Automation Verified; 1000 cycles, 8–24h soak, manual Irok reconnect/input и
external Aula hardware result остаются release gates.

## 2026-08-01 — V14-12I / S21 1000 production cycles

На exact `HallJoy.exe` SHA-256
`6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`
выполнены 1000/1000 обычных production start/one-second-run/bounded `WM_CLOSE`.
Все циклы PASS: exit 0, полный trace без ERROR/capped, zero remaining process,
before/after manifests 11 LocalAppData files идентичны.

Независимая проверка evidence: 1000 trace files, 0 SHA-256 mismatch. Shutdown
min/avg/p50/p95/p99/max = 101/277,1/250/430/1315/2662 ms; 16 циклов >1 s, все
ниже 15-second bound. Max 217 HANDLE, max working set 13 828 096 bytes. Spark
accounting exact: 1 598 879 queries = 1 598 454 ok + 425 одиночных cancellation
в worker shutdown statistics; ни в одном цикле больше одной, ERROR/fault 0.

1000-cycle gate Verified. Остались long soak, manual Irok input/reconnect,
недоступная device-owner matrix и external Aula hardware result.
