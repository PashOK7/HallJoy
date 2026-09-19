# Witmod: продолжение проверки без устройства, 2026-09-13

> После ревью внесены [исправления ND75](IROK_ND75_IMPLEMENTATION_FIXES_2026-09-13.md):
> wire opcode и проверка диапазона исправлены. Полнота состояния остаётся
> нерешённой; актуальные проверки сборки перечислены в новом документе.


> Актуальное уточнение 2026-09-13: [ревью ND75](IROK_ND75_REVIEW_2026-09-13.md).
> Найдены ошибки wire opcode и публикации состояния в experimental backend;
> исправлена одна serializer fixture. Прежний вывод об отсутствии любых
> альтернативных чтений слишком широк: сохранён частичный результат52/81.
> Pro заморожен; эксперименты вне штатной таблицы настроек приостановлены
> по последнему согласованному направлению, продолжается обычное ревью.


## Критическое исправление: SDK opcode и HID opcode различаются

**Все прежние упоминания raw HID команды 0x29 для ND75 следует читать с этой
поправкой. 0x29 — входной формат Witmod SDK; на USB уходит 0x21.**
Это исправляет описание протокола, а не разрешает включение backend.

Источник: magnet0.dll из официального IROK Setup 2.5.6.1,
SHA256 `6ea3e44f0afb88ed7ce31e5d07386d40ed5ff4730aa5791df1355354b1f8eb86`.
Файл сохранён в `.local/research/irok-na87/sdk-2561/$_3_/apps/driver_service/dlls/`.
PE image base 0x10000000; адреса ниже — VA.

- `HidWriteBuff` 0x10008360 копирует 64 байта запроса, смотрит byte1 через
  jump table 0x10008598; для 0x29 ветка 0x10008529 выбирает 0x21.
- 0x10008544/47 записывают report ID 1 и wire opcode 0x21.
  Если путь содержит `col06`, выбирается report ID 6; основной Go путь
  connectMagnet дополнительно фильтрует `&mi_02#`. Не путать интерфейс и collection.
- 0x10008572 вызывает нижний HID write с преобразованным буфером длины 64.
- `sdk_wire_emulation.py` исполняет эту функцию для subcommand 2/3/5;
  перехвачены только allocator, strstr, HID write и free, обе ветки strstr (нет col06 / есть col06).
  Четыре fixtures: проверены все 64 байта результата и неизменность исходного буфера. PASS.
  Устройства не открываются, никакой vendor EXE не запускается.
- Firmware dispatcher ND75 0xfae6 принимает индекс <0x23; handler 0x21 в
  table 0x13664 =0xf5bf. Передача raw 0x29 не соответствует этому dispatcher.

Примеры (остальное — padding до 64 байт):

| Действие | SDK input | HID output |
|---|---|---|
| Подписка | 01 29 00 00 00 18 02 +22 masks | 01 21 00 00 00 18 02 +22 masks |
| Отписка | 01 29 00 00 00 18 03 | 01 21 00 00 00 18 03 |
| Чтение настроек клавиши | 01 29 ... 18 05 row col | 01 21 ... 18 05 row col |

Go service SHA256
`97d53f6447c6f5203436e2306b9c1321d1b05b473a1cd069b7104542754f2004`:
SetKeysWatch 0x7beb30 и helper 0x7be7d0 строят 22 маски column -> bit(row),
6 строк; key=(row<<8)|column. `go_watch_emulation.py` исполняет реальный builder,
пропускает только Go stack guards и перехватывает SendData. Три fixture PASS.
Пустой список посылает subcommand 2 с нулевыми масками, не subcommand 3.
Вывод этого теста — SDK input, не USB capture.

## Точная идентификация обычных моделей в SDK

Встроенный JSON DLL, file offsets около 0x28e0d и 0x28f4a..0x291ba:

| Модель | nucNum | hidName | keyNum |
|---|---|---|---|
| ND75 | M484 XXX | X86HERGB | 82 |
| MU68 | M484 XXX | GK8152HERGB | 68 |
| NA87 | M484 XXX | GK8260HERGB | 87 |

supportHidList задаёт M484 VID 0416, PID 7372. Это запись производителя,
не результат опроса реальной клавиатуры и не доказательство одинаковой firmware.
GetHidAndFirmwareInfoStu 0x100071b0 строит 01 0D, отправляет 64 байта,
ждёт 250 ms, проверяет reply byte1=0D, разбирает строку с byte6 по запятым.
ND75 образ содержит M484,01,KB,ABT,X86HERGB,V1.00.09.
FirmwareUpgrade 0x10006d40 читает переданный вызывающим кодом локальный файл
через fopen/fread/fclose; нового firmware download endpoint здесь не обнаружено.
Точного образа GK8260HERGB/GK8152HERGB пока нет.

