# Аудит concurrency, lifecycle и ownership HallJoy

Дата: 20 августа 2026 года.

Статус: завершённый статический аудит. Исходный код, тесты и build scripts не
изменялись; сборка и физические тесты не выполнялись.

## Краткий вывод

В HallJoy уже есть хорошие изолированные primitives для lifecycle, generation,
latest-value publication и wake sequence. Но их гарантии не доведены до всех
production call sites. Найдены семь отдельных групп дефектов:

- realtime может вызвать `SetEvent` на ViGEm wake handle одновременно с его
  закрытием и повторным созданием watchdog-потоком;
- UI публикует tracked HID list через обычный массив и atomic count, что не
  защищает уже начавшего читать consumer;
- central native registry может продолжать показывать `Running` после
  фактического завершения или fault worker;
- SparkLink/Sayo hotplug, HID discovery, protocol proof, stop и join выполняются
  прямо из `Backend_Tick`;
- `HidIoOperation` правильно сохраняет память до завершения OVERLAPPED, но после
  timeout может ждать cancellation бесконечно;
- shutdown watchdog создаёт собственные event/thread только в момент shutdown;
  при нехватке ресурсов HallJoy остаётся без обещанного process deadline;
- несколько составных runtime/health/UI состояний публикуются как набор
  независимых atomics, а не как одна generation-coherent snapshot.

Первые два пункта являются прямыми C++ data/resource races. Остальные —
liveness, state-machine и coherent-publication defects. Они не требуют
догадок о поведении конкретной клавиатуры и могут быть исправлены и проверены
без аналогового hardware; физические устройства понадобятся позднее для
регрессии latency, reconnect и протокольного поведения.

Рекомендуемый пакет — `V14-23`, Concurrency/lifecycle/ownership hardening. Он
является release blocker и должен выполняться после появления отрицательных
old-bug oracles из `V14-22`, совместно с `AnalogProviderV2`, а не отдельной
серией локальных atomic-пластырей.

## Граница аудита

Проверены:

- startup, rollback, watchdog и shutdown в `app.cpp` / `main.cpp`;
- realtime lifecycle, wait/wake и fault barriers;
- ViGEm output worker, mailbox, restart watchdog и resource cleanup;
- native catalog lifecycle и все production worker ownership patterns;
- SparkLink/Sayo outer service и внутренние reader generations;
- common OVERLAPPED HID operation helper и все его production consumers;
- analog-host child/supervisor/snapshot-bridge ownership;
- overlay accept/client ownership и stop ordering;
- settings, bindings, curves, layout и UI/telemetry publication;
- существующие lifecycle, join, mailbox, wake, static и process fault tests.

Этот документ фиксирует static proof и требуемые проверки. Он не утверждает,
что конкретная гонка уже наблюдалась у пользователя, и не переименовывает
model/static PASS в runtime evidence.

## Что уже сделано правильно

Эти решения нужно сохранить:

- `WorkerLifecycle` различает `Stopped`, `Starting`, `Running`,
  `StopRequested`, `Joined`, `Faulted` и `Poisoned`, а generation не позволяет
  безопасно переиспользовать незавершённого owner;
- при stop timeout большинство workers сохраняют thread/HID/event resources и
  блокируют restart вместо раннего `CloseHandle` или `TerminateThread`;
- startup имеет reverse-order rollback, shutdown соблюдает dependency order;
- UAP child находится в owned job, его hang/crash имеет process boundary;
- realtime не вызывает ViGEm API напрямую: output worker получает latest
  report через SPSC mailbox;
- mailbox и wake-sequence primitives имеют полезные deterministic tests;
- overlay закрывает client sockets до join и не отдаёт ownership accept loop;
- layout публикуется как pinned immutable `shared_ptr` snapshot;
- per-key `KeySettings` имеет seqlock-like coherent read;
- fault barriers нейтрализуют опубликованные значения и не выпускают C++
  exception за thread entry;
