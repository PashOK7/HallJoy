# Аудит всех поддержек клавиатур в UAP/Soup

Дата: 20 августа 2026 года.

Статус: полный статический аудит Windows x64 пути, реально встроенного в HallJoy.
Аппаратная корректность отдельных матриц и firmware-вариантов не объявляется без
физического теста. Git при аудите не использовался.

Связанный подробный аудит DrunkDeer:
[`DRUNKDEER_UAP_PROTOCOL_AUDIT.md`](DRUNKDEER_UAP_PROTOCOL_AUDIT.md).

## Итог

Да, проблема `Fn/Menu` общая и затрагивает почти все семейства UAP.

Выполняемый путь содержит два независимых механизма потери специальных клавиш:

1. UAP преобразует `Fn`, media и OEM в корректные для Wooting ABI 16-битные коды
   `0x3xx/0x4xx`, но HallJoy-fork хранит только 256 значений и отбрасывает всё
   `>= 256`.
2. Soup объявляет стандартный HID `HID_CONTEXT_MENU = 0x65`, но забывает добавить
   его в обе функции преобразования `HID <-> soup::Key`. Поэтому `KEY_CTX` либо
   отбрасывается ещё при разборе HID, либо превращается в ложный код `0`.

Потери по семействам:

| Семейство | Что гарантированно теряется в текущем HallJoy UAP |
|---|---|
| Wooting | `Fn`, media controls, brightness/profile OEM; обычный HID Menu `0x65` не декодируется |
| Razer | `Fn`; Context/Menu превращается в код `0` |
| DrunkDeer | `Fn`, `OEM_1/Menu` |
| Keychron/Lemokey | `Fn`, все размеченные OEM-кнопки |
| NuPhy | `Fn` |
| Madlions MAD60 | `Fn`, Context/Menu |
| Madlions MAD68/MAD68R | `Fn` |

Кроме специальных клавиш обнаружены критические ошибки в NuPhy, Keychron и
DrunkDeer, общая поломка hotplug в текущем профиле HallJoy и несколько transport/
performance-дефектов.

## Реальный профиль сборки HallJoy

HallJoy собирает приватный ABI1 target
`abiv1-pluswooting-mad68native.sun` со следующими значимыми флагами:

- `WOOTING_SUPPORT` — Wooting тоже обслуживается этим UAP;
- `UAP_EXCLUDE_HALLJOY_NATIVE=1` — exact HID interfaces, уже доказанные native-
  backend, исключаются до открытия Soup;
- `UAP_DISABLE_HOTPLUG=1` — встроенный UAP не обнаруживает новые устройства после
  запуска;
- `UAP_POLL_TARGET_US=1000` — poll transports стремятся к deadline 1 мс;
- `UAP_MADLIONS_DISABLE_DIGITAL_ASSIST=1` — текущий HallJoy не использует бинарную
  активацию для ускорения Madlions;
- `UAP_SYNCHRONOUS_POLL=0` — каждое устройство имеет отдельный worker.

Soup закреплён на commit `b02796b0b20276277c8a4b4d3759643eeab43ff7`,
а overlay-файлы и их хеши закреплены в `tools/dependency-lock.json`.

## Фактический каталог

### Wooting

- VID `31E3`, любой PID с usage page `FF54` (V1) или `FF53` (V2);
- старый VID `03EB`, PID `FF01`/`FF02`, usage page `FF54`.

### Razer

- `1532:0266` Huntsman V2 Analog;
- `1532:0282` Huntsman Mini Analog;
- `1532:02A6` Huntsman V3 Pro;
- `1532:02A7` Huntsman V3 Pro TKL;
- `1532:02B0` Huntsman V3 Pro Mini.

### DrunkDeer

Известные PID `2382`, `2383`, `2384`, `2386`, `2391`, а также любой неизвестный
PID VID `352D`, имеющий input report ID `04` и непустое product name.

### Keychron/Lemokey

