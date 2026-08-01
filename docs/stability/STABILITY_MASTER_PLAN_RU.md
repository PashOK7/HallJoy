# Master plan: идеальная стабильность и измеряемая производительность HallJoy

Дата начала: 30 июля 2026 г.  
Исходная версия: HallJoy Universal Analog Final v3.9.0.  
Режим работы: локально, без GitHub, небольшими архивируемыми пакетами.

## 1. Цель

Сохранить весь существующий функционал и протокольную совместимость, одновременно устранив известные причины крашей, зависаний, повреждения состояния, тихой потери настроек и непредсказуемой нагрузки CPU/USB.

«Исправлено» означает не только изменение исходника. Для каждой проблемы нужны:

1. воспроизводимый контракт или тест;
2. минимальная реализация;
3. regression gate;
4. документированный результат;
5. отсутствие ложного сообщения об успехе;
6. архив этапа с manifest и контрольными суммами.

## 2. Жёсткие правила процесса

- Один пакет меняет одну связанную область и должен оставаться обозримым.
- Сначала characterization/failure test, затем production change.
- Нельзя смешивать безопасность lifecycle, UI-рефакторинг и оптимизацию в одном пакете.
- Нельзя менять протокольные команды, packet layout, normalization или routing без отдельного evidence document.
- Нельзя заменять зависание принудительным уничтожением потока.
- Тайм-аут, исключение, partial start и partial save должны иметь явный статус ошибки.
- После каждого пакета обновляются `WORKLOG_RU.md`, `RISK_REGISTER_RU.md`, `DECISIONS_RU.md` при необходимости и package manifest.
- Каждый пакет передаётся отдельным архивом; предыдущий архив остаётся точкой отката.

## 3. Универсальный regression gate

Каждый пакет обязан пройти всё применимое:

### Gate L — локальный переносимый

```text
python tools/run_native_backend_checks.py --require-compiler
```

Дополнительно для затронутых pure-компонентов:

- C++20 `-Wall -Wextra -pedantic`;
- ASan + UBSan;
- новые unit/characterization tests;
- XML parse `.vcxproj/.filters` после изменения project files;
- Python syntax checks после изменения scripts.

### Gate W — Windows build/static

- чистая MSVC Release x64 сборка;
- `/W4` для новых/изменённых translation units без новых предупреждений;
- `/analyze` для затронутой области, когда применимо;
- отсутствие x86/x64 library mismatch.

### Gate F — failure/lifecycle

- отказ каждого создаваемого ресурса на N-м вызове;
- stop до start, повторный stop, partial start, timeout и fault;
- отсутствие double close/unmap;
- restart только после подтверждённого завершения;
- нулевые/безопасные опубликованные значения при fault.

### Gate H — hardware/driver

Для backend/ViGEm/UAP изменений:

- cold start, hotplug, unplug при pending I/O, reconnect;
- sleep/resume и application shutdown;
- два одинаковых устройства;
- native + UAP одновременно;
- измерение CPU, latency и USB transaction rate.

Фактическая доступность устройств и правила отложенной проверки зафиксированы в `VALIDATION_MATRIX_RU.md`. SparkLink используется как непрерывный hardware gate. MAD68/Hex80 проходят обязательный внешний device-owner gate на предфинальном архиве; до него boundary-only изменения остаются `Implemented`, а не `Verified`.

### Gate P — persistence/security

Для записи файлов/IPC/installer:

- read-only directory, disk full/quota, interrupted write;
- corrupted temp/input;
- reserved/Unicode/long names;
- ACL/ownership и precreation проверки;
- отсутствие UI freeze на внешней операции.

## 4. Порядок небольших пакетов

### S00 — Plan and immutable baseline

**Статус:** завершён этим архивом.

Только документация и доказательства baseline. Runtime не меняется.

### S01 — Lifecycle contracts and test scaffolding

**Статус:** завершён в архиве `S01_lifecycle-contracts`.

**Цель:** создать общие типы без миграции runtime.

Добавить:

- `WorkerState`: `Stopped`, `Starting`, `Running`, `StopRequested`, `Joined`, `Faulted`, `Poisoned`;
- `StartResult` и `StopResult` с подробной ошибкой;
- generation id;
- компактный тестируемый state machine;
- abstraction seam для wait/time/thread primitives;
- unit tests всех допустимых и запрещённых переходов.

**Запрещено в S01:** менять существующие backend start/stop callbacks.