- poisoned shutdown приводит к немедленному process exit, а не к продолжению
  CRT cleanup поверх живого worker.

Проблема не в отсутствии lifecycle-кода. Проблема в том, что state, resources
и published data не всегда принадлежат одной и той же generation.

## CON-P01 — ViGEm wake handle имеет close/use race (P0)

`g_vigemOutputWakeEvent` — обычный глобальный `HANDLE`. `VigemOutput_Wake()`
копирует его без lock/pinning и вызывает `SetEvent`. Эту функцию вызывает
realtime после mailbox publication и neutralization, а также UI/control paths.

Одновременно `Backend_EnsureOutputWorkerRunning()` может на UI watchdog tick
выполнить `VigemOutput_Stop()` и `VigemOutput_Start()`, пока realtime продолжает
работать. Stop после join делает:

```text
CloseHandle(g_vigemOutputWakeEvent)
g_vigemOutputWakeEvent = nullptr
```

Lifecycle mutex защищает Start/Stop друг от друга, но realtime его не берёт.
Возможная последовательность:

1. realtime копирует старый ненулевой handle;
2. watchdog join-ит output worker и закрывает handle;
3. Windows повторно использует числовое значение handle или Start создаёт новый;
4. realtime вызывает `SetEvent` по закрытому/переиспользованному значению.

Это одновременно C++ data race на переменной handle и Win32 resource-lifetime
race. Последствие не ограничивается потерянным wake: переиспользованный event
может сигнализировать не ту generation или другой объект процесса.

`std::atomic<HANDLE>` не является исправлением: reader всё равно может загрузить
handle непосредственно перед `CloseHandle`. Нужен один из ownership-вариантов:

- process-lifetime wake event, который не закрывается при watchdog restart;
- generation resource object, который publisher pin-ит до завершения
  `SetEvent`, а owner закрывает только после исчезновения всех ссылок;
- либо запрет close/recreate, пока существует realtime publisher.

Обязательные проверки:

- old-bug stress, который чередует publish/wake и десятки тысяч output restarts;
- Win32 handle-reuse fault injection;
- доказательство, что ни один stale publisher не может разбудить новую
  generation;
- exact final-EXE output-worker fault/recovery с непрерывным realtime input;
- ThreadSanitizer для portable resource-owner model и Windows runtime stress.

Риск: `HJ-V14-P0-005`.

## CON-P02 — tracked HID list публикуется с настоящей data race (P1)

UI writer использует следующий sentinel protocol:

```text
g_trackedCount = 0
write plain g_trackedList[]
g_trackedCount = newCount
```

Realtime reader сначала загружает `g_trackedCount`, затем читает обычные
`uint16_t` из `g_trackedList`. Обнуление count не отзывает reader, который уже
получил старое ненулевое значение. Он может читать элементы одновременно с
новой записью UI. Acquire/release на count не делает обычный массив безопасным
для такого overlap.

Это C++ undefined behavior и возможный mixed old/new list. Дубликаты и лишняя
UI работа из предыдущего performance-аудита здесь вторичны.

Корневое исправление:

- immutable `TrackedHidSnapshot`, опубликованный одной atomic/pinned ссылкой;
- либо корректный sequence/double-buffer protocol с retry reader;
- один snapshot pin на весь tick и явная deduplication policy.

Нельзя заменять массив на 256 независимых atomics: это уберёт формальную race,
но не даст coherent списка.

Обязательные проверки:

- old-bug writer/reader interleaving с intentional yield между каждым slot;
- invariant: reader видит только целиком старую или целиком новую generation;
- duplicate/empty/256-entry cases;
- production-wired UI layout switching во время realtime tick;
- sanitizer/race-detector gate, не source-token audit.

Риск: `HJ-V14-P1-028`.

## CON-P03 — lifecycle state не привязан к фактической жизни worker (P1)

Изолированный `WorkerLifecycle::MarkFaulted()` существует и тестируется, но в
production worker callbacks не вызывается. Realtime и ViGEm completion меняют
отдельные health atomics; UI watchdog позднее замечает это и делает Stop/Start.

