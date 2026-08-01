# Реестр рисков HallJoy v3.9.0

Источник: `AUDIT_V3_9_0_RU.md`. Статусы обновляются после каждого архивируемого пакета.

## Статусы

- `Open` — подтверждённая проблема или риск, production fix ещё не принят.
- `Partial` — часть риска закрыта отдельным малым пакетом; оставшаяся область явно указана.
- `Implemented` — код изменён, но обязательная платформенная/hardware проверка ещё не завершена.
- `Verified` — все обязательные gates пройдены.
- `Deferred` — отложено с явно записанной причиной; не означает исправление.

| ID | Приоритет | Краткое описание | Пакет | Статус | Ключевая проверка |
|---|---|---|---|---|---|
| `HJ-AUD-P1-001` | P1 | `TerminateThread` используется как обычный механизм shutdown | `S04/S05/S08` | Implemented | all ordinary occurrences removed in V14-06A-F; final soak/device qualification remains |
| `HJ-AUD-P1-002` | P1 | «Ограниченный shutdown» трёх native backend'ов всё равно может зависнуть навсегда | `S06/S07` | Implemented | Addressed, Hex80 and MAD68 bounded join/retention/containment gates passed in V14-12B-D; physical device qualification remains |
| `HJ-AUD-P1-003` | P1 | Контракт `stop()` недостоверен | `S03` | Verified | generation-scoped StopResult, poison-on-failure and registry tests passed in V14-05 |
| `HJ-AUD-P1-004` | P1 | Нет верхней границы C++-исключений в ключевых worker-потоках | `S02` | Partial | UAP workers/C ABI verified in V14-07C; only deferred device-owner and final soak gates remain in release qualification |
| `HJ-AUD-P1-005` | P1 | Addressed reader закрывает HID HANDLE из другого потока при активном stack `OVERLAPPED` | `S06` | Implemented | reader-only close, cancel-and-reap ownership audit and simulator containment passed; Addressed hardware gate deferred |
| `HJ-AUD-P1-006` | P1 | Analog host допускает повторную инициализацию поверх не завершившегося поколения потоков | `S09` | Verified | parent generation, bounded group timeout, retained ownership and restart-rejection gates passed in V14-07A |
| `HJ-AUD-P1-007` | P1 | Partial-start cleanup analog host может unmap'нуть память под живым bridge thread | `S09` | Verified | injected supervisor-start failure joined bridge before IPC rollback in V14-07A |
| `HJ-AUD-P1-008` | P1 | Потеря device-change wakeup из-за manual-reset `ResetEvent` после ожидания | `S11` | Verified | durable monotonic wake sequence, notify/wait race tests and production Irok smoke passed in V14-08A |
| `HJ-AUD-P1-009` | P1 | Initial startup игнорирует отказ realtime и зависимых native phases | `S11` | Verified | explicit commit, reverse rollback and realtime/native-phase fault injections passed in V14-08A |
| `HJ-AUD-P1-010` | P1 | Синхронный ViGEm update находится внутри realtime worker | `S12` | Verified | dedicated output owner, latest-state/multi-pad equivalence and bounded 60-second driver-stall containment passed in V14-08B |
| `HJ-AUD-P1-011` | P1 | Mouse IPC неверно определяет, создано ли новое mapping | `S15` | Verified | immediate `CreateFileMappingW` disposition capture, preserve/validate-existing runtime self-test and production init trace passed in V14-10A |
| `HJ-AUD-P1-012` | P1 | Named IPC analog host допускает precreation/spoofing в той же сессии пользователя | `S15` | Verified | named transport removed; explicit inherited-handle list, v10 owner/token schema, child/parent identity checks and invalid-handle restart gate passed in V14-10B |
| `HJ-AUD-P1-013` | P1 | Встроенный dependency installer имеет TOCTOU и неполную проверку цепочки поставки | `S18` | Verified | automatic download/elevation removed; exact official ViGEmBus 1.22.0 manual-only page is lock-pinned and negative primitive audits pass in V14-12F |
| `HJ-AUD-P1-014` | P1 | Dependency installer может навсегда заморозить UI | `S18` | Verified | installer process launch and every wait path removed; pure guidance policy, MSVC W4/WX, official build and normal Irok regression pass in V14-12F |
| `HJ-AUD-P1-015` | P1 | UAP plugin не защищает C ABI от исключений и использует ручные lock/unlock | `S10` | Verified | common C ABI barrier, portable exception/RAII test and no-manual-lock static audit passed in V14-07C |
| `HJ-AUD-P1-016` | P1 | UAP unload выполняет неограниченный join, удерживая глобальный devices mutex | `S10` | Verified | bounded plugin joins occur outside devices mutex; ABI runtime unload and child-process containment gates passed in V14-07C |
| `HJ-AUD-P2-001` | P2 | Overlay server обслуживает только одного клиента синхронно | `S16` | Verified | fixed 16-client worker table, slow/parallel/limit socket gates and active-client shutdown passed in V14-10D |
| `HJ-AUD-P2-002` | P2 | Overlay HTTP parser не реализует framing | `S16` | Verified | incremental fragmentation/pipelining/body framing, strict length/transfer-coding rejection and 8/4/2 KiB socket limit gates passed in V14-10C |
| `HJ-AUD-P2-003` | P2 | Overlay telemetry parser допускает unsigned overflow | `S16` | Verified | exact-key `from_chars`, duplicate/junk/overflow rejection and one-billion bound passed simulator and production socket gates in V14-10C |
| `HJ-AUD-P2-004` | P2 | Overlay endpoint открыт любому origin | `S16` | Verified | 128-bit session cookie, exact loopback origin and hostile/null-origin rejection passed in V14-10D |
| `HJ-AUD-P2-005` | P2 | Mouse IPC читает interlocked-written поля обычными volatile reads | `S15` | Verified | peer-owned attach/heartbeat and schema fields use interlocked reads; static audit and simulator policy self-test passed in V14-10A |
| `HJ-AUD-P2-006` | P2 | UAP poll workers собраны без какого-либо pacing | `S17` | Verified | V14-11A exact production policy/property/sanitizer/build gates passed; no physical USB claim |
| `HJ-AUD-P2-007` | P2 | UAP device identity нестабильна для двух одинаковых устройств | `S17` | Verified | V14-11B path identity passed exhaustive reorder/reconnect stress, golden vectors, GCC/MSVC and ASan+UBSan |
| `HJ-AUD-P2-008` | P2 | `is_initialised()` plugin всегда возвращает `true` | `S10` | Verified | runtime ABI gate proves false before init, true after init and false after bounded unload |
| `HJ-AUD-P2-009` | P2 | `_device_info` не проверяет `buffer == nullptr` | `S10` | Verified | runtime ABI gate proves null device-info and full-buffer arguments return zero without a fault |
| `HJ-AUD-P2-010` | P2 | Snapshot export удерживает глобальный devices mutex при копировании всех значений | `S17` | Verified | V14-11C bounded owner pins, blocked-reader removal, lifetime/coherence stress and sanitizer/build/ABI gates passed |
| `HJ-AUD-P2-011` | P2 | Сохранение `settings.ini` может заменить хороший файл неполным temp | `S13` | Verified | unique same-directory temp, checked flush/readback/replace and five injected failure stages preserve the known-good hash |
| `HJ-AUD-P2-012` | P2 | Overlay settings сохраняются напрямую и всегда сообщают успех | `S13` | Verified | overlay copies the base into a transaction, checks every write/readback and reports exact failure stage |
| `HJ-AUD-P2-013` | P2 | Profile stream не проверяется после flush/close | `S13` | Verified | stream flush/good/close state, schema readback and all-stage bindings fault probe passed |
| `HJ-AUD-P2-014` | P2 | Layout preset очищается и пишется неатомарно без проверки ошибок | `S13` | Verified | checked transaction, schema/exact-entry readback and five injected stages preserve the layout probe hash |
| `HJ-AUD-P2-015` | P2 | Curve preset writer также игнорирует результаты записей | `S13` | Verified | curve preset/state checked writes, exact readback and five injected stages preserve both probe hashes |
| `HJ-AUD-P2-016` | P2 | Большинство вызовов сохранения игнорируют возвращаемую ошибку | `S13` | Verified | central trace/UI reporting covers autosave; profile/layout/curve manual paths retain or roll back state and report partial rename |
| `HJ-AUD-P2-017` | P2 | Writable state хранится рядом с executable | `S14` | Verified | LocalAppData default, explicit writable portable marker, source-preserving transactional migration/replay and five-stage failure gate passed |
| `HJ-AUD-P2-018` | P2 | Имена профилей недостаточно нормализованы | `S14` | Verified | shared NFC/case/reserved-name/length/direct-child policy and Unicode collision tests passed |
| `HJ-AUD-P2-019` | P2 | Публикация curve settings имеет слабый memory-order contract | `S11` | Verified | release generation publication, acquire cache observation and concurrency test passed in V14-08A |
| `HJ-AUD-P2-020` | P2 | Глобальный lifecycle registry не защищён и не кодирует thread affinity | `S03` | Verified | serialized owner-thread registry and wrong-thread tests passed in V14-05 |
| `HJ-AUD-P2-021` | P2 | VID:PID ownership слишком крупнозернистый | `S17` | Verified | V14-11D exact path claims, same-pair siblings, pre-open native/Soup gates, three toolchains, ABI/build and Irok regression passed under D-022/D-024 |
| `HJ-AUD-P3-001` | P3 | TESTING.md описывает отсутствующий тестовый контур | `S20` | Verified | current shipped commands, unified runner and S20 docs audit |
| `HJ-AUD-P3-002` | P3 | Внутренние README/BUILD документы относятся к другим поколениям проекта | `S20` | Verified | v1.4 x64 build/runtime/storage guides and stale-marker audit |
| `HJ-AUD-P3-003` | P3 | Один static audit сам устарел относительно текущей архитектуры | `S20` | Verified | Addressed validator rewritten for catalog/lifecycle and required by unified runner |
| `HJ-AUD-P3-004` | P3 | Validation docs переоценивают lifecycle validation | `S20` | Verified | v3.9 record prominently historical; current matrix owns release claims |
| `HJ-AUD-P3-005` | P3 | Release Win32 конфигурация ссылается на x64 ViGEm library | `S20` | Verified | unsupported Win32/x86 configurations removed; exact x64 library audit/build pass |
| `HJ-AUD-P3-006` | P3 | Проект компилируется только с Warning Level 3 | `S20` | Verified | Debug/Release x64 use W4 with documented narrow baseline; official build has 0 unexpected warnings |
| `HJ-AUD-P3-007` | P3 | Корневой README не перечисляет реальные build dependencies | `S20` | Verified | README dependency list and independent clean-room build pass |
| `HJ-AUD-P3-008` | P3 | Sun build tool берётся с moving branch | `S20` | Verified | exact locked commit, fresh fetch and independent build pass |