**Acceptance:** новый код полностью покрыт переносимыми тестами; существующие binary paths не используют его и ведут себя byte-for-byte эквивалентно.

### S02 — Exception barriers

**Статус:** S02A и build-hotfix S02A.1 прошли Windows clean build/smoke gate. S02B.1 добавил exception barriers MAD68/Hex80: локальные gates и доступный SparkLink regression smoke пройдены, device-specific gates отложены до предфинального архива. S02B.2 SparkLink C++/SEH boundary прошёл Windows/MSVC и реальный SparkLink hardware gate. S02B.3 добавил отдельные exception boundaries main/reader для Addressed; локальные gates и ручной общий SparkLink regression smoke пройдены, Addressed device gate отложен. V14-06F закрыл Sayo C++/SEH reader boundary локальными MSVC и simulator fault-injection gates; Sayo hardware gate отложен. S02V1 вводит временную машинно-проверяемую evidence trace, чтобы последующие runtime-gates не принимались только по устному результату. Открытым подэтапом остаётся UAP/C ABI.

**Цель:** исключение в worker или C ABI не завершает весь процесс.

По одному небольшому коммит-пакету для:

1. realtime/debug/overlay workers;
2. native backend workers;
3. UAP workers и exports.

Wrapper — `noexcept`, ловит `std::exception` и `...`, записывает fault, сбрасывает published values и сигналит completion. В hot path исключения не используются как control flow.

### S02V1 — Временная evidence trace для каждого runtime-пакета

**Статус:** локально реализован; Windows/MSVC + SparkLink log evidence ожидается.

Перед продолжением runtime-миграций вводится повторяемый цикл:

1. инструментировать только границы текущего изменения;
2. выполнить формализованный hardware-сценарий;
3. собрать bounded current/previous trace;
4. получить детерминированный `PASS/WARN/FAIL`;
5. сохранить SHA-256 и отчёт;
6. удалить уже отработавшие временные события;
7. перейти к следующей функции с новым stage marker.

Трассировка запрещена в per-poll/per-key path. Runtime append не выполняет файловый I/O: используется заранее выделенный memory-mapped buffer, а flush выполняется один раз после остановки worker'ов. Перед финальным релизом temporary trace и build flag удаляются.

### S03 — Truthful stop contract and registry

Перевести descriptor contract на реальный `StopResult`. Registry не сбрасывает started-state при `TimedOut/Faulted`; backend становится `Poisoned`, restart блокируется и UI/diagnostics получает точную причину.

Сначала fake backend tests, затем один реальный минимальный backend как pilot.

Статус v1.4: `Verified` в V14-05. Production registry и все native descriptor callbacks переведены на generation-scoped `StopResult`; simulator служит воспроизводимым runtime pilot, а hardware-specific cooperative shutdown остаётся в S04-S08.

### S04 — Realtime loop cooperative shutdown

Удалить `TerminateThread` только из realtime loop. Stop event должен прерывать ожидание; cleanup MMCSS/timer/time period выполняется самим worker. ViGEm call пока не переносится — это отдельный пакет.

Статус v1.4: `Verified` локально в V14-06A. Timeout не закрывает HANDLE и переводит generation в `Poisoned`; watchdog restart и backend teardown блокируются. Зависший ViGEm call остаётся отдельным S12.

### S05 — Diagnostic logger cooperative shutdown

Закрыть producer gate, разбудить writer, bounded drain, flush на writer-потоке, затем completion. При timeout не уничтожать объекты, которые worker ещё может использовать.

Статус v1.4: `Verified` локально в V14-06B. Writer использует общий generation-scoped lifecycle и сериализованный start/stop. Timeout сохраняет HANDLE, event, file и очередь, переводит generation в `Poisoned`, блокирует restart и завершает процесс без CRT cleanup. Simulator-only fault injection подтверждает этот путь воспроизводимо; production Release по-прежнему не включает обычный debug log.

### S05A — Overlay server cooperative shutdown

Остановить приём новых соединений, разбудить `accept`, выполнить `shutdown` активного client socket и подтвердить завершение worker до освобождения HANDLE и WSA ownership. HTTP protocol и однопоточная модель обслуживания остаются отдельным S16.