Native registry имеет более серьёзный разрыв:

1. `NativeAnalogBackends_StartPhase()` вызывает descriptor `start()`;
2. при `true` registry немедленно делает `CompleteStart(... Running)`;
3. backend worker при exception/early exit очищает собственный `g_running` и
   значения, но не сообщает registry generation о completion/fault;
4. registry snapshot остаётся `Running` до будущего explicit Stop;
5. новый `BeginStart` отклоняется как already running, а общего native watchdog
   для таких workers нет.

Некоторые backends делают zero-time early-exit probe, Spark/Sayo проверяют
startup publication подробнее, но единого ready acknowledgement нет. У
realtime и ViGEm `CreateThread` сразу сопровождается `ConfirmRunning`; worker
может завершиться между ними, и Start временно вернёт успех.

Нужен единый generation-bound worker contract:

- `Start` завершается только после ready acknowledgement конкретной generation;
- completion/fault callback публикует terminal state в того же owner;
- callback старой generation не меняет новую;
- registry различает `Running`, `Faulted`, `ExitedAwaitingJoin` и `Poisoned`;
- recovery всегда сначала reap-ит старый thread и только затем запускает новый;
- health snapshot выводится из lifecycle owner, а не из второго набора flags.

Обязательные проверки:

- worker exits before ready, immediately after ready и одновременно с Stop;
- C++ fault, SEH fault, normal unexpected return и stale completion;
- registry state и реальный thread handle совпадают на каждом transition;
- автоматическое native recovery без overlapping generations;
- production-wired tests, а не только `WorkerLifecycle` model.

Риск: `HJ-V14-P1-029`.

## CON-P04 — HID lifecycle выполняется внутри realtime tick (P1)

`Backend_Tick()` напрямую вызывает `SparkTickHotplug(nowMs)` и
`SayoTickHotplug(nowMs)`.

При stale connection эти функции синхронно вызывают Stop:

- timed lifecycle lock может ждать до 500 ms;
- Spark join может ждать до 3 s;
- Sayo reader-group join может ждать до 3 s.

При reconnect они синхронно вызывают Start, который выполняет SetupAPI/HID
enumeration, open, capability/device-info queries, Sayo read-only protocol proof,
claim и создание readers. Номинальные per-operation timeouts не превращают всю
операцию в realtime-safe работу.

Следовательно, dedicated ViGEm output worker изолирует driver submission, но
не гарантирует неблокирующий `Backend_Tick`: unplug, stale stream или обычный
двухсекундный discovery cycle могут остановить обработку всех остальных
источников и XUSB state construction.

Корневое исправление:

- hotplug/discovery/session owner работает в provider/control worker или child;
- realtime только pin-ит последний immutable provider snapshot;
- Stop/Start/probe/join никогда не вызываются из tick;
- provider state change публикуется отдельной generation и будит realtime лишь
  при изменении effective values/ownership;
- slow provider не влияет на уже работающие источники и gamepad output.

Обязательные проверки:

- injected 500 ms lock, 3 s join, slow enumeration и slow protocol proof;
- непрерывный второй provider во время Spark/Sayo reconnect;
- tick duration histogram и hard upper bound без HID calls;
- held-input neutralization и reconnect generation ordering;
- final EXE fault run с ViGEm report continuity.

Риск: `HJ-V14-P1-030`.

## CON-P05 — HID timeout не является hard liveness bound (P1)

`HidIoOperation` решает важную memory-safety задачу: после `CancelIoEx` нельзя
уничтожать `OVERLAPPED`, event и caller buffer, пока kernel/driver окончательно
не завершил request. Поэтому `CancelAndDrain()` вызывает
`GetOverlappedResult(..., TRUE)`.

