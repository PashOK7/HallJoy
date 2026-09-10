# Аудит искусственных ограничений, точности и быстродействия HallJoy

Дата: 20 августа 2026 года.

Статус: завершённый статический аудит. Исходный код не исправлялся, сборка и
физические тесты не выполнялись. Документ обязателен вместе с аудитами UAP,
native-backend и общей analog pipeline.

## Краткий вывод

Ограничение HallJoy до 1001 значения (`0..1000`) не является ограничением
ViGEm или XInput. Это собственный ранний формат native-backend HallJoy. Он
необратимо уменьшает точность Sayo, MAD68 Pro R, Hex80, Aula MAX, Addressed и
SparkLink ещё до пользовательской кривой. UAP при этом передаёт `float`, поэтому
точность HallJoy зависит от того, через какой backend пришла клавиатура.

Одновременно обычный UAP-путь делает работу, несоразмерную одному чтению
клавиши: каждый per-key вызов заново копирует и сканирует все 256 значений,
после чего повторяет это для известных устройств под эксклюзивной блокировкой.
Цельный coherent snapshot уже существует, но production generic build его не
использует как основной источник.

Также существуют независимые жёсткие потолки 1 кГц для poll-only UAP и отправки
XUSB. Они полезны как защита от бесконтрольного цикла, но величина 1000 Гц не
получена из capability устройства или измеренной пропускной способности
ViGEm. Клавиатура на 2/4/8 кГц не может провести все изменения через нынешний
выходной scheduler, а UAP poll-only устройство вообще не опрашивается чаще
1 кГц.

Главный архитектурный принцип для исправления: сохранить исходную точность до
самой границы выбранного output protocol, читать один immutable snapshot на
поколение/tick, обрабатывать только изменившиеся данные и делать pacing
capability-driven, а не набором несвязанных magic constants.

## Классификация ограничений

| Класс | Значение | Политика |
|---|---|---|
| Внешнее | задано XUSB/XInput, USB/HID или форматом прошивки | честно показывать и квантовать только на этой границе |
| Защитное | ограничивает повреждённый ввод, зависание, reconnect storm или загрузку CPU | сохранять, но документировать, измерять и не смешивать с точностью |
| Искусственное | выбрано HallJoy без доказанной внешней необходимости | удалить, сделать динамическим или capability-driven |
| Неподтверждённое | может быть требованием устройства, но в коде нет доказательства | не менять без hardware/protocol gate, но считать открытым долгом |

## LIM-P01 — native ABI преждевременно квантует аналог до 1001 значения (P1)

### Доказательство

- `native_analog_backend.h` требует от каждого модуля normalized `[0..1000]` и
  возвращает `uint16_t getMilli`.
- `NativeAnalogReadResult` хранит поле `milli`, а registry дополнительно
  обрезает результат до `1000`.
- `backend.cpp` восстанавливает `float` делением `native.milli / 1000.0f`.
- `BackendAnalogTelemetry::analogOutputLevels` всегда сообщает `1001`.
- шаблон нового native-backend в `tools/new_native_backend.py` также создаёт
  преобразование в milli, поэтому ограничение автоматически размножается.

Это не требование конечного XUSB report. HallJoy формирует stick как signed
16-bit и сейчас может использовать 65 535 значений `-32767..32767`. Код не
использует только крайнее `-32768`. XUSB trigger действительно имеет лишь 256
значений, поэтому 1001 входного значения для trigger достаточно, но общий
native contract применяется и к 16-bit sticks.

### Реальные потери до кривой

| Источник | Доступный/допустимый raw domain | Что делает HallJoy сейчас |
|---|---:|---|
| Sayo | `0..4000`, 4001 значение | сжимает до 1001; затем `milli < 4` принудительно делает нулём |
| MAD68 Pro R | `0..1600`, 1601 значение | сжимает до 1001 |
| Hex80 | обычный `travelMax=3300` | отбрасывает raw `0..8`, остаток сжимает примерно в 3.3 раза |
| Aula WIN60 HE MAX | физически подтверждённый maximum `3400 um` | сжимает 3401 положение до 1001 |
| Addressed `09/94/02` | 15-bit wire value, рабочий span вычисляется для каждой клавиши | сжимает динамический span до 1001; `milli < 8` затем уничтожается |
| SparkLink | предполагаемый диапазон до `3000..5000` | нормализует по наблюдавшемуся максимуму и округляет до milli |
| Aula W669 | известный processed maximum обычно 340, валидатор допускает до 10000 | известная модель не теряет уровни, будущий больший maximum потеряет |
| DrunkDeer diagnostic / IROK experimental | one-byte travel с малым nominal maximum | 1001 не является текущим узким местом |
| UAP | plugin публикует normalized `float` | сохраняет более высокую точность до общей кривой/XUSB |