Статус v1.4: `Verified` локально в V14-06C. Start/stop сериализованы одним generation-scoped lifecycle. Timeout сохраняет thread HANDLE, WSA и доступные worker-owned socket resources, переводит generation в `Poisoned`, блокирует restart и запрещает зависимый teardown приложения. Simulator подтверждает loopback `/state`, штатный join и отдельный timeout containment.

### S06 — Addressed overlapped I/O ownership

Reader generation владеет HID HANDLE, event, buffer и `OVERLAPPED`. Stop инициирует cancellation, а reader reap'ит completion до выхода. Запрещён cross-thread close активного I/O.

Статус v1.4: `Implemented / Addressed hardware gate deferred` в V14-12B. HID
HANDLE теперь закрывает только reader после terminal completion; внешний stop
ожидает native main-worker HANDLE не более трёх секунд и при timeout сохраняет
всё поколение, возвращает точный `StopResult` и блокирует зависимый teardown.
Simulator-only зависание прошло process-containment gate. Packet constructors и
session polling core посимвольно сохранены; реальный Irok regression пройден.

### S07 — MAD68 and Hex80 lifecycle migration

По одному backend на архив:

- immutable generation context;
- cooperative stop;
- truthful result;
- no unconditional join after timeout;
- restart only after joined generation;
- неизменные parser/command semantics.

Статус v1.4: Hex80 реализован в V14-12C как отдельный пакет со статусом
`Implemented / Hex80 hardware gate deferred`. Worker переведён на waitable
`_beginthreadex`, stop сначала публикует cooperative stop, будит event и вызывает
`CancelIoEx`, затем ждёт поколение не более трёх секунд. При timeout thread/event/
active-HID ownership сохраняется, registry получает точный `StopResult`, блокирует
restart и переводит приложение в process containment. Протокольные файлы,
command builders, parser и GET-only routing не менялись. Simulator timeout gate,
полный repository gate, MSVC `/W4 /WX`, официальный build и реальный Irok
regression пройдены.

MAD68 реализован отдельным V14-12D со статусом `Implemented / MAD68 hardware
gate deferred`. Persistent overlapped read отменяет owner, но reap и закрытие
всех session HANDLE остаются в worker. После stop A8 запрещён, а финальный
worker-owned A9 сохранён. Внешний join ограничен тремя секундами, timeout
сохраняет поколение и выбирает process containment. Protocol `.cpp/.h`, command
send transports, A8/A9 strategy, restore path и decoder не менялись. Simulator,
41 repository audit, 26 portable tests, MSVC `/W4 /WX`, официальный build и Irok
regression пройдены. Кодовая часть S07 завершена; device-owner gates остаются
release qualification.

### S08 — Spark and Sayo lifecycle migration

Отдельно Spark и Sayo. SparkLink C++/SEH exception boundary и early-exit startup publication закрываются в S02B.2, но `TerminateThread` и Win32 HANDLE ownership остаются открытыми до S08. Удалить принудительное завершение, сохранить protocol discovery и claim policy. После стабилизации вынести `.inc` implementation в отдельные translation units без логических изменений.

Статус v1.4: `Verified` локально в V14-06D-F. SparkLink внутренние hotplug-поколения сериализованы, stop event и `CancelIoEx` предшествуют bounded join, HANDLE освобождаются только после completion, а join/lock timeout блокирует restart и зависимый teardown. Protocol discovery, claim policy, commands и polling не менялись. Simulator fault injection пройден; реальный S02B.2 gate предшествует этому lifecycle-only diff, поэтому повторный device regression остаётся release qualification.

Sayo shutdown и exception containment `Verified` локально в V14-06E/F: reader group имеет одно внутреннее поколение и общий трёхсекундный deadline вместо последовательных ожиданий до 24 секунд. Shared stop event и `CancelIoEx` будят все readers; HANDLE освобождаются только после group completion. Timeout сохраняет всю группу, блокирует restart и зависимый teardown. C++ barrier и внешний SEH wrapper нейтрализуют input, останавливают группу, сохраняют per-reader fault и публикуют completion; startup отклоняет early exit/fault. Реального Sayo hardware gate нет, поэтому совместимость устройства и post-change reconnect остаются release qualification.

### S09 — Analog host generation safety

Ввести immutable `ClientGeneration`, shared ownership до confirmed completion, запрет reinitialize после timeout, корректный partial-start rollback. Старое поколение не читает mutable поля нового.