- Q1 HE: `3434:0B10` ANSI, `0B11` ISO, `0B12` JIS;
- Q3 HE ANSI: `3434:0B30`;
- Q5 HE ANSI: `3434:0B50`;
- K2 HE: `3434:0E20` ANSI, `0E21` ISO, `0E22` JIS;
- K4 HE ANSI: `3434:0E40`;
- Lemokey P1 HE ANSI/ISO: `362D:0610`/`0611`.

### NuPhy

Любой PID VID `19F5` на usage page `0001`, usage `0000`, если product name
непустое. README прямо заявляет “Everything by NuPhy”.

### Madlions

- MAD60HE: PID `1053`, `1054`, `1055`, `1056`, `105D`;
- MAD68HE: `1058`, `1059`, `105A`, `105C`;
- MAD68R: `10A7`.

Все требуют usage page `FF60`, usage `0061`. Exact interface может быть забран
проверенным HallJoy native-backend; тогда UAP его не открывает.

## Общие подтверждённые дефекты

### UAP-G01 — 16-битный ABI насильно обрезан до 8-битного пространства

`mapToWootingKey()` выдаёт:

- media: `0x3B5`, `0x3B6`, `0x3B7`, `0x3CD`;
- OEM/profile/brightness: `0x401..0x405`, `0x408`;
- Fn: `0x409`.

Но `HallJoyDenseSnapshot::kKeyCount` равен `256`. `update_from_keyboard()`
выполняет `if (code >= KEY_COUNT) continue`, а `read_analog()` также отвергает
такие коды. Это не только ограничение интерфейса HallJoy: оно ломает и публичный
Wooting-style ABI этой собранной DLL.

Исправление не должно просто увеличить один массив. Ограничение `0..255` проходит
через shared-memory ABI, analog host, backend, bindings, curves, UI и overlay.
Нужен versioned key domain либо осознанные внутренние pseudo-codes.

### UAP-G02 — Context Menu объявлен, но не преобразуется

В `HidScancode.hpp` существует `HID_CONTEXT_MENU = 0x65`, однако:

- `hid_scancode_to_soup_key()` не содержит `HID_CONTEXT_MENU -> KEY_CTX`;
- `soup_key_to_hid_scancode()` не содержит `KEY_CTX -> HID_CONTEXT_MENU`.

Следствия:

- стандартный Menu из Wooting/HID теряется как `KEY_NONE`;
- вручную созданный `KEY_CTX` Razer/Madlions проходит в `mapToWootingKey()`,
  получает default-код `0` и публикуется как несуществующая клавиша.

Это отдельный простой дефект Soup, не требующий расширения key domain.

### UAP-G03 — hotplug отключён во всех HallJoy UAP targets

`UAP_DISABLE_HOTPLUG=1` означает:

- клавиатура, подключённая после запуска HallJoy, не обнаруживается;
- Razer Synapse, запущенный после HallJoy, не приводит к обнаружению Razer;
- после перезапуска analog host в момент, когда клавиатура отключена, последующее
  подключение не будет замечено здоровым child process;
- `remove_stopped_devices()` не вызывается, поэтому завершённые worker остаются
  в registry.

Нулевое число устройств считается нормальным успешным состоянием child host, так
что supervisor не обязан перезапускать его только ради нового enumeration.

### UAP-G04 — ghost devices при отключённом hotplug

Telemetry и dense export безусловно ставят `DeviceFlag_Connected` каждому объекту
в registry. При отключённом hotplug завершённый worker оттуда не удаляется.

Если отключено единственное устройство, aggregate `read_full_buffer` обычно
вызовет перезапуск child. Но если отключён stream-device, а рядом остаётся другое
живое UAP-устройство, `live_device` остаётся true, stream freshness не проверяется,
и отключённое устройство может числиться подключённым бессрочно.

### UAP-G05 — stale snapshot при исключении

Worker очищает snapshot только после нормального выхода из цикла. В `catch (...)`
он отмечает общий plugin fault, но не вызывает `clear_snapshot()`. Malformed report
одного устройства способен временно оставить последнее нажатие и одновременно
остановить все UAP-workers в этом child process.

