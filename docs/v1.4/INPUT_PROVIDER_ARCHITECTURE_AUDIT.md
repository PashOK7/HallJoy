# Аудит архитектуры input-provider и IPC HallJoy

Дата: 20 августа 2026 года.

Статус: завершённый статический аудит. Исходный код не изменялся, сборка и
физические тесты не выполнялись.

## Краткий вывод

Главная архитектурная проблема HallJoy находится не в самой shared memory и не
в необходимости немедленно переписать все протоколы UAP. Проблема — во
внутреннем контракте между источниками аналога и общим pipeline.

Изолированный UAP host уже получает coherent полный снимок всех клавиш и
устройств. После этого он строит и копирует несколько представлений тех же
данных, а основной процесс снова изображает поклавишный Wooting SDK: для каждого
HID под exclusive lock заново получает полный массив и повторяет это для
известных устройств. Сторонний compatibility API фактически стал внутренним
realtime data plane HallJoy.

Корневое исправление — единый versioned `AnalogProvider` contract:

```text
UAP или native transport
        -> один immutable all-device snapshot
        -> одна публикация source/value generation
        -> одно coherent acquisition в HallJoy
        -> O(1) lookup по полной KeyIdentity
        -> curves / bindings / SOCD / XUSB
```

Shared memory нужно сохранить для высокочастотных latest-value данных, но
разделить data, control и health/telemetry planes. UAP следует оставить первым
provider-adapter и постепенно заменять только те семейства, для которых
собственная реализация доказанно лучше. Native HID I/O в конечной архитектуре
должен получить такую же process containment границу, какая уже есть у UAP.

## Граница и метод аудита

Проверены:

- `analog_host_shared.h` — фиксированный IPC ABI;
- `analog_host_client.cpp` — child polling, snapshot publication, parent bridge
  и Wooting-style функции;
- `backend.cpp` — безопасные SDK wrappers, source fallback и per-HID hot path;
- `native_analog_backend_registry.*` — общий native descriptor и read ABI;
- `app.cpp` и `realtime_loop.cpp` — процессная граница и пробуждение realtime;
- `settings.cpp`, профили и предыдущие common-pipeline/limits аудиты —
  публикация runtime-конфигурации;
- существующие lifecycle, ownership, output-worker и IPC security решения,
  которые нельзя потерять при миграции.

Это архитектурный статический вывод. Он не доказывает конкретный runtime CPU,
USB rate или latency на физической клавиатуре. Количественная оценка текущего
generic UAP hot path находится в
`ARTIFICIAL_LIMITS_PERFORMANCE_AUDIT.md`.

## Что в текущей архитектуре уже правильно

Следующие решения следует сохранить:

- UAP выполняется в disposable child process, связанном с owned job;
- parent контролирует поколения, bounded shutdown, hang/crash containment и
  запрещает небезопасный overlap поколений;
- точный HID interface path распределяется до открытия устройства;
- high-rate input передаётся через shared memory, а не через текстовый IPC;
- realtime wake является ускорением, а не единственным носителем истины;
- ViGEm имеет dedicated output worker и latest-state mailbox, поэтому driver
  I/O не блокирует `Backend_Tick`;
- HallJoy уже умеет запускать один и тот же EXE во внутренней child-роли — новая
  архитектура не требует дополнительных пользовательских файлов.

Аудит не предлагает заменить эти механизмы. Он меняет данные и ownership на
границе provider/core.

## ARCH-P01 — полный снимок превращается обратно в поклавишный SDK (P1)

### Текущий путь

Child host на каждом цикле вызывает private UAP `readFullBuffer`, отдельно
получает dense per-device snapshots, валидирует их, снова строит merged dense и
sparse представления, после чего `PublishSnapshot` копирует в один
`SharedState`:

- `codes[256]` и `values[256]` для compatibility sparse view;
- `denseValues[256]` для merged view;
- до восьми полных `denseDevices`, каждый ещё по 256 float;
- device telemetry и realtime/diagnostic counters.

В parent уже существует `DenseSnapshot`, который coherent копирует полный
массив из shared memory. Но production path не приобретает его один раз на tick.
`ReadAnalogByCodeWithDeviceFallback` сначала вызывает merged
`wooting_analog_read_analog`, затем повторяет
`wooting_analog_read_analog_device` для каждого известного device ID. Каждый
wrapper использует один exclusive `g_wootingApiLock`, а каждая нижняя
`AnalogHostClient_ReadAnalog*` операция создаёт локальный массив, вызывает
`DenseSnapshot`, копирует 256 значений и повторно сканирует их для active count.