Источник-domain deadzone необратим: пользовательская кривая и собственная
deadzone уже не могут вернуть удалённые малые значения. Фильтрация шума может
быть необходима, но значения Sayo `4 milli`, Addressed `8 milli` и Hex80 raw
`8` должны иметь физическое обоснование/калибровку либо быть явно управляемой
политикой, а не скрытым вторым deadzone.

### Требуемый контракт

Native ABI следующей версии должен публиковать normalized `float` как минимум
с точностью float32 и отдельно, при наличии, source raw numerator/domain и
метаданные calibration/validity/timestamp/generation. Нельзя заменять один
жёсткий предел `1000` на другой общий предел `65535`: будущий источник может
иметь иной domain. Квантование выполняется один раз при построении конкретного
выхода: 16-bit axis, 8-bit trigger или будущего output protocol.

Для signed stick следует использовать полный асимметричный XInput domain:
negative side до `-32768`, positive до `32767`. Потеря одного кода сейчас мала,
но искусственна.

## LIM-P02 — параметры кривой и profiles имеют только шаг 0.001 (P1/P2)

`settings.cpp`, `key_settings.cpp`, `settings_ini.cpp` и keyboard curve presets
округляют deadzone, cap, control points и weights в integer milli. Быстрый
per-key snapshot использует `int16_t` milli; INI сохраняет `float * 1000`.
Следовательно, даже float UAP не даёт пользователю настроить параметры точнее
0.1%.

Дополнительные ограничения:

- global low/high всегда разделены не менее чем на `0.01` (1%);
- output cap должен быть не меньше anti-deadzone плюс `0.01`;
- Last Key Priority sensitivity разрешена только `0.02..0.95`;
- rational Bezier weights обрезаются до `0..1`, хотя вес больше единицы сам по
  себе математически допустим;
- curve control points фиксированы двумя точками и двумя режимами.

Запрет вырожденной кривой и нечисловых значений является защитным. Но именно
шаг 0.001, зазор 1% и верхняя граница веса 1 не следуют из формата XUSB.
Нужен versioned profile schema, который читает старые milli без потери
совместимости, а новые значения хранит как достаточный decimal/float. UI может
оставлять удобный coarse step с вводом точного значения; UI step не должен быть
внутренней точностью pipeline.

## PERF-P01 — generic UAP выполняет O(keys × devices × 256) работы за tick (P1)

### Текущий путь одного HID

`ReadRaw01Cached` вызывает `ReadAnalogByCodeWithDeviceFallback`. Тот выполняет:

1. merged `wooting_analog_read_analog(code)`;
2. per-device `wooting_analog_read_analog_device(code, id)` для каждого
   известного parent-у устройства;
3. каждый вызов проходит через один exclusive `g_wootingApiLock`;
4. `AnalogHostClient_ReadAnalog*` создаёт массив из 256 float;
5. `DenseSnapshot` копирует все 256 значений из shared memory, проверяет seqlock
   и затем ещё раз сканирует 256 значений для `activeCount`;
6. per-device вариант дополнительно линейно ищет устройство среди восьми
   dense records.

В production generic build одновременно заданы
`kEnableFullBufferAssist=false` и `kPreferFullBufferSnapshot=false`. Primary
full snapshot включён только для отдельного Madlions diagnostic profile, хотя
изолированный host уже публикует канонический dense snapshot.

При 100 уникальных HID и 8 известных устройствах получается до 900 полных
256-float copy/scan за tick — около 900 KiB просмотренной памяти на tick, или
порядка 900 MB/s при 1 кГц, без учёта locks, seqlock retries и native/catalog
работы. Это статическая верхняя оценка, не runtime measurement. Bind capture
сканирует HID `1..255` и создаёт ещё худший режим.