### UAP-G06 — все HID interfaces требуют право записи

Windows enumeration открывает каждый кандидат с `GENERIC_READ | GENERIC_WRITE`
до определения семейства. Даже чисто streaming Wooting/Razer/NuPhy не принимаются,
если доступна только безопасная read-часть. Это ухудшает совместимость с другими
драйверами и приложениями без протокольной необходимости.

### UAP-G07 — raw evidence теряется до telemetry

Все семейства проходят через float clamp `0..1`; telemetry видит уже
нормализованные/квантованные значения. Ошибочный scale, overflow и raw выше
номинала часто выглядят как корректное полное нажатие. Для расследований нужны
family parser counters до clamp.

### UAP-G08 — release bookkeeping не разделено по устройствам

`pending_release` является одной process-global таблицей. В публичных вызовах,
чередующих `read_full_buffer_device` для разных device ID, release от устройства A
может попасть в ответ устройства B. HallJoy использует aggregate device ID `0`,
поэтому его основной путь этим конкретным дефектом не затронут.

### UAP-G09 — лишняя работа на каждом report

Каждый worker создаёт новый `std::vector<ActiveKey>`, строит новую 256-float
таблицу, обновляет telemetry и будит host даже при идентичном состоянии. Poll-
семейства дополнительно создают Buffer и kernel objects. Это не причина неверной
раскладки, но заметный источник CPU/jitter при нескольких клавиатурах.

## Wooting

### Что сделано хорошо

- выбираются специализированные usage pages `FF54/FF53`;
- V1/V2 используют bounds-aware `MemoryRefReader`;
- scale явно задан как `0..255` или `0..1023`;
- blocking stream read можно отменить через persistent OVERLAPPED при shutdown;
- повторный Soup key в старом V1 report объединяется внутри report.

### Подтверждённые проблемы

1. `Fn`, media, profile и brightness коды декодируются, но затем теряются из-за
   UAP-G01.
2. Обычный HID Context Menu `0x65` теряется из-за UAP-G02.
3. Один пустой V1/V2 read немедленно объявляет устройство disconnected. Нет
   различения transient read error и физического отключения.
4. В targets без `WOOTING_SUPPORT` фильтр пропускает `FF53`, исключая только
   `FF54`. Поэтому обычная non-Wooting сборка может неожиданно открыть Wooting V2.
   Текущий HallJoy использует `WOOTING_SUPPORT`, поэтому это latent packaging bug,
   а не активная ошибка его target.

Оценка: обычные клавиши — наиболее безопасная ветка UAP; специальные клавиши и
общий hotplug всё равно сломаны.

## Razer

### Detection

PID и требуемый input report ID (`07` для V2, `0B` для V3) проверяются, но usage
page/usage и report length не проверяются. Наличие процесса определяется только
по именам `RazerAppEngine.exe` и `Razer Synapse 3.exe`.

### Подтверждённые проблемы

1. Parser отбрасывает первый byte как report ID, но не проверяет, что реально
   пришёл `07`/`0B`. Другой report того же interface будет разобран как аналог.
2. `Fn` теряется по UAP-G01.
3. Razer scancode `81` превращается в `KEY_CTX`, затем в ложный код `0` по UAP-G02.
4. Wrong-ID или malformed, но непустой report сбрасывает счётчик пустых reports и
   публикует нулевой/ложный snapshot вместо protocol error.
5. Synapse проверяется только при discovery. С выключенным hotplug запуск Synapse
   после HallJoy не помогает; исчезновение stream без ошибки ReadFile не имеет
   отдельного runtime watchdog.

Bounds чтения записей V2/V3 защищены `MemoryRefReader`; прямого OOB на короткой
записи здесь не найдено.

## DrunkDeer

Подробно описан отдельным аудитом. Основные проблемы:

- одна матрица `6x21` для всех моделей и неизвестных PID;
- `Fn/OEM_1` теряются;
- три reports не проверяются и склеиваются по порядку прихода;
- короткие reports приводят к underflow/OOB;
- write/read/mutex могут ждать бесконечно;
- fixed scale `40` и последующий clamp скрывают аномалии.