Таким образом, runtime зависит от семантики публичного Wooting-style API даже
после того, как UAP данные уже находятся в собственной памяти HallJoy.

### Почему это архитектурный дефект

- стоимость чтения зависит от `keys * devices * snapshot-size`, хотя source
  generation один;
- compatibility lock сериализует независимые чтения уже опубликованных
  immutable данных;
- merged и per-device policy исполняется внутри поклавишного пути, а не один
  раз на coherent generation;
- seqlock contention или один неудачный acquisition может повторяться для
  каждой клавиши;
- bind capture и будущие потребители вынуждены обходить искусственный SDK, а
  не читать канонический changed/active set;
- исправления UAP и native неизбежно расходятся, потому что используют разные
  внутренние ABI.

Это тот же корень, который зарегистрирован как `HJ-V14-P1-018`, но здесь
зафиксировано архитектурное требование: Wooting compatibility API не может
оставаться внутренним HallJoy data plane.

### Требуемое состояние

- один provider snapshot приобретается не чаще одного раза на наблюдаемую
  generation/tick;
- snapshot остаётся неизменяемым на протяжении всего tick;
- нужная клавиша читается из tick-local индекса за O(1);
- per-device данные приобретаются один раз только если source policy требует
  различать устройства;
- public/legacy Wooting-style функции могут остаться adapter surface, но
  common pipeline через них не проходит.

## ARCH-P02 — IPC смешивает три направления и несколько временных шкал (P1)

`HallJoyAnalogHost::SharedState` одновременно содержит:

- parent-to-child control: `requestedKeycodeMode`, diagnostic injection;
- child-to-parent status и data: status, errors, heartbeat, generations,
  sparse/dense/per-device values;
- медленную device telemetry;
- crash/hang checkpoints и счётчики диагностики.

Обе стороны отображают весь объект с `FILE_MAP_ALL_ACCESS`. Один
`snapshotSequence` защищает не только аналоговые values, но и связанные с ними
device/telemetry копии. Parent control меняется атомарно вне этого snapshot
transaction, а `SetKeycodeMode` ждёт подтверждение циклом `Sleep(1)` до 500 ms,
хотя direct host фактически принимает только HID mode.

Дополнительно ABI жёстко связывает размер процесса с `sizeof(SharedState)`, 256
key slots и восемью устройствами. Заголовок имеет `kVersion = 10`, тогда как
комментарий у dense section называет его V11. Это не само по себе runtime
повреждение в self-spawned одинаковом EXE, но показывает, что эволюция схемы
происходит добавлением полей в один монолит, а не независимыми versioned
секциями.

### Последствия

- частая data publication переносит и редко нужную телеметрию;
- права записи шире фактического ownership;
- изменение диагностического или control контракта затрагивает data ABI;
- фиксированную capacity нельзя честно согласовать или безопасно увеличить без
  новой полной версии структуры;
- один event не различает новое USB sample, реальное изменение значения,
  connection change и telemetry refresh;
- shared monolith затрудняет одинаковый IPC для UAP и native providers.

### Требуемое состояние

Разделить контракт как минимум логически, предпочтительно отдельными mapping/
mailbox объектами с минимальными правами:

1. **Data plane, provider -> core.** Только immutable latest snapshots и
   publication metadata. Parent отображает payload read-only.
2. **Control plane, core -> provider.** Небольшие versioned команды start/stop,
   config, path assignment и diagnostic mode. Child не пишет в командный
   payload.
3. **Health/telemetry plane, provider -> core.** Редкие агрегированные counters,
   capability и ошибки. Обновление этого plane не будит realtime.

Новый IPC обязан различать:

- `sampleGeneration`: provider получил новый допустимый sample;
- `valueGeneration`: изменились values, ownership или connection state;
- heartbeat/health timestamp: worker жив, но input state мог не измениться.

Realtime wake нужен только для `valueGeneration` и событий, меняющих
авторитетность источника. Idle heartbeat не должен создавать 1000 downstream
ticks в секунду.

## ARCH-P03 — отсутствует единый provider contract для UAP и native (P1)

UAP и native сейчас сходятся слишком поздно:

- UAP сохраняет float, native возвращает `uint16_t milli`;
- UAP имеет merged и per-device snapshots, native registry читает каждый HID
  через `getMilli` и агрегирует `max`;
- capacity, freshness, identity и connection представлены разными типами;
- source arbitration находится в `backend.cpp` и ручных списках, а не в
  описании provider/device/key ownership;