Но `TRUE` означает неограниченное ожидание. Если broken/hostile driver не
завершает cancellation, объявленный 20/25/30 ms operation timeout превращается
в бесконечный wait. Helper используется Addressed, Aula MAX, W669, Spark/Sayo,
DrunkDeer, Hex80, IROK и MAD68 workers. Для Spark/Sayo этот путь дополнительно
может оказаться внутри realtime-initiated lifecycle.

Текущий unit test сознательно требует один blocking drain и тем самым доказывает
memory ownership, но не liveness. Простое добавление второго timeout с выходом
из функции будет use-after-free и запрещено.

Надёжный hard bound требует failure boundary:

- предпочтительно provider process, который можно завершить целиком после
  hard deadline, сохранив безопасность main process;
- допустимая промежуточная модель обязана удерживать heap-owned request
  generation до реального completion и блокировать reuse/close, но сама по
  себе не решает вечный kernel request и утечку;
- main process после provider deadline публикует neutral snapshot и продолжает
  работать с другими providers.

Обязательные проверки:

- fake driver никогда не завершает cancelled request;
- child hard deadline, kill, zero publication, no survivor и restart;
- repeated hang cycles без handle/process leak;
- shutdown во время зависшего read и write;
- реальный compatible hardware regression после введения process boundary.

Риск: `HJ-V14-P1-031`. Он конкретизирует liveness-часть уже открытого
`HJ-V14-P1-023`; закрывать их нужно одной архитектурой native containment.

## CON-P06 — shutdown deadline зависит от ресурсов, выделяемых при shutdown (P1)

`AppShutdownNoThrow()` правильно пытается вооружить process-wide watchdog до
первой cleanup операции. Но его event и thread создаются именно в этот момент.
Если `CreateEventW` или `CreateThread` не сработали из-за resource exhaustion,
код только записывает `shutdown.watchdog.arm_failed` и продолжает все
потенциально зависающие joins/logger/storage operations без process deadline.

Это противоречит комментарию и архитектурному обещанию, что broken HID driver
не потребует Task Manager. Наиболее неблагоприятный момент shutdown одновременно
является моментом, когда программа пытается получить новые kernel resources.

Требуемое состояние:

- watchdog/supervisor provisioned до начала обычной работы либо внешний
  process owner уже существует;
- shutdown только переводит заранее созданный owner в armed state;
- failure заранее создать обязательный containment boundary является явной
  startup failure/degraded-state policy, а не тихой потерей shutdown bound;
- disarm не содержит собственного неограниченного wait без независимого owner.

Обязательные проверки:

- injected failure `CreateEventW` и `CreateThread`;
- resource exhaustion перед WM_CLOSE;
- hang в каждом shutdown stage при недоступном logger/CRT lock;
- final process deadline и zero survivor;
- exact final-EXE gate.

Риск: `HJ-V14-P1-032`.

## CON-P07 — составные snapshots могут быть внутренне противоречивыми (P2)

Несколько областей используют безопасные отдельные atomics, но не публикуют
единое логическое состояние:

- `NativeAnalogBackends_ReadMilli()` последовательно вызывает
  `isConnected()`, `ownsHid()` и `getMilli()`; disconnect/reconnect может
  сменить generation между этими вызовами;
- native telemetry собирает connected, identity, report sizes, counters,
  freshness и fault state из разных моментов;
- settings/curve snapshot читает отдельные поля; profile load меняет live
  configuration множеством setters и generations;
- raw/output/source UI telemetry не имеет одной общей generation;
- Sayo reader completion каждый раз заново считает live readers и затем делает
  `store`. Два simultaneous completion могут записать сначала `0`, а затем
  устаревшее `1`, хотя оба reader уже завершились.

Последний случай — конкретная lost-update logic race. Для остальных результат
не обязан быть C++ UB, но может описывать состояние, которого никогда не было.
Whole-profile publication уже входит в `HJ-V14-P1-015`, common provider
snapshot — в `HJ-V14-P1-022`, UI generation — в `HJ-V14-P2-011`; новый риск не
дублирует их, а объединяет оставшиеся health/liveness pseudo-snapshots.