## Keychron и Lemokey

Есть два transport режима:

- custom/full-matrix: один request и четыре ответа;
- official/per-key polling: отдельный request на каждую выбранную клавишу.

### K-UAP-01 — неправильное объединение вариантов раскладки

Q1 HE ANSI (`0B10`), ISO (`0B11`) и JIS (`0B12`) получают одну таблицу
`layout_keychron_q1_he`. K2 HE ANSI/ISO/JIS (`0E20..0E22`) также получают одну
таблицу `layout_keychron_k2_he`.

Эти таблицы не содержат ISO/JIS-specific keys, включая `KEY_INTL_BACKSLASH` и
дополнительные JIS positions. Это подтверждённая неполная поддержка заявленных
PID, даже если основная буквенная матрица совпадает.

Lemokey P1 ANSI/ISO имеет две отдельные таблицы — здесь разделение сделано лучше.

### K-UAP-02 — небезопасные короткие ответы

`safeReceiveReport()` гарантирует только два bytes и совпадение первых двух
полей. Но дальше код:

- читает `report.at(2)` при version response;
- читает `report.at(3)` или `report.at(6)` при per-key response;
- в full-matrix режиме проверяет только `empty()`, затем склеивает `size()-2` и
  индексирует layout size.

`Buffer::at()` в Soup не делает bounds check — он эквивалентен `operator[]`.
Поэтому короткий непустой report вызывает реальный out-of-bounds read, а не
безопасное исключение.

### K-UAP-03 — пакеты не коррелируются полностью

Фильтр проверяет только command/value bytes. Не проверяются report ID, размер,
chunk index/offset, status, sequence и порядок четырёх full-matrix пакетов. Четыре
подходящих ответа склеиваются строго в порядке прихода.

### K-UAP-04 — generic transport без таймаутов

- результат каждого `sendReport()` игнорируется;
- write использует бесконечный `GetOverlappedResult(..., TRUE)`;
- `safeReceiveReport()` может бесконечно читать, пока не встретит нужные два bytes;
- read ждёт без timeout;
- shutdown не может отменить локальный pending write.

### K-UAP-05 — mutex и аллокации

Каждый poll создаёт/закрывает named mutex handle `KeychronMtx`, ждёт его с
`INFINITE`, не проверяет wait result и освобождает вручную. Исключение/OOB fault
может оставить mutex abandoned. Все Keychron/Lemokey устройства сериализуются.

### K-UAP-06 — стоимость official polling зависит от бинарной активации

Без custom full-matrix firmware background scanner проверяет только одну группу
из четырёх matrix positions за цикл. Digital state ускоряет выбор после бинарной
активации, а уже активные analog keys продолжают опрашиваться.

Для глубокой точки бинарной активации первый слабый analog press обнаруживается
только background scan:

| Layout | Ячеек | Состояний полного background scan |
|---|---:|---:|
| Q1/P1 | 90 | 23 |
| Q3/K2 | 96 | 25, включая один пустой цикл |
| Q5/K4 | 114 | 29 |

Каждое непустое состояние требует до четырёх последовательных request/response,
плюс отдельные запросы для digital/held keys. Поэтому nominal deadline 1 мс не
является реальной частотой полной матрицы; latency и USB load растут с числом
зажатых клавиш.

Пустой цикл у layouts, размер которых делится на четыре, создаётся off-by-one
условием `(layout_size >> 2) + 1`.

### Остальные подтверждённые ограничения

- travel `< 5` принудительно считается нулём: встроен несогласованный с HallJoy
  deadzone до пользовательской кривой;
- scale жёстко равен `235`, значения выше silently clamp;
- `Fn` и OEM-кнопки Q3/Q5/K2/K4 теряются по UAP-G01;
- detection проверяет usage/PID, но не input/output report lengths;
- README fork перечисляет Q1/Q3/Q5/K2, но не поддерживаемый кодом K4 HE —
  документация UAP устарела.

## NuPhy

