# Аудит всех native-реализаций аналоговых клавиатур HallJoy

## 2026-09-06 — Owner clarification: preserve automatic Sayo letter mapping

The owner requires automatic user-configured letters with no manual assignment,
setup wizard or mandatory confirmation, and reports no complaints about current
behavior. This supersedes earlier blanket prohibitions on Sayo letter learning:
physical depth remains measured independently; automatic letter association is
intentional. RM-03 now preserves it and tests ambiguous correlation (addedCount==1
already guards multiple new HID keys in one report; cross-report candidates need
review). No per-press activation-threshold delay was established. Treat stale-depth
fallback separately according to its intended digital/analog contract. Do not
remove Sayo, replace letters with physical-only controls, or label learning itself
P0. Reconcile old HJ-V14-P0-003 subclaims rather than copying its blanket verdict.
See FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md v1.1, section 0, for other potential
intent traps: fallback/shadow removal, single-instance, async save, process
isolation and release scope. Proposed rewrites require Purpose/compatibility
review and evidence; product behavior changes require an explicit product decision.
Only documentation changed in this clarification; no new runtime PASS is claimed.


Дата: 20 августа 2026 года.

Статус: полный статический аудит production-каталога native backend на Windows
x64. Исправления не выполнялись. Физическая корректность не объявляется без
соответствующей клавиатуры и отдельного аппаратного теста. Git не использовался.

Связанный аудит UAP/Soup:
[`UAP_ALL_KEYBOARDS_AUDIT.md`](UAP_ALL_KEYBOARDS_AUDIT.md).

## Краткий итог

Предыдущий cross-family аудит полностью разобрал встроенный UAP/Soup, но не
семь production-native маршрутов. В native-коде обнаружены самостоятельные
ошибки, которые нельзя считать закрытыми исправлением UAP:

| Приоритет | Маршрут | Статический вывод |
|---|---|---|
| P0 | SayoDevice | физический аналоговый индекс привязывается к HID по последующему бинарному keyboard report; до этого используются ложные F/G/H, а при несвежей глубине создаётся искусственное значение 1000 |
| P0 | Aula W669 | после подписки backend не имеет тайм-аута живости live-event потока; пропавший stream или потерянный release могут навсегда оставить `connected=true` и ненулевую глубину |
| P1 | Hex80 | chunks публикуются немедленно, а один постоянно сломанный chunk не накапливает failure streak, если остальные chunks исправны; старое значение этого chunk может остаться навсегда |
| P1 | Addressed `09/94/02` | при неполной/отсутствующей карте применяется QBZ canonical layout без ограничения по бренду или VID/PID; protocol proof не доказывает совпадение физической раскладки |
| P1 | SparkLink | коды карты выше 255 теряются, discovery может принять усечённую карту, duplicate HID перезаписывается последней строкой, шкала жёстко предполагается в диапазоне 3000..5000 |
| P1 | Все native | ABI принимает `uint16_t`, но фактический key domain почти везде ограничен массивами 256; Fn/OEM/media либо теряются, либо кодируются неуниверсальным псевдо-HID |
| P2 | MAD68 / Hex80 siblings | fixed physical maps применяются к protocol-compatible sibling PID/firmware без отдельного доказательства идентичной матрицы |

Самый важный вывод: Sayo нарушает базовое правило проекта «не определять
идентичность аналоговой клавиши по бинарному нажатию». Глубокая точка цифровой
активации делает такой алгоритм принципиально неспособным правильно назвать
раннюю часть аналогового хода.

## Что именно входит в production

Единый каталог `src/HallJoyProject/HallJoy/native_analog_backends.def` включает:

1. MAD68 Pro R;
2. Hex80;
3. Addressed Analog `09/94/02`;
4. Aula WIN60 HE MAX / совместимое 6x21 family;
5. Aula W669 family;
6. SparkLink / XD row protocol;
7. SayoDevice depth protocol.

IROK ND75 включается только с `HALLJOY_IROK_ND75_EXPERIMENTAL`, DrunkDeer
native — только с `HALLJOY_DRUNKDEER_DIAGNOSTIC`. Они не являются частью
обычного production-каталога и не расширяют выводы этого документа.