### Лишняя работа до parent process

UAP `Device::publish_dense` на каждом poll заново проходит 256 значений,
обновляет production telemetry и всегда увеличивает generation, даже если
снимок не изменился. Child host после каждого такого поколения:

- вызывает sparse full-buffer API;
- копирует до 8 per-device массивов по 256 float;
- снова строит merged dense и sparse views;
- копирует их в shared memory;
- всегда сигналит parent snapshot event.

Parent bridge всегда будит realtime loop. Поэтому idle poll-only UAP при 1 кГц
может создавать примерно 1000 полных downstream ticks в секунду без изменения
выходного XUSB report. Sayo аналогично уведомляет realtime после каждого depth
packet без сравнения старого и нового массива; SparkLink и большинство новых
native backend уже уведомляют только при реальном изменении.

### Исправляемая архитектура

- один versioned immutable all-device snapshot на source generation;
- одна copy/map операция на realtime tick, затем O(1) lookup по key identity;
- per-device data копируется один раз только если policy действительно требует
  различать устройства;
- source публикует две последовательности: sample/freshness и value-changed;
- heartbeat/telemetry не будят realtime, если input state не изменился;
- bind capture читает active/changed analog set из того же snapshot, никогда не
  выводя identity из binary events;
- production telemetry обновляется по изменившимся значениям или на редком
  sampling interval, а не полным проходом на каждый poll.

## PERF-P02 — три независимых magic-ограничения частоты (P1)

### UAP poll-only

Все шесть `.sun` профилей компилируются с `UAP_POLL_TARGET_US=1000`. Успешный
цикл paced по start-to-start deadline; медленная транзакция не получает лишнюю
паузу. Это лучше старого additive sleep, а exponential failure backoff до 64 ms
нужно сохранить. Но быстрый transport никогда не превысит 1 кГц, даже если
устройство и USB path устойчиво поддерживают 2/4/8 кГц. Static test прямо
требует строку «targets at most 1 kHz», то есть тест закрепляет выбранный
потолок, но не доказывает его оптимальность.

### Realtime heartbeat

Настройка `PollingMs` ограничена `1..20 ms`. Input notifications обходят этот
heartbeat, поэтому 1 ms не является обязательной задержкой stream/native
источника. Однако UI называет величину polling interval, хотя MaxBurst
SparkLink её игнорирует, stream backends event-driven, а UAP имеет собственную
compile-time частоту. Одна настройка управляет несколькими несопоставимыми
вещами и не сообщает effective input/output rate.

### XUSB publication

Каждый `VigemOutputScheduler` безусловно конфигурируется интервалом `1000 us`.
Первое изменение после idle отправляется сразу, последующие корректно
coalesce-ятся до fixed deadline, unchanged report не посылается. Эти свойства
хорошие. Искусственным является именно недокументированный общий предел 1 кГц.
Он не получен из ViGEm capability/return timing и не сопоставлен с rate
источника или потребителя.

Нужен один явный rate policy с наблюдаемыми `source Hz`, `value-change Hz`,
`realtime tick Hz`, `coalesced Hz`, `ViGEm submit Hz` и latency percentiles.
Default protection допустима, но ceiling должен быть доказан измерениями и,
если безопасно, настраиваться/адаптироваться. Устройство выше 1 кГц нельзя
опрашивать и пересчитывать 8 тысяч раз, если output всё равно отправляется
тысячу раз: до ближайшего законного output deadline следует coalesce-ить
latest source generation, сохраняя release/edge correctness.

## PERF-P03 — backend-specific pacing содержит неподтверждённые паузы (P2)

| Backend | Наблюдение | Класс |
|---|---|---|
| Aula MAX | после каждой полной matrix transaction всегда ждёт `kPollPauseMs=1` | искусственный/неподтверждённый; измерить USB stability с 0 и adaptive deadline |
| Hex80 | 104 slots читаются chunks ровно по 4, то есть 26 request/response на matrix | неподтверждённый protocol limit; 128-byte payload теоретически вмещает больше, нужен hardware sweep допустимого size |
| SparkLink | default MaxBurst делает лишь периодический yield; Safe добавляет `PollingMs` после каждой row | режимы осмысленны, но UI должен показывать фактическую matrix Hz и цену режима |
| Addressed | packet ограничен девятью keys | wire-format limit для текущего scheduler, не общий HallJoy limit |
| report-stream backends | blocking overlapped read без искусственного success sleep | корректно |
| reconnect/failure paths | 15 ms, 250/500/750/1000 ms и exponential backoff | защитные; не удалять без failure-storm теста |