## Evidence package S01

S01 добавил общий lifecycle-контракт, generation-aware state machine и fault-injection seam. V14-05 подключил его к production registry/backend callbacks и закрыл `HJ-AUD-P1-003` и `HJ-AUD-P2-020` локальными verified gates. V14-06A-F удалили все ordinary `TerminateThread` и закрыли Sayo reader exception boundary локальными fault-injection gates. V14-07C закрыл UAP worker/C ABI часть. `HJ-AUD-P1-001` остаётся `Implemented`, потому что `Verified` требует финального soak/device qualification; открытая часть `HJ-AUD-P1-004` ограничена отложенными device-owner и финальными soak gates.

## Evidence package S02A

S02A добавил общий allocation-free `RunWorkerEntryBarrier`, portable unit tests и `noexcept` wrappers для realtime, diagnostic writer и overlay worker. При C++-исключении:

- realtime закрывает run/alive publications, освобождает MMCSS/timer/time-period через RAII и публикует нейтральное UI/ViGEm состояние;
- logger немедленно закрывает producer gate и запрещает переинициализацию поверх не освобождённого writer generation;
- overlay сбрасывает running/port, закрывает worker-owned sockets, сохраняет WSA ownership до owner-side reap и блокирует restart до `Stop()`.

Normal scheduling/processing loops realtime, logger и overlay подтверждены побайтно одинаковыми относительно S01. S02A/S02A.1 затем прошли пользовательский clean Windows build и runtime smoke test. Риск остаётся `Partial`, потому что часть native backend workers и UAP exports ещё не покрыта.