- UAP crash-isolated, native работает в основном процессе;
- UAP и отдельные native backend по-разному определяют value-changed wake.

Пока общего контракта нет, исправление только `HJ-V14-P1-018` оставит дубли:
точность, key identity, capacity, freshness и lost-release semantics снова
придётся реализовывать отдельно для каждой стороны.

### Минимальный `AnalogProviderV2`

Точные C/C++ layouts нужно утвердить отдельным design package, но смысловой
контракт должен включать:

```text
ProviderDescriptor
  provider/protocol/version/capabilities

DeviceDescriptor[]
  stable device identity
  exact assigned interface identity
  protocol/layout proof classification
  capacity/truncation state

AnalogSnapshot
  schema version
  provider generation
  sample generation + timestamp
  value generation + timestamp
  connection/freshness state
  complete authoritative samples/releases

AnalogSample[]
  KeyIdentity
  normalized float value
  optional validated raw value/domain/calibration metadata
  source device + validity/ownership flags
```

`KeyIdentity` не должна быть просто `uint8_t HID`. Минимум нужен versioned
namespace плюс HID usage page/usage; provider-defined physical controls должны
иметь отдельный namespace и не маскироваться под случайный HID. Нельзя получать
analog identity из более позднего binary event.

Полный authoritative snapshot является источником истины для release-to-zero.
Changed bitset/list допустим только как ускоритель и не может заменять полный
снимок, иначе потерянный delta снова способен оставить зависшее нажатие.

### Capacity

Физическая память всегда конечна, поэтому «совсем без лимита» невозможно.
Правильный контракт не скрывает лимит:

- header сообщает `capacity`, `actualCount`, `requiredCount` и `truncated`;
- sections имеют version/offset/size, а не compile-time массив на весь срок ABI;
- при недостатке места provider не публикует правдоподобно полный список;
- parent может создать mapping согласованного размера или безопасно начать
  новое IPC generation с большей capacity;
- тестируются 1/8/16/32 устройства и переполнение, а не просто увеличивается
  magic 8 до magic 16.

## ARCH-P04 — process containment применяется только к UAP (P1)

UAP/Soup выполняется в isolated child. Native catalog запускается из обычного
startup приложения, его workers и storage находятся в основном процессе, а
`Backend_Tick` напрямую вызывает `NativeAnalogBackends_ReadMilli`.

Существующие native lifecycle/exception/stop guards значительно уменьшают
риск, но не могут дать процессную изоляцию от memory corruption, SEH fault,
неотменяемого vendor call или зависшего стороннего/USB состояния. Один дефект
native parser/worker способен затронуть UI, realtime, ViGEm и общий shutdown,
тогда как аналогичный UAP дефект ограничен disposable process.

### Целевое состояние

Весь transport/protocol I/O должен находиться за provider-host границей. Core
владеет только:

- device/path broker и routing policy;
- immutable input snapshot acquisition/merge;
- runtime config;
- curves, bindings, SOCD и XUSB;
- supervision и neutralization потерянного provider ownership.

Начальная разумная топология сохраняет простоту:

```text
HallJoy.exe: UI/core/realtime/output
  |- HallJoy.exe --provider-host=uap
  `- HallJoy.exe --provider-host=native
```

Не требуется сразу создавать процесс на каждое устройство. Более мелкую
изоляцию можно вводить позднее, только если измерения покажут пользу. Один EXE
для пользователя сохраняется.

При остановке/падении provider core обязан немедленно нейтрализовать только
принадлежащие этому provider клавиши, сохранить состояние остальных sources и
не запускать overlapping replacement generation.

## ARCH-P05 — runtime-конфигурация не является одним immutable поколением

`settings.cpp` хранит runtime state в большом числе независимых atomics. Пара
low/high упакована специально для coherent read, но остальные связанные поля
кривой, SOCD, output, bindings и профилей публикуются отдельно. Предыдущий
common-pipeline аудит уже доказал piecemeal profile load как
`HJ-V14-P1-015`.

Provider redesign не исправит этот дефект автоматически. Целевой core должен
получить immutable `RuntimeConfigSnapshot`:

```text
parse all files
  -> validate/migrate
  -> build curves, bindings and source policy
  -> atomically publish one config generation
  -> reset generation-bound SOCD/cache state