Статус v1.4: `Verified` локально в V14-07A для parent-side ownership. Snapshot bridge и supervisor образуют одно generation; thread/IPC/event/job HANDLE освобождаются только после общего confirmed join. Partial-start supervisor failure сначала останавливает и join'ит уже созданный bridge. После двух ограниченных shutdown-фаз незавершённый join сохраняет ресурсы, переводит generation в `Poisoned`, запрещает restart и распространяет failure до process-level containment. Simulator partial-start/timeout scenarios, normal scenario, portable/static gates и production MSVC build прошли. UAP worker/C ABI/unload относятся к S10 и этим статусом не закрываются.

### S10 — UAP plugin ABI and unload safety

- exception barrier на каждом export;
- RAII locks;
- null/buffer validation;
- truthful initialized state;
- unload без удержания global devices mutex;
- bounded graceful child shutdown, после которого parent может завершить изолированный job, не повреждая основной процесс.

### S11 — Startup transaction and wakeup correctness

- initial startup проверяет каждый phase;
- rollback выполняется в обратном порядке;
- realtime start failure не игнорируется;
- device-change wakeup получает sequence/counter semantics без lost reset race.

### S12 — ViGEm output isolation

Разделить producer и driver-facing consumer:

```text
Input snapshot -> curves/SOCD -> latest desired report -> ViGEm output worker
```

Latest-wins/coalescing, bounded reconnect policy, telemetry driver fault. Перед переносом — characterization test report sequence. После — latency/CPU comparison.

### S13 — Transactional persistence foundation

Создать единый `SettingsStore`/atomic file writer:

1. unique temp в той же директории;
2. проверка каждой записи;
3. flush;
4. schema parse/read-back;
5. atomic replace;
6. rich error;
7. UI сообщает отказ.

Сначала unit tests на mock filesystem, затем по одному типу данных: settings, overlay settings, profiles, layouts, curves.

### S14 — Writable state location and name normalization

Перенести изменяемые данные в `%LOCALAPPDATA%` с безопасной одноразовой миграцией и резервной копией. Нормализовать Unicode/case/reserved names и запретить path traversal/alias collisions.

### S15 — IPC hardening

- analog host: inherited unnamed handles или проверенные ACL/owner/session;
- shared memory sequence/version validation;
- mouse IPC корректно различает create/open;
- interlocked publication/read contract без обычных volatile reads;
- restart backoff и лимит.

### S16 — Overlay server correctness

Сначала parser tests: fragmented headers, pipelining, limits, overflow, malformed lines. Затем bounded multi-client architecture, stop-aware recv/send, localhost/origin policy и telemetry number validation.

### S17 — UAP polling performance and identity

- adaptive pacing/backoff вместо постоянного zero-sleep spin;
- измерение CPU/USB rate до и после;
- stable identity для одинаковых устройств;
- snapshot export сокращает время удержания global mutex — выполнено в
  V14-11C через bounded `shared_ptr` pins до per-device lock/copy.
- coarse VID:PID arbitration заменена в V14-11D на first-proof-wins ownership
  точного normalized HID interface path; same-VID/PID siblings, native pre-open
  checks и общий Soup hook покрыты production-code gates по D-022/D-024.

Нельзя снижать effective sampling rate без измеренного latency budget.
V14-11 завершён code-level проверками; физические CPU/USB/latency и multi-UAP
наблюдения не заявляются и остаются частью доступной hardware qualification.

### S18 — Installer and supply-chain hardening

- убрать длительные/привилегированные операции с UI thread;
- pin exact versions/commits;
- download в unique temp;
- hash/signature проверка непосредственно перед execute;
- безопасная очистка;
- предпочтительно отделить installer helper от основного runtime.

Статус v1.4: `Verified` в V14-12F более строгим решением — встроенный installer
полностью удалён. HallJoy не скачивает, не запускает и не повышает привилегии
для dependency-файлов и не ждёт installer process. Вместо этого показывается
закреплённая официальная страница ViGEmBus 1.22.0; version/URL/manual-only policy
совпадают между central dependency lock и production constant. Negative static,
portable policy, MSVC `/W4 /WX`, официальный build и normal Irok gates прошли.

### S19 — Modularization without behavior change

Только после стабилизации и characterization tests:

- `backend.cpp` разделить на input aggregation, curves/SOCD, ViGEm manager, mouse processor;
- `keyboard_subpages.cpp` разделить по model/controller/view страниц;
- Spark/Sayo `.inc` заменить `.cpp/.h`;
- telemetry публиковать immutable snapshots.