## Evidence package S02B.1

S02B.1 намеренно ограничен двумя native backend'ами без вложенных worker-поколений: MAD68 и Hex80.

- OS entry обоих workers теперь `noexcept` и использует общий `RunWorkerEntryBarrier`;
- `std::exception` и неизвестные C++-исключения не выходят за thread boundary;
- fault path закрывает loop, сбрасывает native ownership/connected publications и обнуляет опубликованные analog values;
- completion всегда снимает `g_running`;
- перед заменой `std::thread` владелец reap'ит завершившееся поколение, поэтому restart после fault не вызывает `std::terminate`;
- normal polling bodies подтверждены посимвольно одинаковыми относительно S02A.1, кроме нового типа результата и финального `return 0u`;
- GCC/Clang common gates и ASan/UBSan portable suite прошли.

Статус риска остаётся `Partial`: доступный SparkLink regression smoke полного пакета S02B.1 пройден, но device-specific MAD68/Hex80 gate отложен до предфинального внешнего архива. Это не считается `Verified` для двух изменённых backend'ов.

## Evidence package S02B.2

S02B.2 ограничен SparkLink worker boundary и не меняет HID protocol/hot path.

- существующая SEH-защита сохранена внешним Win32 wrapper;
- C++-исключения проходят через общий allocation-free `RunWorkerEntryBarrier`;
- C++ и SEH fault paths обнуляют analog publications, очищают connected/freshness и будят realtime loop для neutral snapshot;
- fault закрывает write-capable publication и сохраняет fixed record/native SEH code;
- completion публикует `g_sparkWorkerExited`;
- `SparkStart()` проверяет ранний выход worker до и после connected publication, исключая ложный healthy start;
- Spark polling loop подтверждён посимвольно одинаковым относительно S02B.1;
- GCC/Clang common gates и ASan/UBSan portable suite прошли.