Нельзя автоматически увеличивать Hex80 chunk или убирать device-specific pause
без физического теста: это открытая возможность оптимизации, а не доказанный
совместимый fix.

## LIM-P03 — фиксированная вместимость устройств расходится между слоями (P1)

- UAP хранит devices в динамическом `std::vector` и `_device_info` способен
  вернуть столько записей, сколько запросил caller.
- private dense snapshot и telemetry ABI ограничены `kMaxDevices=8`.
- parent `backend.cpp` хранит до 16 known device IDs.
- per-device fallback для ID 9..16 всё равно делает вызовы, но child dense lookup
  не может найти соответствующий snapshot.

Merged UAP value может случайно продолжать учитывать устройства после восьмого,
потому что plugin объединяет весь vector, но HallJoy теряет их identity,
telemetry и per-device snapshot. Усечение не имеет explicit truncated flag и
может выглядеть как полный список.

Sayo отдельно прекращает enumeration после восьми устройств. Native backend
registry имеет compile-time capacity 32; это разумный внутренний guard для
нынешнего каталога, если переполнение является явной startup ошибкой, а не
тихим усечением.

Следующий IPC ABI должен передавать required count/truncated flag и versioned
variable-length sections либо безопасно переоткрываемый snapshot нужного
размера. Нельзя просто увеличить 8 до очередного magic 16.

## Остальные limits и verdict

### Обоснованные внешние

- максимум четыре XInput/XUSB controller slots для обычных потребителей;
- четыре signed 16-bit stick axes, два 8-bit trigger и фиксированный набор
  XUSB buttons;
- размер конкретного HID report и доказанная вместимость wire packet;
- bounded parser lengths, HTTP header/body/target limits и client ownership
  table — security/resource policy, не analog precision.

Поддержка более четырёх gamepad имеет смысл только как новый output backend,
например не-XInput. Увеличение `kMaxVirtualPads=4` внутри текущего XUSB режима
не сделает дополнительные контроллеры доступными обычному XInput consumer.

### Искусственные/неполные

- key domain 256 уже записан как `HJ-V14-P0-001`, `P1-011` и `P1-016`; USB HID
  identity требует как минимум usage page + usage, а не 8-bit index;
- tracked HID list допускает дубликаты и поэтому повторяет UI/cache работу;
- axis имеет ровно один minus и один plus HID, trigger — один HID, тогда как
  button допускает несколько; несколько физических альтернатив для одной
  стороны axis/trigger искусственно запрещены;
- gamepad button использует фиксированный threshold `0.50`, bind capture —
  фиксированные `0.12`; это не задержка, но скрытая responsiveness policy;
- UI refresh сохраняется как `1..200 ms`, однако key-drag принудительно не
  медленнее 50 ms, а mouse visual timer всегда `8..33 ms`. Выбранные пользователем
  51..200 ms поэтому местами игнорируются и создают лишние repaint/timer wakes;
- overlay на каждый `/state` заново сериализует layout, labels, settings и все
  key values даже при неизменном generation. Browser уже умеет не рисовать
  неизменный frame, но server всё равно строит и передаёт полный JSON;
- overlay polling и desktop UI не используют один coherent generation snapshot.

## Что нельзя «оптимизировать» удалением защиты

Следующие механизмы полезны и должны оставаться:

- finite HID I/O timeout и cooperative cancellation;
- exponential backoff после transport failure;
- bounded reconnect pacing;
- seqlock/generation validation shared snapshot;
- latest-value mailbox и coalescing промежуточных report;
- немедленный первый changed report после idle;
- пропуск неизменных XUSB reports;
- fail-closed parser/admission bounds;
- stale/lost-release neutralization, которой некоторым backend сейчас как раз
  не хватает.

