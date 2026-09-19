# IROK / IYX: результаты перед аппаратной проверкой

> После ревью внесены [исправления ND75](IROK_ND75_IMPLEMENTATION_FIXES_2026-09-13.md):
> wire opcode и проверка диапазона исправлены. Полнота состояния остаётся
> нерешённой; актуальные проверки сборки перечислены в новом документе.


> Актуальное уточнение 2026-09-13: [ревью ND75](IROK_ND75_REVIEW_2026-09-13.md).
> Найдены ошибки wire opcode и публикации состояния в experimental backend;
> исправлена одна serializer fixture. Прежний вывод об отсутствии любых
> альтернативных чтений слишком широк: сохранён частичный результат52/81.
> Pro заморожен; эксперименты вне штатной таблицы настроек приостановлены
> по последнему согласованному направлению, продолжается обычное ревью.


2026-09-13. Продолжение по просьбе владельца до конкретного блокера.
Область выполненной работы: firmware/SDK reverse, offline emulation,
исследовательский парсер и документация. HallJoy runtime, README проекта,
релизы и аппаратные устройства не менялись. Поддержка моделей не объявлена.

## На чём остановлено продвижение

| Ветка | Что установлено | Что блокирует следующий вывод |
|---|---|---|
| Обычная NA87 Mag / обычная MU68 | Точные SDK hidName GK8260HERGB / GK8152HERGB; M484, VID/PID 0416:7372; SDK переводит 0x29 в wire 0x21 | Нет точного firmware этих моделей или диагностического capture. Нельзя переносить наличие/отсутствие функций ND75 или Pro |
| ND75, известный V1.00.09 образ | Полный scanner теряет события и release через единственную pending-позицию; альтернативные исследованные reads не возвращают массив глубин | Известный stream не обеспечивает надёжный полный аналог. Для устранения нужны другой firmware/protocol path либо изменение прошивки; аппаратный тест не исправит доказанную структуру |
| NA87 Pro / MU68 Pro / ND63 / Cyan, десять образов | Полные массивы, serializer execution, карты датчиков, USB-дескрипторы, raw ranges, offline parser | Реальные USB delivery/timing, таймауты, поздние ответы, совместимость с обычным вводом и реальная калибровка. В firmware найдены потери фрагментов |
| Ultra / Polar75 | Видны отдельные классы/карты в официальном веб-драйвере | Точных firmware нет; не объявлять результаты Pro доказательством их поддержки |

Это не утверждение, что любая возможная статическая работа исчерпана.
Закрыты основные вопросы протокола и подготовлен воспроизводимый набор проверок;
оставшиеся выводы о целевой ordinary NA87 и качестве доставки Pro требуют
нового внешнего материала, которого сейчас нет.

## Обычная ветка: читать исправление протокола

[Witmod offline closure](IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md) содержит
хеши, адреса и границы доказательств. Критическое исправление:
**SDK input 0x29 -> HID wire opcode 0x21**, report ID 1, либо 6 для col06.

В существующем отключённом экспериментальном коде
`src/HallJoyProject/HallJoy/irok_nd75_protocol.h:15` всё ещё
`kHostAnalogCommand=0x29`; builders используют его, backend отправляет raw HID,
а unit tests закрепляют 0x29. Они не подтверждают правильность wire протокола.
Это известный дефект исследовательской реализации; он не исправлен в runtime
в рамках firmware reverse. Одной заменой opcode проблема потерь ND75 не решается.
Не включать backend на основании старых PASS.

## Pro: подтверждённый транспорт

Машиночитаемая таблица всех десяти образов, хешей, offsets, endpoints и карт:
`.local/research/irok-na87/pro-transport-maps.json`.
Её проверяет `pro_transport_maps.py` по исходным firmware и pinned web bundle
SHA256 `2b80dd9398565e13ebf315351af96c3bfcf059fa917a774638b0be42c69c44dc`.

| Ревизии | VID:PID в device descriptor |
|---|---|
| Все полученные 1.0.3 / 1.0.5 | 1C4F:EE88 |
| NA87 Pro 1.0.8 | 1CA2:0401 |
| MU68 Pro 1.0.8 и Cyan 1.1.4 | 1CA2:0402 |
| ND63 1.0.8 и Cyan 1.1.4 | 1CA2:0406 |

Во всех десяти config templates: interface0 boot keyboard IN81 /8 bytes;
interface1 composite keyboard/media IN82 /64 bytes;
interface2 vendor IN83 и OUT04 /64 bytes. Vendor report descriptor:
usage page FFA0, usage1; 512-bit input и output, **без report-ID item**.
На уровне WebHID это report ID 0 и отдельный 64-byte payload. Дескриптор не
означает, что Windows API принимает 64-byte buffer без ведущего report-ID byte:
в будущем получить HIDP_CAPS и соблюдать контракт конкретного API (обычно
65-byte buffer с нулевым первым байтом для такого устройства).
Это не измеренный Windows capture.