Это одна из наиболее рискованных веток.

### N-UAP-01 — чрезмерно широкое распознавание

Любое устройство VID `19F5` с usage `0001:0000` и product name принимается как
аналоговое, без PID allowlist, report ID/length или capability proof. После этого
все неизвестные PID получают scale `800`, кроме `6120/FEE0`, которым назначается
`1600`.

Заявление “Everything by NuPhy” не подтверждено протокольным admission test.

### N-UAP-02 — использование неинициализированных данных

После byte `type == A0` parser игнорирует результаты `skip/u16_be`. Если report
короче ожидаемых восьми bytes, локальные `scancode` и/или `value` остаются
неинициализированными и затем читаются. Это undefined behavior на входе от HID.

### N-UAP-03 — undefined conversion при raw выше scale

Parser вычисляет float `value / scale * 255` и напрямую преобразует его в
`uint8_t` без clamp. Если firmware отдаст значение выше `800/1600` либо scale
неверен для неизвестного PID, floating-to-integer result выходит за диапазон
`uint8_t`; поведение C++ не определено.

### N-UAP-04 — потеря точности и малых значений

16-битное raw немедленно сжимается в 8-битный cache с truncation. Теряется
точность, а несколько первых ненулевых raw levels превращаются в ноль. Telemetry
после этого уже не может показать исходный диапазон.

### Остальные проблемы

- проверяется только первый type byte; report ID, точная длина и trailing fields
  не валидируются;
- `Fn` (`FF05`) распознаётся, но отбрасывается по UAP-G01;
- cache обновляет одну клавишу на report и зависит от обязательного release event;
  потерянный release не имеет sequence/full-snapshot механизма восстановления;
- после каждого одиночного события сканируется весь Soup key cache и заново
  строится vector всех активных клавиш;
- один пустой read немедленно отключает устройство.

До исправления bounds/scale и admission нельзя считать “все NuPhy” стабильной
production-поддержкой.

## Madlions

Эта ветка уже значительно безопаснее исходной благодаря HallJoy SafeHID patch:

- persistent thread-local events;
- timeout 100 мс;
- cancel-and-drain OVERLAPPED lifetime;
- stale queue ограничена 32 reports;
- response короче 27 bytes отвергается;
- final partial layout chunk проверяет границы;
- raw travel clamp `0..350`.

### M-UAP-01 — Fn/Menu

MAD60 table содержит `KEY_CTX` и `KEY_FN`; оба теряются по UAP-G01/G02. MAD68
содержит `KEY_FN`, который также теряется.

### M-UAP-02 — недостаточная проверка ответа

После проверки `size >= 27` не валидируются report ID, echo команды
`02 96 1C`, requested offset, status и metadata четырёх records. Первый достаточно
длинный queued response принимается как нужный chunk.

### M-UAP-03 — persistent failure позднего chunk никогда не достигает порога

`consecutive_failed_reports` сбрасывается в ноль после каждого успешного chunk.
Если один и тот же поздний chunk постоянно повреждён, предыдущий chunk каждого
цикла успевает сбросить счётчик, а сбой повышает его только до единицы. Порог `8`
для controlled disconnect/restart никогда не достигается.

При этом worker продолжает публиковать частичный snapshot, `telemetry_last_update`
обновляется, а host stale detector не срабатывает. Backoff видит ошибку, но не
лечит session.

### M-UAP-04 — polling cost

При выключенном digital assist background scan независимо от бинарной точки
покрывает одну группу из 16 matrix positions за цикл и всю раскладку за пять
циклов. Каждый цикл — до четырёх HID transactions плюс chunks уже активных keys.
При большом rollover число transactions приближается к числу активных 4-key
chunks.

### Остальные ограничения

- static `MadlionsMtx` лучше, чем handle на каждом poll, но wait всё ещё
  `INFINITE`, результат не проверяется и shutdown не может его отменить;
- MAD68R получает общую MAD68HE table с комментарием “sorta the same”, без
  отдельного model proof;
- detection не проверяет report lengths/capability response до публикации
  устройства;