Один extraction-пакет не должен одновременно менять алгоритм.

### S20 — Build, CI-equivalent local scripts and documentation

- привести `TESTING.md` к реально поставляемым scripts/tests;
- добавить локальный Windows test runner;
- исправить stale version docs;
- W4 baseline и новые предупреждения как ошибки для новых файлов;
- pin Sun dependency;
- проверить x64 library references;
- обновить validation package без завышенных утверждений.

Статус v1.4: `Verified` в V14-12G. Три внутренние инструкции описывают реальные
v1.4 scripts/output/storage, Addressed validator переписан под central catalog и
включён в unified runner, v3.9 validation явно помечен историческим. Фиктивные
Win32/x86-конфигурации удалены: поставляемые ViGEm/UAP artifacts и продукт
x64-only. Debug/Release x64 используют W4 с узким задокументированным baseline
`4100/4127/4324/4505`; официальный build отклоняет остальные warnings и прошёл
с нулём compiler warnings. S20 завершён; следующий этап — S21 qualification.

### S21 — Performance and long-run qualification

После correctness:

- ETW/WPA CPU, wakeups, USB, scheduler latency;
- 8–24 часа streaming/polling;
- 1000 start/stop/reconnect cycles;
- Application Verifier, PageHeap, handle tracing;
- driver reinstall/disconnect during runtime;
- итоговая hardware matrix.

Только после S21 допустимо заявлять повышенную production stability.

Статус v1.4: V14-12E добавил машинно-проверяемый normal-operation runner и
прошёл post-build pilot 25/25 production start/stop cycles на доступном Irok.
Каждый цикл завершился exit 0, без trace ERROR и оставшихся процессов; 11
пользовательских файлов не изменились. Pilot не закрывает обязательные 1000
циклов, 8-24 часа, ручной reconnect/input и недоступную hardware matrix.

## 5. Зависимости

- S01 обязателен до S03 и всех lifecycle migrations.
- S02 может идти после S01 и до миграции конкретного worker.
- S03 обязателен до S04–S10.
- S06 должен завершиться до общего Addressed lifecycle refactor.
- S09 должен завершиться до IPC security S15.
- S12 выполняется после безопасного realtime shutdown S04.
- S13 обязателен до S14.
- S19 выполняется только после S04–S17, чтобы не смешивать рефакторинг с исправлением отказов.
- S21 — финальная qualification, не замена unit/fault tests.

## 6. Формат каждого следующего архива

Имя:

```text
HallJoy_v3_9_0_STABILITY_SXX_<short-name>.zip
```

Внутри обязательно:

- полный проект;
- обновлённый `WORKLOG_RU.md`;
- обновлённый `RISK_REGISTER_RU.md`;
- test logs для пакета;
- package manifest SHA-256;
- список изменённых файлов;
- acceptance result и известные ограничения.

## 7. Условия остановки и отката

Пакет не принимается, если:

- baseline test перестал проходить без заранее объяснённого обновления контракта;
- изменились protocol outputs для прежних vectors;
- timeout выдаётся как success;
- появилась новая forced termination/cross-thread close;
- невозможно показать точный список изменённых файлов;
- производительность улучшилась ценой потери событий или ухудшения shutdown;
- Windows gate обязателен, но не выполнен — в этом случае статус только `implemented, platform validation pending`.

Откат выполняется заменой дерева предыдущим архивом этапа. Миграции данных обязаны иметь собственный reversible backup/rollback path.

## Статус автоматизации S21 — V14-12H

Normal-cycle runner сохраняет checkpoint после каждого цикла, state manifests
и Spark route counters. Добавлен отдельный 8-часовой по умолчанию production
soak с post-warm-up baseline, CSV ресурсов, overlay probes, bounded shutdown и
trace analyzer. Короткие пилоты, unified gate и официальный build пройдены.
Это готовность контура, а не выполнение финальных 1000 циклов, 8–24 часов или
недоступной hardware matrix.

## Статус выполнения S21 — V14-12I

Финальный normal-operation lifecycle gate выполнен: 1000/1000 production
start/one-second-run/`WM_CLOSE` PASS на одном exact EXE hash. Все 1000 trace
SHA-256 перепроверены, LocalAppData manifests идентичны, процессов после циклов
нет. 1000-cycle пункт закрыт; long soak, ручной input/reconnect и hardware matrix
остаются отдельными gates.