Требуемое состояние:

- `ProviderSnapshot` содержит values, ownership, connection, identity,
  freshness и source generation одной публикации;
- один tick читает одну pinned generation;
- config profile коммитится одной immutable runtime generation;
- telemetry либо generation-coherent, либо явно маркируется independent
  approximate counters;
- reader-group count выводится монотонно/атомарно из owner state, а не
  конкурентными scan-and-store callbacks.

Обязательные проверки:

- forced disconnect/reconnect между connected/owns/value reads;
- simultaneous Sayo reader completion barrier;
- profile switch в каждой точке curve construction;
- telemetry invariant/mixed-generation tests;
- source generation видна в diagnostics и evidence.

Риск: `HJ-V14-P2-018`.

## Lock-order и UI boundary

В просмотренных основных stop paths не найдено доказанного цикла lock-order:

- owner обычно выставляет stop, сигнализирует/cancel-ит I/O, отпускает
  внутренние resource locks и лишь затем join-ит;
- worker не делает синхронный `SendMessage` в UI; фоновые уведомления используют
  atomics/events, а UI-specific `SendMessage` остаются в UI code;
- analog-host освобождает state locks до bounded worker join;
- overlay cancellation владеет socket slots и сохраняет resources при poison.

Это положительный static result, не доказательство отсутствия deadlock.
Неограниченные waits остаются в Addressed nested reader cleanup, overlay client
reap и HID cancellation drain. На верхнем уровне они сейчас полагаются на
process watchdog; поэтому `CON-P05` и `CON-P06` нельзя закрывать отдельно.

## Архитектурное целевое состояние

```text
pre-provisioned process supervisor
              |
    generation-owned provider child/control worker
              |
      immutable ProviderSnapshot
   values + ownership + freshness + identity
              |
      one pin per realtime tick
              |
    curves / bindings / SOCD / XUSB
              |
      latest-value output mailbox
              |
 generation-owned or process-lifetime wake event
              |
          ViGEm worker
```

Ключевые правила:

1. Resource нельзя закрывать, пока любой publisher способен использовать его.
2. Atomic scalar не заменяет ownership и не создаёт coherent object.
3. `Running` означает ready и живую generation, а не успешный `CreateThread`.
4. Realtime не выполняет enumeration, open, protocol proof, stop или join.
5. Timeout является hard bound только при наличии boundary, которую можно
   уничтожить целиком без освобождения живой request memory.
6. Values, ownership и freshness одного provider читаются одной snapshot.
7. Shutdown containment существует до начала shutdown.

## Порядок реализации V14-23

### V14-23A — отрицательные oracles

- ViGEm wake close/use/reuse stress;
- tracked-list coherent-publication race;
- early-exit/ready/fault registry matrix;
- Spark/Sayo slow lifecycle in tick;
- never-completing cancelled I/O;
- shutdown watchdog allocation failure;
- simultaneous Sayo completion and provider TOCTOU snapshots.

Каждый oracle должен падать на текущем defect до production fix.

### V14-23B — generation-owned resources и truthful lifecycle

- исправить ViGEm event ownership;
- ready/completion callbacks с generation tokens;
- один authoritative lifecycle/health state;
- stale callback rejection и owner-side reap.

### V14-23C — убрать lifecycle из realtime

- вынести Spark/Sayo hotplug/discovery/stop/start;
- перейти на immutable provider snapshots;
- добавить tick duration/zero-HID-call invariant.

### V14-23D — hard process containment

- выполнить native isolation из `V14-21` / `HJ-V14-P1-023`;
- доказать hard I/O deadline, neutralization, restart и survivor behavior;
- pre-provision shutdown supervisor.

### V14-23E — coherent configuration/health publication

- whole-profile immutable generation;
- provider values/ownership/freshness snapshot;
- Sayo reader owner-state correction;
- UI/telemetry generation contracts.

### V14-23F — qualification