Основные проверенные исходники:

- `native_analog_backend.h`, `native_analog_backend_registry.cpp` и
  `native_analog_backends.def`;
- `backend_sayo.inc` и Sayo descriptor в `backend.cpp`;
- `backend_sparklink.inc` и SparkLink descriptor в `backend.cpp`;
- `aula_w669_backend.cpp` и `aula_w669_protocol.cpp`;
- `aula_win60he_backend.cpp`, `aula_win60he_client.cpp` и
  `aula_win60he_protocol.cpp`;
- `hex80_backend.cpp`, `hex80_protocol.cpp` и `hex80_protocol.h`;
- `addressed_analog_backend.cpp` и `addressed_poll_scheduler.cpp`;
- `mad68pr_backend.cpp` и `mad68pr_protocol.h`.

## Общие проблемы native-архитектуры

### NAT-G01 — фактический key domain меньше заявленного ABI (P1)

`NativeAnalogBackendDescriptor::ownsHid/getMilli` принимают `uint16_t`, но все
текущие реализации используют массивы на 256 элементов и отбрасывают значения
`>=256`. Поэтому проблема UAP с Fn/OEM/media не ограничена UAP:

- MAD68 публикует 67 из 68 физических клавиш; Fn имеет `hid=0`;
- Hex80 явно превращает vendor Fn `0x409` в нулевой slot;
- SparkLink превращает любой layout code `>0x00FF` в `0`;
- Addressed игнорирует и `keyId`, и `hid` `>=256`;
- Aula MAX публикует только keyboard-page usages `0x04..0xE7`;
- W669 использует `0xFA` как локальный псевдо-HID Fn, хотя это не универсальное
  представление клавиши и для него не найден отдельный UI/layout contract.

Нужно проектировать один версионированный key domain для UAP и native, а не
чинить специальные клавиши отдельно в каждом backend.

### NAT-G02 — protocol proof не всегда доказывает physical layout (P1)

Проверка корректного ответа команды доказывает семейство wire protocol, но не
обязательно соответствие `physical position -> key`. Это особенно опасно там,
где после динамического admission применяется fixed/canonical map:

- Hex80 проверяет scale и только первый chunk из четырёх slots, затем применяет
  одну 104-slot карту к любому прошедшему PID;
- Addressed при менее чем 20 dynamic entries применяет QBZ canonical map;
- MAD68 разрешает protocol-proven family siblings, но использует одну таблицу
  68 descriptors.

Добавление sibling должно отдельно доказывать карту, а не только opcode.

### NAT-G03 — разные определения `connected` и freshness (P1)

Маршруты используют несовместимые критерии:

- Aula MAX разрушает session при первой ошибке полного matrix read;
- Addressed требует глобальный и per-HID sample моложе 500 ms;
- SparkLink перезапускается после тишины;
- Hex80 остаётся connected при частичной успешности chunks;
- W669 не отсоединяется от бесконечных timeout/error reads;
- Sayo считает живым любой packet от любого выбранного interface, не только
  корректный depth response.

Для stream и chunked matrix нужен общий контракт: свежесть должна следовать из
валидного аналогового поколения/кадра, а не из наличия handle или постороннего
пакета.

### NAT-G04 — duplicate HID должен иметь определённую семантику (P1)

Если две физические позиции назначены на один HID, корректная итоговая глубина
обычно равна максимуму двух позиций. Aula MAX это делает. W669, SparkLink и
частично Addressed записывают последнее обработанное значение, поэтому release
одной позиции может обнулить вторую, которая всё ещё нажата.

### NAT-G05 — max aggregation скрывает конфликт маршрутов (P2)

Общий registry читает все connected backend и возвращает максимум по одному
HID. Это разумно для нескольких физических устройств, но не выявляет ошибочную
двойную ownership: неверно подключившийся backend может доминировать над
правильным. Exact interface-path claims предотвращают двойное открытие одного
HID interface, но не конфликт двух разных interfaces/backend, публикующих один
HID.

## SayoDevice depth

### NAT-SAYO-01 — идентичность аналога выводится из бинарного события (P0)

`backend_sayo.inc`:

- строки 126–130 заранее назначают физические индексы 0/1/2 клавишам F/G/H;
- строки 167–193 принимают analog edge с physical index;
- строки 202–251 разбирают обычный boot-keyboard report и, если в течение
  80 ms появилась ровно одна цифровая клавиша, переназначают pending analog
  index на её HID;
- modifier byte не участвует, NKRO не поддерживается, одновременное появление
  двух HID не обучает карту.

Следствия:

- до первого цифрового срабатывания ранний аналог ошибочно принадлежит F/G/H;
- при глубокой activation point HallJoy не знает настоящую клавишу на значимой
  части хода;
- modifiers и одновременные нажатия не могут надёжно обучиться;
- сохранённая карта зависит от порядка прошлых цифровых событий, а не от
  firmware key map.

Это не временная диагностика, а production-native descriptor `sayo-depth`.
Алгоритм нельзя сохранять как fallback. Нужна firmware/protocol-команда карты
физических индексов либо точная model-specific карта, доказанная для каждого
поддерживаемого PID.

### NAT-SAYO-02 — бинарный edge создаёт искусственный полный аналог (P0)

Если depth packet старше 160 ms, `SayoSetIndexState` на digital-like analog edge
записывает `1000`. Это превращает отсутствие свежего измерения в полный ход и
противоречит контракту, по которому цифровой сигнал не должен становиться
сфабрикованной аналоговой глубиной.

### NAT-SAYO-03 — admission и шкала не согласованы (P1)

- известный PID `8089:0009` выбирается без обязательного depth proof;
- неизвестный PID того же VID принимается по одному ответу `0x22`, в котором
  три значения лишь должны быть `<=8000`;
- runtime всегда нормализует относительно 4000 и насыщает всё выше 4000.

Таким образом, sibling со шкалой 8000 может пройти proof, но половина его шкалы
будет представлена как полный ход.

### NAT-SAYO-04 — живость подтверждает любой packet (P1)

Все interfaces выбранного PID с `inputLen>=8` открываются как readers, а
`g_sayoLastPacketMs` обновляется до определения типа packet. Keyboard report
или неизвестный report может удерживать connected, даже если depth interface
перестал отвечать. Несколько write-capable 1024-byte interfaces также могут
одновременно отправлять один depth poll каждые 8 ms.

## Aula W669 family

### NAT-W669-01 — бесконечно stale stream (P0)

После успешной отправки subscription backend сразу ставит `connected=true`.
Основной цикл:

- читает с timeout 100 ms;
- timeout игнорирует;
- прочую read error только считает;
- не имеет silence deadline или failure threshold;
- обновляет глубину только по subtype `01` live event;
- snapshot recovery намеренно отключён.

Если stream прекратился или release event потерян, session продолжает владеть
ранее увиденным HID, а последнее ненулевое значение остаётся опубликованным до
остановки процесса/worker. Это может удерживать ось или кнопку бесконечно.

### NAT-W669-02 — Fn представлен непереносимым псевдо-HID (P1)

Factory maps WIN60/WIN68 содержат Fn как `0xFA`, и `Publish()` принимает любой
ненулевой byte. В отличие от MAD68/Hex80 Fn не отбрасывается на transport
уровне, но `0xFA` не является общим HallJoy key identity contract. Отдельного
названного layout/UI mapping для 250 в production-коде не найдено. Поэтому
наличие analog event ещё не означает, что пользователь может увидеть и
назначить Fn корректно.

### NAT-W669-03 — duplicate mapping перезаписывает held key (P1)

Dynamic key map не проверяется на уникальность HID. Каждое событие пишет
`g_milli[hid]` напрямую. При двух физических клавишах с одним назначением
release последней обработанной позиции может дать ноль, пока другая удержана.

### Сильные стороны W669

Identity, travel scale и все 10 fragments карты проверяются до admission.
Известные firmware product strings выбирают отдельные factory maps; неизвестная
модель с all-zero inherited map не получает произвольный factory fallback.
Это заметно безопаснее Addressed canonical fallback.

## Hex80

### NAT-HEX80-01 — permanent failed chunk не разрывает session (P1)