NA87 Pro 1.0.8 file offsets: device 0x14958; vendor HID report 0x14978;
config template 0x15e78. GET_DESCRIPTOR handler 0x2bd6..0x2be4 выбирает
vendor descriptor runtime0x18978 длиной0x36 для interface2.
Configuration возвращается из RAM0x20000040; интервал endpoint1 ms — значение
шаблона, не обещание фактической частоты при всех настройках.

Прямой numeric secondaryProductId в веб-каталоге — часть составного
идентификатора модели приложения, не дополнительное поле USB descriptor.
`vn/zc` сопоставляет VID/PID и productName; Cyan разделяется именем модели.
VID/PID недостаточно: старые модели делят одну пару, Cyan делит новые пары.

## Pro: точные позиции датчиков и host key map

NA871.0.8 mapper 0xde3c..0xde96 читает 96-byte table runtime0x5554
(file0x1554), пропускает FF, декодирует row=v>>5,col=v&31 и записывает
ADC по slot=row*21+col. Затем 0xdeaa вызывает depth producer0xd34e.
В старых образах такая таблица лежит file0x1564; во всех десяти таблицы
проверены на уникальные координаты, границы и соответствие официальной карте.
Полный control-flow mapper прослежен для NA871.0.8, остальные — структурная
сверка таблиц, не whole-program proof каждой ревизии.

Карты задают 87/68/64 занятых позиций для NA87/MU68/ND63 соответственно,
включая Fn. ND63 действительно содержит 64 позиции в этом firmware; название
модели не заменяет подсчёт. Все они разрешаются в host key map после фильтра
пустых слотов. Не использовать 126 слотов как 126 физических клавиш.

Официальный CMt объединяет шесть 64-byte reports, удаляя 6-byte header
первого и четвёртого. Padding третьего/шестого не удаляется. Поэтому во
внутренней карте DMt/Hn/rl вторая половина начинается с индекса93 вместо63.
Для корректного trimmed массива: hostIndex=slot при slot<63, иначе slot+30.
Например NA87 slot63 -> host93 -> CapsLock57; slot64 -> host94 -> A4.
JSON сохраняет оба индекса. Key code1 обозначает Fn в приложении
(`Text:"fn"`, web offset840778), а не USB ErrorRollOver.

NA87/MU68/ND63 non-Cyan raw depth max4000; Cyan max3600 из lookup tables.
Не выдавать математический диапазон за аппаратное измерение миллиметров.

## Pro: исполнение кода прошивки

`pro_riscv_emulation.py` проверяет SHA256 и исполняет реальные serializers
во всех десяти образах: type2/type6, обе половины, границы и нулевые значения.
Firmware загружается по runtime0x4000. Unicorn не поддерживает QingKe XWCHC:
его стандартный RVC decoder ошибочно исполняет эти байты как FP instructions.
Поэтому только compressed lbu/lhu/sb/sh реализованы instruction hooks согласно
LLVM21.1.8 `+m,+c,+xwchc`; bytes/PC сверяются, неизвестная vendor instruction
прерывает проверку. Ветки, циклы, обычные инструкции и checksum выполняет CPU.
Это эмуляция с явно реализованным расширением ISA, не hardware PASS.

Для NA871.0.8 дополнительно выполнен целый producer0xd34e..0xd924 с
синтетическими calibration/filter RAM и ADC. Четыре позиции0/1/63/125 дают
независимые raw2161/3079/3613/4000 после24 проходов при ADC2500/2000/1500/1100.
Затем ADC первой позиции3050 и второй1800, 32 прохода: первая глубина0,
вторая остаётся ненулевой; реальные serializers обеих половин сохраняют результат.
Остальные source=FFFF пропускаются и не затирают cached sentinel.
Fixture показывает работающий путь независимых значений и представление нуля,
а не физическую точку отпускания. Например ADC3000 при синтетическом baseline3000
давал небольшой residual30: нулевой release зависит от calibration/filter.

## Pro: ограничения доставки, которые нельзя скрывать

- Queue0x4c8e: 16 slots,15 usable; 132-byte response занимает3 slots.
  Всегда копирует64 bytes, включая60 байт вне logical frame. Эмуляция проверяет
  padding AA и ситуацию one-free-slot: первый chunk принят, два остальных потеряны.