## ND75: потеря событий в полном сканере

Проверенный образ SHA256
`a5168399caca4bbd2c04a1e988f478265364577a46994199a73848abf0ab735f`.
`nd75_full_scan_emulation.py` исполняет целиком scanner 0x3190 с его
фильтрами и преобразованием ADC, затем реальный scheduler 0xfafc.
Единственная заглушка внутри scanner — начальный wait 0x72a6 возвращает ready.
RAM синтетическая; данные инициализации копируются из flash 0x1fe80 длиной
0xd54, согласно scatter table 0x138e0. Это обычное копирование, не сжатие.
Bootloader и приложение имеют разные layouts; app vector расположен 0x2000.
Координаты ptr+0x11 назначаются согласно реальному initializer 0xe6de..0xe6e6.

Физические позиции (0,0),(1,0),(2,0),(3,0) соответствуют logical (0,0)..(3,0).
При baseline 400 и ADC 385/375/360/350 после прогрева фильтров все четыре
изменения 16/22/29/31 принимаются **в одном проходе**. Выходит только
`01 21 00 00 00 03 01 03 00 1f`; pending bit22 очищается.
При ADC 400/365/360/350 один проход принимает release (0,0,0) и (1,0,27),
но выходит только `01 21 00 00 00 03 01 01 00 1b`.
Cache первой клавиши действительно равен нулю, pending уже очищен.
Assertions проверяют совместное изменение за один проход и потерю release.

Регистрация задач: descriptors RAM 0x20000098 matrix(priority1),
0x20000030 main(priority3),0x20000050 idle(priority6).
Регистратор 0x70b8 сортирует по priority; scheduler 0x7464 вызывает task
и получает управление после возврата. Idle вызывает USB scheduler в 0x25aa.
0x72a6 при ожидании помечает task неготовой и возвращает код, не делает
контекстное переключение посередине scanner. SysTick 0x7584 обновляет
ready/tick. Это подкрепляет достижимость последовательности scanner -> USB,
но не является измерением частоты/таймингов периферии на железе.

## Альтернативные serializer sources

`nd75_serializer_coverage.py`: 38 fixtures для всех 20 ненулевых функций
в 24-slot scheduler table 0x136f0. В трассируемых fixtures только bit22 читает
live cache 0x20004110..0x20004193. Остальные пути — настройки, keycodes,
lighting, calibration flags или ACK. Подробности в nd75-serializer-coverage.json.
Это охват всех serializer entry points с выбранными состояниями, не формальное
доказательство для всех возможных RAM states.

Особенность Unicorn: MEM_READ hook меняет выполнение IT-последовательности
state21/8 и вызывает зацикливание. Эта fixture выполняется без read hook;
её источник проверен статически: 0xf920..0xf9a6 читает calibration ptr+0x12,
а не depth. Не выдавать этот сбой инструмента за зависание прошивки.
`nd75_read_path_emulation.py` отдельно проверяет config subcommand5 и
шесть строк command0x10 при изменении всех cached depths — ответы неизменны.

## Практический предел обычной ветки

Для ND75 известный event stream теряет изменения/отпускания; backend нельзя
считать надёжным источником независимых глубин. Найденные другие чтения не
дают альтернативного полного массива. Для обычной NA87/MU68 нужен точный
образ или диагностический capture соответствующей модели: ND75 не доказывает
наличие или отсутствие такого пути у них. Протокол Pro переносить нельзя.
Runtime, README проекта и релизы не менялись. Все PASS здесь offline.

Все38 serializer fixtures дополнительно сверены с control execution без hooks:
полный RAM и return value совпадают. В existing disabled runtime остаётся
kHostAnalogCommand=0x29; его tests закрепляют SDK opcode вместо wire opcode.
Актуальный итог: [IROK_PRE_HARDWARE_STATUS_2026-09-13.md](IROK_PRE_HARDWARE_STATUS_2026-09-13.md).

Дополнительно `nd75_read_path_emulation.py` теперь входит через actual outer
dispatcher0xfae6: raw0x29 отвергается без RAM writes, wire0x21 доходит до
config subcommand5 и даёт прежние config-only ответы. USB peripheral не моделируется.