104 slots читаются chunks по четыре. При валидном chunk
`consecutiveFailures=0`. Поэтому сценарий «каждый цикл исправны chunks 0..N,
один поздний chunk всегда сломан» никогда не достигает лимита восьми ошибок.
Backend остаётся connected.

### NAT-HEX80-02 — torn generations и вечные старые значения (P1)

Каждый валидный chunk публикуется немедленно. Полный matrix frame не staging-
буферизуется. При отказе chunk уже опубликованные части принадлежат новому
поколению, а отказавшая часть сохраняет старые значения. `completeCycle`
управляет только telemetry counter и не откатывает публикацию. В сочетании с
NAT-HEX80-01 старая нажатая клавиша может остаться ненулевой навсегда.

### NAT-HEX80-03 — fixed map шире доказанного layout (P1)

Admission требует VID `373B`, `FF60:0061`, report sizes, корректный travel scale
и корректный chunk slots 0..3. PID не ограничен. После этого применяется одна
104-slot таблица и ownership объявляется для всех её HID, в том числе ещё не
наблюдавшихся. Proof wire protocol не доказывает физическую карту всех slots.

### NAT-HEX80-04 — Fn теряется (P1)

Slot 95 имеет vendor code `0x409`, но fixed map сохраняет его как 0, потому что
HallJoy ограничен HID `<256`.

### Сильные стороны Hex80

Decoder проверяет operation/subcommand, exact returned offset/size, длину,
scale и plausible travel. SET calibration-finish отправляется только после
повторного GET-only proof на текущем handle.

## Addressed Analog `09/94/02`

### NAT-ADDR-01 — небезопасный canonical fallback (P1)

Кандидат не ограничен брендом/VID. При наличии менее 20 dynamic map entries
`BuildProfile()` применяет всю QBZ canonical table, возможно с partial
overrides. Admission при этом проверяет лишь минимум два адреса, найденных для
W/A/S/D, и один корректно коррелированный `09/94/02` response.

Устройство с тем же wire protocol, но другой physical matrix, может быть
уверенно принято и получить правдоподобную, но неверную раскладку. Fallback
должен быть привязан к доказанной firmware identity; иначе требуется достаточно
полная device map и fail-closed.

### NAT-ADDR-02 — special key domain и partial-map loss (P1)

Map parser отбрасывает `keyId>=256` и `hid>=256`. При partial map менее 20
entries новые key IDs вне canonical table также не добавляются в профиль.

### Сильные стороны Addressed

Runtime response коррелируется с exact pending key IDs/count, rejects duplicate
IDs и malformed length. Connected требует response моложе 500 ms, а ownership
отдельного HID — sample моложе 500 ms. Scheduler работает с физическими IDs и
не выводит identity из бинарных keyboard events.

## SparkLink / XD row protocol

### NAT-SPARK-01 — extended key codes теряются (P1)

Layout code `>0x00FF` превращается в 0. Fn/media/OEM не входят в native output.

### NAT-SPARK-02 — layout discovery может быть усечён (P1)

Discovery прекращается:

- при первой ошибке чтения row;
- после двух пустых rows вслед за первой активной областью.

Успешные предыдущие rows при этом принимаются. Непрерывность карты и полный
конец layout firmware не доказаны. Поздние или временно не прочитанные rows
исчезают из ownership.

### NAT-SPARK-03 — duplicate HID имеет last-row-wins семантику (P1)

Уникальность layout codes не проверяется. Две позиции пишут один
`g_sparkAnalogMilli[hid]`; последняя опрошенная row перезаписывает первую вместо
агрегации максимумом.

### NAT-SPARK-04 — шкала угадана, а не получена из firmware (P1)

Наблюдаемый максимум для каждого HID принудительно зажимается в 3000..5000 и
изначально фактически равен 3000. У sibling с полным ходом ниже 3000 значение
никогда не достигнет 1000; выше 5000 — преждевременно насытится. Capability
proof не читает scale.

### NAT-SPARK-05 — успех соседней row скрывает stale failed row (P1)