- clamp выше `350` обеспечивает безопасность, но скрывает raw protocol anomaly.

## Сравнительная оценка

| Семейство | Обычные клавиши | Special keys | Memory/parser safety | Transport | Layout confidence |
|---|---|---|---|---|---|
| Wooting | высокая | сломаны | высокая | хорошая stream cancellation | высокая для стандартных HID |
| Razer | средняя/высокая | Fn/Menu сломаны | bounds-safe records | report ID не проверяется | vendor map, hardware proof нужен |
| DrunkDeer | низкая для вариантов | сломаны | критические OOB | unbounded | одна непроверенная table |
| Keychron/Lemokey custom | средняя | сломаны | критические OOB на short report | unbounded | ISO/JIS неполны |
| Keychron official | средняя с latency | сломаны | критические OOB | множество unbounded transactions | ISO/JIS неполны |
| NuPhy | не подтверждена для всех PID | Fn сломан | undefined behavior на short/out-of-range | stream | admission слишком широкий |
| Madlions | средняя/высокая | сломаны | существенно усилена | bounded SafeHID | MAD68R не доказан отдельно |

## Приоритет исправлений

### P0 — до заявления «стабильно для всех клавиатур»

1. Исправить `KEY_CTX <-> HID_CONTEXT_MENU (0x65)` и запретить публикацию code `0`.
2. Спроектировать versioned key domain для `Fn`, media и OEM; не маскировать
   проблему простым silent drop `>=256`.
3. Исправить NuPhy: exact record length, проверка каждого read, initialized fields,
   raw clamp/range telemetry и model/capability admission.
4. Исправить Keychron: минимальные длины, exact packet correlation/order,
   bounded transport, RAII mutex и отдельные ISO/JIS layouts.
5. Вернуть безопасный hotplug либо перезапускать/enumerate child по реальному HID
   arrival/removal с truthful connected flags.
6. После лога G65 заменить DrunkDeer на проверенный native backend.

### P1

7. Исправить Madlions failure counter на transaction/cycle/session уровень и
   валидировать response correlation.
8. Усилить Razer interface/report-ID admission и отслеживание Synapse lifecycle.
9. Убрать per-poll named-handle/event/Buffer churn; использовать persistent
   transport contexts и заранее выделенные snapshots.
10. Разделить release state по device ID и нейтрализовать snapshot в каждом
    exception path.

### P2 — физическая квалификация

11. Проверить каждую заявленную модель/variant реальным hardware trace:
    matrix coverage, все special keys, minimum/maximum raw, rollover, report rate,
    unplug/reconnect и shutdown.
12. Сузить README claims до реально доказанных профилей; “Everything by NuPhy” и
    “Everything by DrunkDeer” оставить только после family capability proof.

## Что не следует делать

- Нельзя восстанавливать analog layout по последнему binary key event.
- Нельзя считать product name достаточным capability proof.
- Нельзя silently clamp malformed raw и объявлять frame успешным.
- Нельзя назначать ISO/JIS ANSI-таблицу без отдельного physical/protocol proof.
- Нельзя исправлять Fn случайным стандартным HID usage: у Fn нет универсального
  USB Keyboard usage, поэтому нужен явный внутренний domain.

## Основные исходники

- `third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/AnalogueKeyboard.cpp`;
- `third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/AnalogueKeyboard.hpp`;
- `third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/hwHid.cpp`;
- `third_party/UniversalAnalogPluginFixed/Soup/soup/HidScancode.hpp`;
- `third_party/UniversalAnalogPluginFixed/Soup/soup/Buffer.hpp`;
- `third_party/UniversalAnalogPluginFixed/Soup/soup/NamedMutex.hpp`;
- `third_party/UniversalAnalogPluginFixed/main.cpp`;
- `third_party/UniversalAnalogPluginFixed/halljoy_dense_snapshot.h`;
- `src/HallJoyProject/HallJoy/analog_host_client.cpp`;
- `src/HallJoyProject/HallJoy/analog_host_shared.h`.