Оптимизация не должна превращаться в busy-spin, flood USB/ViGEm, потерю release
или использование binary key event для определения analog identity.

## Обязательный roadmap исправления

1. Ввести измерения без verbose per-sample log: source/sample/change/tick/send
   counters, skipped unchanged generations, snapshot copies/bytes, lock waits,
   tick и end-to-end latency percentiles.
2. Создать versioned canonical analog sample/snapshot contract с полноценной
   key identity, float normalized value, optional raw domain, timestamp,
   source/device identity, validity и generation.
3. Перевести native modules с milli на новый contract без source-domain
   quantization. Старый ABI допускается только как временный adapter.
4. Сделать один UAP snapshot acquisition на generation/tick и удалить normal
   per-key/per-device full-array copies. Bind capture использует analog changed
   set того же snapshot.
5. Отделить sample/freshness generation от value-change generation и не будить
   realtime на неизменные UAP/Sayo snapshots.
6. Заменить 8-device IPC на честный versioned variable-capacity contract.
7. Объединить input/output pacing policy, убрать недоказанный compile-time 1 kHz
   как универсальный предел и квалифицировать безопасные rates на ViGEm и
   representative 1/2/4/8 kHz devices.
8. Мигрировать profile precision, сохранив чтение старых milli profiles.
9. Устранить duplicate tracking, inconsistent UI timers и full overlay JSON
   polling; static layout/settings передавать отдельно от changed analog state.
10. Только после unit/load/simulator gates проводить физическую квалификацию;
    отсутствие доступной клавиатуры не позволяет объявить timing fix Verified.

## Обязательные validation gates

- exact raw-to-float-to-XUSB vectors для 8/10/12/15/16-bit sources;
- monotonicity, endpoint, zero/release и no-early-quantization tests;
- старые profile milli читаются тождественно, новые precise values переживают
  save/load без округления до 0.001;
- 1/8/16/32 synthetic devices без тихого усечения;
- UAP cost растёт O(keys + devices), а не O(keys × devices × 256);
- idle UAP не создаёт realtime wake storm;
- bind capture не использует binary events и не делает 255 полных snapshots;
- changed input не теряет release при coalescing;
- ViGEm stalled/slow/fast models подтверждают bounded CPU и latest report;
- физические rate/latency/USB-error runs для каждого изменённого poll protocol;
- production profiler отдельно для native Spark и UAP poll/stream устройств:
  прежний Irok profile не измеряет найденный UAP hot path.

## Новые риски

- `HJ-V14-P1-017`: ранняя native milli-квантизация и скрытые source deadzones;
- `HJ-V14-P1-018`: O(keys × devices × 256) generic UAP path и unchanged wake storm;
- `HJ-V14-P1-019`: несвязанные жёсткие 1 kHz ceilings скрывают rate и теряют
  преимущество более быстрых устройств;
- `HJ-V14-P1-020`: silent 8-device snapshot/telemetry truncation при parent
  capacity 16;
- `HJ-V14-P2-012`: milli precision profiles и искусственные curve bounds;
- `HJ-V14-P2-013`: single-HID axis/trigger и фиксированные action thresholds;
- `HJ-V14-P2-014`: duplicate tracking, inconsistent UI timers и full overlay
  serialization;
- `HJ-V14-P2-015`: неподтверждённые Aula pause/Hex80 chunk и protocol-specific
  throughput debt.

Все P1 добавлены в обязательный release blocker. P2 являются обязательным
roadmap debt и должны быть закрыты до заявления, что HallJoy не имеет
искусственных ограничений; они не разрешают ослаблять safety bounds без
доказательств.

## 2026-08-22 R2-A/B1 implementation progress

`AnalogValueV2` now preserves normalized float and optional exact raw
numerator/domain, while legacy milli conversion is explicitly marked rather
than becoming the new contract. The portable old-bug vector proves that raw
`1/4095` and `2/4095` remain distinct although both truncate to legacy milli
zero. Snapshot capacity is explicit and complete 1/8/16/32-device models pass;
hidden truncation fails closed.

The production native ABI still returns milli and the UAP live IPC is still
fixed to eight devices. Therefore P1-017/P1-020 remain open until adapters,
live routing, profile/output and variable-capacity IPC/hardware gates pass.