Пользовательский Windows/MSVC + реальный SparkLink gate S02B.2 пройден до lifecycle-only V14-06D. V14-06D удалил `TerminateThread` и прошёл deterministic timeout containment; повторный device regression остаётся release qualification, а открытая часть S08 теперь ограничена Sayo.

## Evidence package S02B.3

S02B.3 ограничен Addressed exception containment и не меняет protocol/polling/overlapped hot path.

- main worker и вложенный HID reader получили отдельные `noexcept` barriers;
- reader HANDLE освобождается RAII при stack unwinding;
- owner session всегда join'ит reader до rethrow, поэтому joinable `std::thread` не вызывает `std::terminate`;
- exceptional cleanup очищает stack-backed scheduler pointer, pending response token и analog publication;
- повторный start reap'ит завершившееся main generation до замены `std::thread`;
- session polling loop, reader I/O loop и packet constructors token-identical S02B.2.

Статус риска остаётся `Partial`: Addressed device gate отложен до предфинального внешнего архива; UAP/C ABI ещё не покрыт. Sayo boundary закрыт локально в V14-06F, но его device gate остаётся release qualification. Риск `HJ-AUD-P1-005` не закрывается — cross-thread force-close активного overlapped HANDLE остаётся S06.

Это исторический итог S02B.3. Текущий статус после V14-12B указан в основной
таблице: `HJ-AUD-P1-005` реализован без force-close, а Addressed hardware gate
остаётся отложенным.

## Evidence package S02B.4 / V14-06F

V14-06F добавил Sayo reader C++/SEH boundaries, фиксированную per-reader fault-диагностику, neutral publication, group-stop signalling и completion для normal/exceptional exit. Startup отклоняет early-exited/faulted group. Simulator C++ fault injection, повторный timeout containment, normal scenario, portable gate и production MSVC build прошли. Реального Sayo device gate нет; `HJ-AUD-P1-004` остаётся `Partial` из-за UAP/C ABI и отложенных hardware gates.

## Evidence package S09 / V14-07A

V14-07A ввёл единое parent-side generation для snapshot bridge и supervisor.
Thread/IPC/event/job HANDLEs освобождаются только после confirmed group join.
Injected supervisor-start failure join'ит уже созданный bridge до IPC rollback.
Injected bridge timeout проходит bounded graceful и child-job phases, после
чего сохраняет reachable ownership, poison'ит generation, запрещает restart и
распространяет failure до process-level containment. Оба injected scenarios,
normal simulator, portable/static gates и production MSVC build прошли.

`HJ-AUD-P1-006` и `HJ-AUD-P1-007` закрыты. UAP worker exceptions, C ABI и
unload остаются открытыми в S10/V14-07B/C.

## Правило закрытия

Запись переводится в `Verified` только после выполнения acceptance criteria соответствующего пакета и сохранения test log в `docs/stability/tests/`. Один и тот же кодовый change может закрыть несколько записей, но каждая запись получает отдельное доказательство.

## Сводка

- P1: 3 implemented, 1 partial, 12 verified.
- P2: 21 verified.
- P3: 6 open, 2 verified.
- Всего: 6 open, 3 implemented, 1 partial, 35 verified.

## Дополнительный риск, обнаруженный при Windows gate S02A

| ID | Приоритет | Краткое описание | Пакет | Статус | Ключевая проверка |
|---|---|---|---|---|---|
| `HJ-DISC-P1-001` | P1 build blocker | Fresh Soup patch всегда падал: validator требовал marker, который generator не эмитил | `S02A.1` | Verified | clean Windows `BUILD.cmd` + generated `hwHid.cpp` ordering check |

S02A.1 добавляет marker внутрь генерируемого `$preOpenBlock` и отдельный audit generator/validator contract. Повторная чистая Windows-сборка и runtime smoke test пройдены; build blocker закрыт как `Verified`.

## Evidence infrastructure S02V1

S02V1 не меняет статус ни одного исходного риска и не считается исправлением production-дефекта. Пакет вводит временный доказательный слой для последующей приёмки:

- bounded 1 МиБ memory-mapped event trace;
- запрет disk I/O и flush в worker event paths;
- непрерывный sequence, balanced worker generations и обязательный final shutdown;
- машинный `PASS/WARN/FAIL`;
- проверяемый SparkLink/ViGEm сценарий;
- обязательное удаление специализированных событий после подтверждения пакета.

Ручной SparkLink smoke S02B.3 зафиксирован как положительное наблюдение, но evidence-verified статус для будущих runtime-пакетов требует trace bundle. `HJ-AUD-P1-004` остаётся `Partial`; UAP/C ABI boundaries ещё не завершены, а device-specific MAD68/Hex80/Addressed/Sayo gates отложены.