- Consumer0x4c3c читает и удаляет chunk через0x4b08/0x4ae0 **до** send0x2542.
  Low send0x22ae проверяет endpoint busy через gp+0x2f0+endpoint и может
  вернуть0; consumer не возвращает chunk и не повторяет отправку.
  Это выполнено с synthetic enabled-but-busy endpoint3: read cursor вырос,
  endpoint остался busy, chunk потерян. Не измерена частота такой ситуации
  в реальном расписании/USB; не объявлять её неизбежной при любом polling rate.
- Reply не содержит half ID, sequence ID или timestamp. Две половины следует
  запрашивать последовательно; stale same-kind reply после timeout неотличим
  от нового по одному payload. Нужна проверенная на устройстве стратегия recovery.
- Vendor checksum покрывает только header0..2 и последний byte, с константой35h.
  Изменение среднего byte проходит checksum — тест это явно демонстрирует.
- Нет гарантии атомарного snapshot: массив копируется циклом без наблюдаемой
  блокировки producer; ни половина, ни пара половин не несут общего snapshot ID.

`pro_offline_parser.py` — чистый исследовательский декодер без USB-доступа.
Требует три полных64-byte chunks, обрезает до132, проверяет header/kind/checksum,
выделяет63 uint16LE, объединяет заранее упорядоченные половины, фильтрует
по физической карте, сохраняет zeros и проверяет depth range. 200 synthetic
полных snapshots по всем десяти metadata records прошли, включая releases,
padding, недостающие/короткие reports, другой kind, checksum и range errors.
Он намеренно не притворяется, что может восстановить отсутствующий sequence ID.

## Обычный ввод во время Pro polling: что даёт static trace

NA871.0.8 main loop отправляет normal boot report8 bytes через0x23ca
(endpoint1) из0x33a6; NKRO report14 bytes через0x23e8 (endpoint2) из0x33d0.
Флаги pending этих normal reports очищаются после успешного return.
Затем путь сходится в0x322e: command dispatcher0x4e06 вызывается0x323a,
vendor queue consumer0x4c3c —0x323e. Есть также normal-send вызовы в0xdb7c
и далее из IRQ-related пути. Vendor sender использует endpoint3 и отдельный
busy slot, поэтому сам по себе busy vendor endpoint не равен busy keyboard.

Read type2/type6 строят ответ и не посылают команду отключения клавиатуры.
Отдельные режимы/флаги могут обходить normal path (например bit28 у gp-0x6dc,
проверка0x3206..0x3212; изменение через0x1014c). В конкретном read handler
это переключение не обнаружено. Этот trace показывает сосуществующие пути,
но не доказывает отсутствие задержек/потерь у ОС при частом polling.

## Следующая аппаратная проверка — конкретные критерии

До любого опыта записать точную модель/ревизию, firmware version, productName,
VID/PID, interface/usage, HIDP_CAPS, report IDs и исходный обычный ввод.
Для ordinary NA87 сначала нужен read-only identity/capture её официального
драйвера; не отправлять Pro packets и не прошивать ND75/Pro image.

Для точного Pro образа: отдельная vendor collection, по одному запросу type2,
half1 затемhalf2, ожидание всех3 reports на каждый запрос; ADC type6 только
отдельным последовательным циклом. Сохранять timestamp каждого USB report,
число chunks, logical length, timeout/recovery и физический сценарий.
Не менять calibration/lighting/keyboard modes как условие read теста.

Проверить не менее четырёх одновременно удерживаемых клавиш в обеих половинах,
независимое движение, отпускание одной при удержании/движении других, повторное
нажатие и отсутствие зависшего ненулевого значения после ошибки/разрыва.
Начать с редкого polling; повышать частоту только по измеренным timeout/loss/
latency и обычному набору текста. Отдельно отключение/reconnect, остановка
опроса, Fn/modifiers и наблюдаемое поведение при потерянном фрагменте.
Без этих данных нельзя выбрать подтверждённый polling interval или заявить
пригодность Pro для HallJoy, даже при полном доступе к массиву глубин.

## Воспроизведение

`python .local/research/irok-na87/verify_irok_offline.py`
запускает11 offline scripts и создаёт exclusive
`offline-verification-<timestamp>.json` с code hashes, outputs и exit codes.
Зависимости: Python3.10, local Unicorn2.1.4, pefile, Capstone, LLVM21.1.8.
Исходные бинарники и большие listings остаются в .local; в релиз не включать.

Общий прогон11 scripts завершён PASS; полный журнал с хешами находится в
`.local/research/irok-na87/offline-verification-*.json` (брать последний timestamp).
Raw0x29 отдельно отвергается actual ND75 dispatcher0xfae6 без RAM changes;
wire0x21 проходит тот же dispatcher и возвращает настройки через subcommand5.