```

Realtime читает одну конфигурацию на tick. Это уменьшает количество atomic
loads и исключает смесь старых и новых полей. Отдельный новый risk ID не
добавляется: требование уже покрыто `HJ-V14-P1-015` и `HJ-V14-P2-010`.

## Рекомендуемый shared-memory data plane

Для HallJoy не нужна очередь всех USB samples. XUSB потребляет новейшее
состояние, а backlog старых samples увеличивает latency.

Предпочтителен latest-value snapshot mailbox:

- 2–3 payload slots;
- writer заполняет неактивный slot;
- per-slot sequence/version защищает от повторного использования во время
  parent copy;
- release publication атомарно сообщает active slot и generation;
- reader acquire-копирует один coherent snapshot и при необходимости делает
  bounded retry;
- старые поколения coalesce-ятся, а не накапливаются;
- output/realtime deadline использует самый новый complete generation.

Просто double buffer без slot sequence/reader ownership недостаточен: очень
быстрый writer может обойти reader и начать повторно использовать slot во время
копирования. Конкретный вариант — triple buffer, acknowledged slots или
double-buffer с проверяемой slot sequence — должен пройти отдельный concurrency
harness.

Named pipe/inherited pipe подходит для редких control messages. Передавать по
pipe одно сообщение на клавишу или на каждый 2/4/8 kHz sample не нужно.

## Source ownership и merge

Core должен получать от provider явные данные:

- каким exact device/interface владеет source;
- какие `KeyIdentity` он авторитетно измеряет;
- является ли zero реальным измерением, stale state или отсутствием ownership;
- когда был последний полный допустимый sample;
- был ли список усечён;
- какая policy разрешает несколько sources одной клавиши.

Merge нельзя сводить к безусловному `max`, глобальному `AnyConnected` или
ручному списку брендов. Policy должна быть catalog/provider-driven и
тестироваться для одновременных UAP/native/digital sources. Digital input не
используется для определения identity аналога ни при какой глубине actuation.
Это связывает новый контракт с `HJ-V14-P1-014`.

## Что делать с UAP и DrunkDeer

Немедленный полный fork/rewrite UAP не рекомендуется:

- UAP уже содержит несколько разных transport/parser families;
- без representative hardware большой rewrite создаст больше непроверенных
  путей, чем исправит;
- основная текущая цена возникает после parser — на compatibility/data-plane
  границе.

Правильная миграция:

1. Добавить private прямой snapshot export/adaptor из UAP в
   `AnalogProviderV2`.
2. Перестать вызывать Wooting-style per-key functions из `Backend_Tick`.
3. Оставить текущие UAP parsers за новым adapter и исправлять их по отдельному
   all-family аудиту.
4. Реализовать собственный DrunkDeer provider только после получения G65
   diagnostic evidence и точного протокола.
5. Выбрать native DrunkDeer как authoritative provider для доказанных моделей,
   оставив UAP fallback только там, где он отдельно квалифицирован.

Так DrunkDeer не требует отдельного пути в curves/bindings/UI: он меняет только
provider implementation.

## Неправильные варианты исправления

Запрещено считать архитектурным решением:

- сопоставлять аналог с последним binary keyboard event;
- заменить shared memory поклавишными pipe/RPC вызовами;
- складывать каждый USB sample в неограниченную очередь;
- только увеличить `kMaxDevices` с 8 до 16;
- только включить существующий full-buffer assist, сохранив два внутренних ABI;
- переписать все UAP families одним большим изменением без hardware matrix;
- сохранить milli ABI внутри нового provider;
- использовать heartbeat как value-changed событие;
- нейтрализовать все источники при падении одного provider;
- позволить UAP и native одновременно открывать один exact interface.

## Порядок миграции

### Этап A — characterization без смены поведения

- измерить source sample Hz, value-change Hz, IPC publish/wake Hz, snapshot
  retries, realtime tick/coalescing и XUSB submit Hz;
- добавить deterministic snapshot/merge model tests;
- закрепить текущие значения XUSB для известных simulator vectors.

### Этап B — типы и adapter boundary

- утвердить `KeyIdentity`, `AnalogSnapshot`, device/source status и
  `RuntimeConfigSnapshot`;
- создать provider registry и adapter для старого UAP/native пути;
- не менять протокольные байты и layouts на этом этапе.

### Этап C — прямой UAP snapshot

- получать UAP dense all-device snapshot один раз на generation/tick;
- построить tick-local O(1) index;
- перевести bind capture на active/changed set того же snapshot;
- удалить внутренний per-key/device Wooting fallback;
- доказать XUSB equivalence, lost-release, reconnect и multi-device semantics.

Это наиболее выгодный и наименее аппаратно-зависимый первый implementation
package.

### Этап D — IPC V2

- разнести data/control/telemetry;
- ввести latest-value multi-slot publication;
- добавить negotiated capacity и explicit truncation;
- разделить sample/value/health generations;
- проверить memory ordering, fast-writer/slow-reader, crash/restart и malformed
  section boundaries.

### Этап E — общий native provider

- адаптировать native catalog к тем же snapshot/identity/freshness semantics;
- убрать раннюю milli-квантизацию;
- централизовать ownership, stale neutralization и source merge;
- сохранить текущий exact-path proof и fail-closed admission.

### Этап F — native process isolation

- сначала перенести весь native catalog в один self-hosted process;
- доказать normal stop, cancellation, hang, C++/SEH fault, child termination,
  restart и отсутствие survivor;
- только после измерений решать, нужна ли изоляция per-family/per-device.

### Этап G — immutable configuration

- полностью parse/validate/migrate профиль вне realtime;
- опубликовать один config generation;
- сбрасывать зависимое SOCD/cache state на semantic generation change.

### Этап H — удаление legacy data plane

- удалить неиспользуемые sparse/per-key compatibility копии из внутреннего IPC;
- оставить внешний compatibility adapter только если он реально нужен;
- запретить статическим gate возврат pokey SDK path в `Backend_Tick`.

## Обязательные validation gates

До закрытия архитектурных P1 нужны:

- old/new deterministic XUSB report equivalence на обычных поддерживаемых
  значениях, с отдельно утверждёнными изменениями точности;
- ramp, simultaneous keys, complete release, lost packet, stale timeout,
  disconnect/reconnect и provider crash/hang;
- два одинаковых устройства, UAP+native одновременно и collision exact path;
- key identity выше 255, Menu/Context, Fn/OEM/media и invalid identity;
- 1/8/16/32 devices, insufficient-capacity и remap/restart generation;
- fast writer/slow reader, generation wrap boundary и malformed IPC sections;
- отсутствие realtime wake от неизменившихся values и одной telemetry update;
- подтверждение одной snapshot acquisition на generation/tick и O(1) key
  lookup;
- source/value/tick/XUSB rate и latency percentiles под CPU/USB load;
- process fault injection для UAP и native hosts без survivor и без потери
  input других providers;
- representative physical UAP и native keyboards; недоступные модели остаются
  честно `hardware pending`.

## Связь с реестром рисков

Аудит объединяет уже открытые:

- `HJ-V14-P0-001`, `HJ-V14-P1-011`, `HJ-V14-P1-016` — key domain;
- `HJ-V14-P1-014` — source arbitration;
- `HJ-V14-P1-015`, `HJ-V14-P2-010` — transactional config/SOCD state;
- `HJ-V14-P1-017` — ранняя квантизация;
- `HJ-V14-P1-018` — поклавишный UAP hot path;
- `HJ-V14-P1-019` — rate policy;
- `HJ-V14-P1-020` — фиксированная capacity;
- `HJ-V14-P2-011`, `HJ-V14-P2-014` — coherent UI/overlay и лишняя работа.

Новые архитектурные риски:

- `HJ-V14-P1-021` — смешанный монолитный IPC ABI;
- `HJ-V14-P1-022` — отсутствие единого UAP/native provider contract;
- `HJ-V14-P1-023` — native transport не имеет process containment.

## Финальный вердикт

Локально исправлять старый per-key UAP/HallJoy слой недостаточно. Его следует
заменить, но без big-bang rewrite протоколов.

Самая безопасная последовательность — сначала ввести provider contract и
прямое одноразовое чтение существующего UAP snapshot, затем разнести IPC,
перевести native на тот же контракт и только после этого вынести native I/O в
изолированный process. Это уменьшает код и горячую работу, одновременно создаёт
одинаковые correctness/freshness/identity правила для каждой нынешней и будущей
клавиатуры.

## 2026-08-22 R2-A/B1 implementation progress

The semantic foundation selected by D-060 is now production-compiled in
`analog_provider_v2.h/.cpp`. It provides versioned, namespaced key identity,
float/raw values, device/source proof fields, explicit ownership/freshness,
independent provider/sample/value/ownership generations and honest variable
capacity/truncation. Portable gates cover identity collisions, Fn/Menu/media,
invalid values, authoritative zero release, duplicate rejection, generation
transitions and 1/8/16/32 devices. Full native and MSVC Release builds pass.

This does not close ARCH-P01..P04. The live UAP host still uses the V10 fixed
shared structure and `Backend_Tick` still consumes the legacy compatibility
surface. The next implementation package is the read-only UAP adapter plus
old/new deterministic XUSB equivalence; IPC V2 and native process containment
remain later packages.