- portable deterministic concurrency tests;
- healthy sanitizer/race controls;
- Windows high-contention process tests;
- exact final-EXE fault/recovery/shutdown gates;
- available representative hardware latency/reconnect regression;
- honest `hardware pending` for unavailable families.

## Что не является исправлением

- сделать только `HANDLE` atomic;
- сделать 256 list/value slots отдельными atomics без snapshot generation;
- увеличить timeout;
- выйти из `CancelAndDrain` и уничтожить живой `OVERLAPPED`;
- добавить ещё один `g_running` flag;
- считать UI watchdog подтверждением корректного lifecycle;
- спрятать HID reconnect за более редким вызовом из realtime;
- считать static token audit или green build runtime race proof;
- закрыть риски одним simulator run без old-bug oracle.

## Физическое дополнение 2026-08-21

### CON-P08 — ViGEm thread handle становится невалидным, recovery необратим (P0)

На физическом IROK MG75 Max SparkLink продолжил `279189/279189` успешных
запросов, но ViGEm watchdog увидел `alive=1 fault_kind=0 has_thread=1`, после
чего `WaitForSingleObject(g_vigemOutputThread, 3000)` немедленно вернул
`WAIT_FAILED`, Win32 6 (`ERROR_INVALID_HANDLE`). Worker одновременно завершился
без exception, lifecycle стал `Poisoned`, а 161 последующая попытка recovery
осталась `restart_blocked=1`.

За 235 мс до отказа был снят keyboard low-level hook. Это обязательный сценарий
stress-oracle, но пока только корреляция. Доступный stable 1.4 source имеет тот
же участок output worker построчно; стабильный EXE служит A/B baseline, но не
закрывает latent ownership/interleaving defect.

Исправление требует provenance каждого create/duplicate/wait/close, одного
generation-owner для thread/wake resources, progress acknowledgement и
bounded neutral/rebuild. Увеличение join timeout, проверка только `alive` или
бесконечный retry poisoned generation не являются исправлением.

Риск: `HJ-V14-P0-006`.

### CON-P09 — Sayo lifecycle lock отравляет несвязанный shutdown (P1)

Во втором физическом запуске ViGEm и SparkLink работали 160 секунд; SparkLink
выполнил `421858/421858` запросов. Shutdown всё равно завершился кодом 2 после
`sayo stop.lock_timeout wait_ms=500`, хотя активным route был SparkLink.
Созданный watchdog-файл прямо помечен `exit_watchdog_synthetic` и не является
unhandled exception evidence.

Нужно определить владельца и максимальное время hold mutex, вынести
start/stop/proof из realtime, сделать absent backend no-op по truthful
generation и проверить concurrent hotplug/shutdown с hard process bound.

Риск: `HJ-V14-P1-038`.

Полная privacy-safe выжимка:
`docs/stability/tests/V14_RELEASE_READINESS_IROK_FREEZE_FORENSICS_2026-08-21.txt`.

Обновлённый `V14-23A` обязан добавить invalid/reused thread-handle, hook/profile
toggle и Sayo lifecycle-lock old-bug oracles. `V14-23B/F` обязан доказать
recovery и clean exact-EXE IROK/DrunkDeer shutdown.

## Решение по релизу

Новые P0/P1 являются обязательными блокерами следующей стабильной версии:

- `HJ-V14-P0-005`;
- `HJ-V14-P0-006`;
- `HJ-V14-P1-028` .. `HJ-V14-P1-032`;
- `HJ-V14-P1-038`.

`HJ-V14-P2-018` не является самостоятельным stable-release blocker по
приоритету, но его provider/config части пересекаются с уже блокирующими
`HJ-V14-P1-015` и `HJ-V14-P1-022` и должны исправляться в той же архитектуре.

До выполнения `V14-23` нельзя утверждать, что current thread/resource lifecycle
и realtime path стабильны для всех клавиатур. Этот вывод не отменяет прошлые
PASS: они остаются доказательством своих узких scenarios, но не покрывают
найденные interleavings.