Worker имеет один `failStreak` и один `g_sparkLastPacketMs` на всю matrix. После
ошибки row индекс уже сдвинут; успешная соседняя row в следующей итерации
сбрасывает streak в ноль и обновляет общий freshness. Поэтому одна постоянно
не читаемая active row не обязана разрушить session и может бесконечно хранить
предыдущие ненулевые `g_sparkAnalogMilli`.

Физические IROK-логи от 2026-08-21 не воспроизвели этот defect: обе сессии
имели `route_fail=0`. Это отдельное статически доказанное lost-release окно,
которое требует per-row/whole-cycle freshness, neutralization и old-bug oracle.
Риск: `HJ-V14-P1-039`.

### Сильные стороны SparkLink

Vendor-page fingerprint сам по себе не считается proof: до exact-path claim
выполняется device-info transaction. Runtime имеет failure streak и watchdog
тишины, а realtime loop будится только при фактическом изменении row.

## MAD68 Pro R

### NAT-MAD68-01 — Fn намеренно не публикуется (P1)

Таблица содержит 68 физических descriptors, но `kPublishedKeyCount=67`.
Descriptor Fn имеет известные scanner/internal bytes, однако `hid=0`, поэтому
его аналоговая глубина теряется.

### NAT-MAD68-02 — sibling protocol proof не равен layout proof (P2)

Точная пара `373B:1109`, firmware `0102` имеет отдельный audited path. Для
другой firmware/PID того же семейства допустим dynamic protocol proof, после
которого всё равно применяется одна 68-key descriptor table. Перед публичным
claim sibling требуется отдельное подтверждение карты.

### Важное отличие от Sayo

MAD68 использует Raw Input только для freshness arbitration: после цифрового
edge временно перестаёт доверять старому analog sample. Идентичность analog
descriptor известна из protocol table и не обучается по бинарной клавише. Это
не нарушает запрет на binary-derived analog identity.

## Aula WIN60 HE MAX / 6x21 family

Статический аудит не нашёл открытого P0 в основном data path.

Сильные свойства:

- brand/VID prefilter до metadata open;
- exact `FFA0:0001` и 65-byte Windows envelope;
- exclusive same-handle capability proof и polling;
- sync, precision, вся default map, двойное идентичное чтение active Fn0 map и
  начальная полная travel matrix до connected;
- duplicate default keys отклоняются;
- active duplicate HID корректно агрегируется максимумом по полной matrix;
- любая ошибка полного travel read разрушает session до следующей публикации;
- ambiguous candidates fail closed;
- active map периодически перечитывается двумя поколениями.

Ограничение NAT-G01 остаётся: active function публикуется только если умещается
в byte и является keyboard usage `0x04..0xE7`; Fn/OEM/media за пределами этого
домена невидимы.

## Что уже разобрано полностью

После этого документа статически разобраны:

- весь реально встроенный Windows x64 UAP/Soup каталог;
- весь production-native каталог;
- общий native descriptor/registry/routing contract;
- admission, mapping, parser correlation, normalization, freshness,
  disconnect, duplicate HID и special-key поведение каждого native family.

Не разобраны несуществующие у нас firmware-команды и нельзя статически доказать
реальную электрическую шкалу, частоту, rollover или physical position map.
Следующая стадия — не ещё один общий code audit, а исправления по приоритету и
targeted hardware qualification, когда подходящее железо будет доступно.

## Рекомендуемый порядок будущих исправлений

1. Sayo: удалить binary-derived identity и искусственный 1000; найти/доказать
   physical map и привязать depth liveness к валидному `0x22`.
2. W669: ввести live-stream silence deadline, neutralization/reconnect и
   snapshot-based recovery; определить настоящий key identity для Fn.
3. Hex80: staging полного поколения, per-chunk failure accounting и fail-closed
   layout admission.
4. Общий versioned key domain UAP/native для Fn/Menu/media/OEM.
5. Addressed: убрать неглобально доказанный canonical fallback.
6. SparkLink: полное discovery, firmware scale, duplicate aggregation и
   per-row/whole-cycle freshness с neutralization failed row.
7. Отдельно квалифицировать sibling layouts MAD68/Hex80/Addressed.

До появления аналоговой клавиатуры эти пункты остаются аудитом и требованиями,
а не разрешением на неподтверждённые production-изменения.
