# IROK / IYX: handoff исследования прошивок

> Актуально: [первая сборка с реальным аналоговым вводом NA87](IROK_NA87_NATIVE_SUPPORT_2026-09-13.md).
> Поток глубин подтверждён аппаратным логом; новый backend публикует его в HallJoy.
> ANSI подтверждена владельцем. Прежний сборщик заменён непрерывным вводом,
> логирование сокращено до агрегатов. Автотесты пройдены; нужен аппаратный прогон
> нового EXE для проверки сочетаний и отпусканий.

> После локального лога: [исправления runtime](IROK_NA87_RUNTIME_FIXES_2026-09-13.md).
> Устранён дефект владения Windows debug handles, ограничены повторные логи,
> остановлен цикл перезапусков при невалидном command event. Автотесты пройдены;
> Повторный запуск16:08 завершился штатно: прежние сбои не повторились.
> Текущий EXE можно передавать NA87-тестировщику; аналог NA87 ещё не подтверждён.

> Актуально: [обычный HallJoy.exe с автоматической диагностикой NA87](IROK_NA87_HALLJOY_DIAGNOSTIC_2026-09-13.md).
> Прежний ZIP отклонён. Передавать только EXE: обычный геймпад сохранён,
> пользователь нажимает клавиши30–60 секунд, закрывает окно и присылает HallJoy.log.
> На локальном ПК Keychron; NA87 здесь нет. Работающий HallJoy владельца не закрывать.
> Полный тест instance guard/профилей здесь блокируется уже запущенным HallJoy
> (Windows error5); остальные доступные проверки и самотесты диагностики пройдены.

> Продолжение после ревью: [исправления ND75](../research/IROK_ND75_IMPLEMENTATION_FIXES_2026-09-13.md).
> Wire format и отбрасывание некорректной глубины исправлены; полнота состояния
> при потере событий остаётся нерешённой. Итоги проверок — в новом документе.


> Актуальное уточнение 2026-09-13: [ревью ND75](../research/IROK_ND75_REVIEW_2026-09-13.md).
> Найдены ошибки wire opcode и публикации состояния в experimental backend;
> исправлена одна serializer fixture. Прежний вывод об отсутствии любых
> альтернативных чтений слишком широк: сохранён частичный результат52/81.
> Pro заморожен; эксперименты вне штатной таблицы настроек приостановлены
> по последнему согласованному направлению, продолжается обычное ревью.


Обновлено 2026-09-13. Прочитать после OWNER_CONTEXT.md при продолжении в новом чате.

## Задача и последнее решение владельца

Исходная задача — получить прошивку **обычной IROK NA87 Mag**, найти аналоговый
протокол для HallJoy. Не путать с механической NA87 и NA87 Pro Mag.
Владелец разрешил искать и разбирать все родственные прошивки, просил не
останавливаться после промежуточных находок. Физической клавиатуры для этой
работы нет. Предыдущий вывод о том, что остаётся offline работа, отработан в продолжении.
Актуальные результаты и внешние блокеры перечислены в новом pre-hardware doc.
Актуальная просьба владельца: продолжать до конкретного блокера либо завершения
работ до аппаратного теста. Firmware reverse продолжен; runtime/релиз не включались.

Не выдавать сходство UI, протокола или VID/PID за доказательство совместимости.
Особенно важны независимые глубины нескольких одновременно удерживаемых клавиш,
доставка отпусканий и сохранение обычного цифрового ввода. Владелец уже
справедливо возражал против формулировок «хороший кандидат» без доказательств.
Не требовать физическую проверку вместо ещё возможного реверса.

## Что читать, не начинать поиск с нуля

1. `docs/research/IROK_NA87_RECON_2026-09-13.md` — источники, поиск, родство,
   официальный Go-драйвер, каталоги и полученные пакеты.
2. `docs/research/IROK_ND75_M484_STATIC_ANALYSIS_2026-08-17.md` — обязательно
   включая **новое продолжение 2026-09-13 с потерей событий**. Старый вывод о
   готовности экспериментального backend не является разрешением включить его.
3. `docs/research/IROK_PRO_FIRMWARE_PROTOCOL_2026-09-13.md` — десять Pro-образов,
   команды, адреса, диапазоны, очередь, производители данных, проверки.

Рабочая папка: `W:\github\HallJoy\HallJoy-main`.
Все бинарники/скрипты исследования: `.local/research/irok-na87/`.
Программу HallJoy, README, релиз и runtime-код в этой работе не меняли.
Прошивальщики не запускали, к USB-устройствам не обращались, ничего не прошивали.
Не распространять скачанные vendor binaries вместе с HallJoy.
Команда `git status` в HallJoy-main вернула «not a git repository»; не пытаться
инициализировать Git или публиковать исследование без отдельной задачи.

## Родство: две разные ветки

В живом `https://hid.irok.cn/` официальном bundle:

- **Witmod:** обычная IROK NA87, IROK ND75, обычная IYX MU68.
- **JingTaiV1Base:** NA87 Pro, MU68 Pro/Cyan/Ultra, ND63/Cyan, IYX Polar75.

NA87 Pro наследует UI-класс обычной NA87, но это геометрия/интерфейс, не MCU.
Обычная магнитная ветка desktop-driver ищет 0416:7372.
Pro web filters: 1C4F:EE88, secondaryProductId 32, usage FFA0:0001,
replacement 1CA2:0401. Не строить окончательный admission только по этой записи.
**Для обычной NA87 релевантнее ND75, а не Pro.** Перекос внимания на Pro уже
вызвал вопрос владельца. Продолжать обе ветки, приоритет исходной задаче.

## Полученные файлы

### ND75

`ND75_Firmware_Upgrade_V12.exe`, 5,344,536 байт, действительная Authenticode.
Официальный URL:
https://s.joyway.net/packages/irok/software/install/ND75_Firmware_Upgrade_V12.exe

`extract_nd75.py` выделил 0x80000 байт с file offset 0x2ce208:
`ND75_V12_flash_512K.bin`, SHA256
`A5168399CACA4BBD2C04A1E988F478265364577A46994199A73848ABF0AB735F`.
Это **тот же** ранее исследованный образ, не новая версия.
Идентичность: `M484,01,KB,ABT,X86HERGB,V1.00.09`.

### Pro

Четыре подписанных EXE: `pro-family-1.0.3-updater-0.0.2.exe`,
`pro-family-1.0.5a.exe`, `pro-family-1.0.8.exe`, `fw_cyan_1.1.4.exe`.
Внутри — прямой URL .bin + 64 ASCII-символа SHA256 + прошивка ASCII HEX.
`extract_pro.py` проверяет SHA256; соседние HEX-символы могут примкнуть к строке,
поэтому конец определяется проверкой хеша, а не только regex.
15 записей дают **10 уникальных образов**. Последние два EXE содержат одинаковые
пять образов. Файлы названы полным SHA256 в `pro-images/`.
`pro-images/manifest.json` содержит URL, точные размеры, хеши и offsets в EXE.
Не скачивать повторно и не считать два одинаковых пакета новыми прошивками.

Модели: NA87 Pro 1.0.3b/1.0.5a/1.0.8; MU68 Pro 1.0.3/1.0.5a/1.0.8;
ND63 1.0.5a/1.0.8; MU68 Pro Cyan и ND63 Pro Cyan 1.1.4.
В `NA87_App_v1.0.8` явно встроена строка **IROK NA87 PRO** — это не обычная NA87!

Точной обычной NA87/MU68, Ultra или Polar75 прошивки всё ещё нет.
IROK desktop installers 2.5.6.1 и 2.6.0.1 тоже сохранены, но не считать их
прошивками. Отсутствие найденного файла не означает, что прошивки не существует.

## Главный новый результат ND75: потеря событий

Протокол: report ID 1, 64 байта; subscribe `29/18/02` с 22 column masks;
unsubscribe `29/18/03`; reply `21`, subtype 1, row/col/depth в [7..9].
Все offsets в этой строке включают report ID.

Машинный код ARM Thumb:

- scanner 0x3190, преобразование глубины 0x34d2..0x34f8 ограничивает её 40;
- cache: RAM `0x20003fec + 0x124 + row*22 + col`;
- при изменении проходит direction/deadband gates, затем 0x35ae пишет cache;
- 0x35b2..0x35c6 проверяет mask: base+0x10e+col, бит row;
- 0x35c8..0x35e0 выставляет бит 22 в 0x20004a20, записывает **одну общую**
  пару row/col в base+0x10c/0x10d; очереди событий здесь нет;
- serializer 0xf9d6 читает эту пару и cache;
- scheduler 0xfafc через таблицу 0x136f0[22]=0xf9d7 вызывает serializer,
  после результата 1 снимает бит. Busy по 0x200041a0+0xb8 откладывает отправку;
- main service вызывает scheduler по адресу 0x25aa;
- scan function pointer 0x3191 найден в образе по offset 0x1ff18;
  полное расписание задач/прерываний ещё не разобрано.

**Фактическая компонентная эмуляция** `nd75_event_emulation.py` (Unicorn 2.1.4):
исполняет настоящие инструкции 0x35ae..0x35e4 и полный scheduler 0xfafc,
с искусственной RAM. Два принятых изменения (1,2)=17 и (3,4)=29:
оба значения в cache, отправлен только `01 21 00 00 00 03 01 03 04 1D`,
pending bit снят. Аналогично потерялось нулевое отпускание первой клавиши.
Все assertions PASS. В начальной версии теста забыли выставить R11 перед входом
в середину функции; исправлено, актуальный скрипт задаёт его явно.

Это НЕ полный запуск MCU/USB: доказана потеря при таком порядке исполнения,
не её частота на железе. Но обещать lossless multi-key уже нельзя.
Subscribe 0xf648 только копирует masks, unsubscribe 0xf674 их очищает.
Cache не сбрасывается, поэтому вращение подписки по одной клавише не является
доказанным способом опросить уже неподвижно удерживаемую клавишу.
29/18 subcommands 0/1 записывают настройки: не выдавать их за read-only workaround.
Обычная key processing ветка продолжается после 0x35e4; это ещё не полный аудит
доставки цифровых USB-отчётов.

## Главный результат Pro: полноценный адресуемый массив

RISC-V QingKe, app link base 0x4000. Файловые offsets + 0x4000 = runtime.
Обычный Capstone ошибочно показывает XW byte/halfword instructions как floating
point; такие `.asm` — только навигация. Правильные полные листинги `*.xw.asm`
получены LLVM 21.1.8 с **+c,+m,+xwchc**. ELF — только обёртка сырых байтов.

Логические запросы до HID padding (без Windows report ID):

```text
5C 04 12 A6 02 01 FF FF   depth half 1
5C 04 12 A6 02 02 FF FF   depth half 2
5C 04 12 A6 06 01 FF FF   ADC half 1
5C 04 12 A6 06 02 FF FF   ADC half 2
```

Ответ 132 логических байта: `5C 80 92 checksum 00 type` + 63 uint16 LE.
Две половины дают 126 позиций, не обязательно 126 физических клавиш.
Checksum=(0x35+b0+b1+b2+b[length+3])&255; для ответа последний байт 131.
В ответе нет явного номера половины: запросы не конвейеризировать без анализа.
Нулевые значения не фильтруются, чтение не является last-key stream.

NA87 Pro 1.0.8 (hash prefix 892583b1):

- dispatcher command 0x12: file 0x574a; type2 0x57c8, type6 0x584e;
- depth RAM 0x20004f6a / 0x20004fe8; ADC 0x20005066 / 0x200050e4;
- serializer 0x737a, checksum 0x7c00; три группы по 21 uint16;
- producer 0xd34e, loop 0xd654..0xd674: 6x21; missing sample 0xffff пропускается;
- ADC store 0xd4da, depth store 0xd50a. Working base 0x20004a76:
  +0x5f0 ADC, +0x4f4 depth; они совпадают с читаемыми массивами;
- caller 0xdeaa передаёт lookup runtime 0x4d54 (file0xd54);
- 0xd796..0xd7be вычисляет индекс 0..1023, затем таблица преобразования;
- 1024 монотонных значений: 0..4000. В Cyan аналогичные таблицы 0..3600.
  Не нормализовать вслепую всё семейство одним максимумом;
- transmit ring 0x4c8e: 16 slots, 15 usable, chunks по64 даже последний;
  132 байта занимают3слота. Full queue пропускает chunk (0x4cbc).
  Нужно последовательное чтение, контроль длины и отбрасывание padding.

Старый NA87 Pro1.0.3: producer0xbd7a, caller0xc94c, table file0xd64;
base0x200048ee +0x2fc depth =0x20004bea (совпадает с read pointer),
+0x3f8 ADC. Loop0xc1e4..0xc204 проходит21x6.
Для всех десяти найдены lookup->depth stores и проверены таблицы; полные offsets
в research doc и `pro-images/producer-evidence.json`.
Это не означает полный control-flow audit нормального ввода всех десяти версий.

## Конкретный план продолжения — делать без нового разрешения на исследование

### 1. ND75: найти альтернативу ненадёжным событиям (первый приоритет)

- Проследить dispatch 29 и остальные read handlers: есть ли безопасное чтение
  конкретного sensor/depth или всей RAM-таблицы через штатный read command.
- Отличать настройки actuation от текущей измеренной глубины. Подкоманда,
  принимающая row/col, сама по себе ещё не является запросом live depth.
- Допроверить call graph scanner и USB scheduler: pointer0x1ff18,
  producer0x3190, service0x25aa. Не отменять reproduced loss из-за отсутствия
  аппаратного измерения; уточнить реальные допустимые interleavings.
- Расширить эмуляцию найденных read handlers на искусственных двух/четырёх
  глубинах и отпусканиях. Проверять, не меняют ли они calibration/config.
- Если альтернативы нет в исследованных ветках — честно зафиксировать охват
  поиска. Не объявлять «невозможно вообще» до проверки других путей/версий.

### 2. Обычная NA87/MU68: продолжить добычу firmware и сравнение host code

- Сохранённый Go desktop-driver подтверждает формат row/col/byte, но не
  доказывает, что NA87 имеет ту же event-loss проблему, что ND75.
- Разобрать создание устройства, команды подписки и механизм обновления,
  сопоставить exact product identity/version с ND75. Не путать `.watch`
  (может быть reader loop) с методом установки masks.
- Попытаться найти дополнительные download/update endpoints/пакеты через
  официальный код. Не тратить время на повторное скачивание уже пустых API
  без нового параметра/источника/причины ожидать другой результат.

### 3. Pro: довести протокол до спецификации, не выпускать backend автоматически

- Полные USB descriptors: VID/PID, интерфейс, report IDs, endpoint/report sizes,
  фактическая фрагментация и padding, timeouts/неполные ответы.
- Отследить нормальные keyboard reports и любые gates во время polling;
  отличать calibration mode от read-only analog queries.
- Карты126sensor slots -> HID usages для каждой модели из официальных
  конфигов/таблиц; не переносить одну карту по похожей раскладке.
- Уточнить единицы и модельные диапазоны через таблицы и код; 4000/3600 —
  уже доказанные пределы lookup, физические миллиметры отдельно подтверждать.
- Тесты разделения половин, пропущенного/лишнего отчёта, очереди, нулей,
  model-specific normalization. Не копировать blindly web parser.
- В web bundle `CMt` собирает шесть сообщений для двух половин; нужно проверить
  его обращение с padding. Firmware queue копирует по64, поэтому нельзя просто
  сцепить все полученные байты и трактовать хвост как дополнительные клавиши.

### 4. Граница доказательств

Продолжать статический анализ и эмуляцию можно. Только реальные USB latency,
электрическое поведение датчиков, реальные simultaneous presses и конкретная
аппаратная ревизия потребуют железа. Не обещать production support сейчас.
Не менять HallJoy runtime, README, GitHub или прошивки пользователя в этой задаче.

## Команды и инструменты

PowerShell, Python `C:\Program Files\Python310`, capstone и pefile доступны.
LLVM binaries в `C:\Program Files\LLVM\bin`; 7-Zip доступен.
Unicorn установлен **локально** в `.local/research/irok-na87/python-deps`,
скрипт эмуляции сам добавляет этот каталог в sys.path.

Из HallJoy-main:

```powershell
python .local/research/irok-na87/nd75_event_emulation.py
python .local/research/irok-na87/verify_pro_protocol.py
python .local/research/irok-na87/verify_pro_producers.py
python .local/research/irok-na87/pro_full_listings.py
```

Последние три проверки/эмуляция завершились PASS. Проверки разделять:
hash/static pattern; synthetic parser fixtures; actual ARM fragment emulation.
Ни один из этих PASS не является hardware PASS.

Другие scripts: fetch.py (TLS urllib, не перезаписывает файл), extract_nd75.py,
extract_pro.py, scan_updates.py, disassemble_images.py, trace_listing.py,
go_functions.py. Последний отказывается перезаписывать уже созданные go_*.asm.
Полные общие Capstone-дизассемблеры читают и данные как код; не принимать любой
совпавший immediate за функцию, сначала проверять достижимость/таблицы/контекст.

`python -X utf8` нужен при китайских строках в stdout; read_text encoding utf-8.
Не делать read_bytes() внутри цикла по каждому байту огромного EXE — один раз
прочитать в память. Избегать rg по целой минифицированной строке (огромный вывод):
использовать Python bounded snippets. Не перезапускать vendor EXE.

## Официальные источники, уже сохранённые локально

- Live https://hid.irok.cn/assets/index-BQvZQSA6.js -> web-index-BQvZQSA6.js;
  sdk-keyboard-naj1DnVU.js -> web-sdk-keyboard-naj1DnVU.js.
- https://irok.ast.joyway.net/deviceDrivers -> deviceDrivers-current.json;
  entry26 ordinary NA87 magnetic без web support, entry32 Pro с web support.
- https://ncus.iyx.fun/deviceDrivers -> iyx-deviceDrivers.json.
- firmwareVersion/latest на обоих хостах возвращал пустые data[], сохранено.
- https://api.mall.irok.cn/v3/news?pageNum=1&pageSize=100 -> news-catalog.json;
  https://api.mall.iyx.fun/v3/news?pageNum=1&pageSize=100 -> iyx-news.json.
- /v3/news/<id>: IROK46/IYX45 дают1.0.3, IROK52/IYX61 дают1.0.8,
  IYX48 даёт1.0.5a, IYX59 даётCyan1.1.4. Последняя сохранена отдельно.
- Wayback CDX для s.joyway.net/*NA87* ранее дал пустой список.
- Go service `unpacked/$_3_/apps/driver_service/driver.service.exe`:
  SHA97D53F6447C6F5203436E2306B9C1321D1B05B473A1CD069B7104542754F2004.
  pclntab magic f1ffffff00000104. FindDevices0x7a97d0;
  handleRecive0x7aa1c0; sendDynamicKey0x7abca0; NewMagnet0x7aae40;
  connectMagnet0x7abd30; FirmwareInfo0x7ac3f0; SendData0x7aa6c0;
  SendDataSync0x7aa990. Остальные границы извлекаются go_functions.py.

## Как начать новый чат

Не пересказывать весь проект и не спрашивать, продолжать ли. Прочитать этот
handoff и три research doc, затем начать с альтернативного live-read пути ND75.
Владелец оценивает визуал сам; здесь вообще не нужны визуальные проверки.
Документировать дальнейшие находки с хешем, offset, методом и ограничениями.


## Продолжение в новой сессии: проверка альтернативных чтений ND75

См. `../research/IROK_ND75_READ_PATHS_2026-09-13.md` — новые адреса,
точный охват и результаты; читать перед повторным разбором read handlers.
`nd75_read_path_emulation.py` исполняет реальные ARM handler/scheduler:
29/18/05 возвращает настройки выбранной клавиши, не live depth; четыре
клавиши, изменение глубин и отпускания проверены, RAM write audit PASS.
Внутренняя команда 0x10 отдаёт шесть строк из pointer+0x0c, не массива глубин;
изменение всех 132 cached depths не влияет на ответы. PASS, не hardware PASS.
Подкоманды 12/13 — тоже запись настроек, не новые read commands.
Это не исчерпывающий аудит всего command table; другие read sources и
расписание scanner/service ещё требуют анализа. Потеря событий не отменена.

Go connectMagnet дополнительно проверяет `&mi_02#` в пути. FirmwareInfo
делегирует чтение в `CWitmodHid_GetHidAndFirmwareInfoStu` SDK, затем читает
строку из result+0x180 и вызывает parseVersion. Следующий полезный шаг
обычной ветки — реализация SDK и построение masks у вызывающего кода.
Созданы go_NewMagnet.asm, go_connectMagnet.asm, go_FirmwareInfo.asm, go_SendData.asm.
Прошивка обычной NA87/MU68 по-прежнему не получена; Pro не продвигался в этом
проходе. Runtime/README/релиз не менялись, устройства не открывались.


## Продолжение: критическое уточнение SDK и полный scanner

Прочитать `../research/IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md` первым.
0x29 — SDK input; wire opcode 0x21 (реальная DLL эмулирована). Полный scanner
ND75 воспроизводит четыре изменения и потерю release в одном проходе.
38 fixtures охватывают все 20 serializer entries; live cache читает event path.
SDK даёт точные строки GK8260HERGB=NA87, GK8152HERGB=MU68, X86HERGB=ND75.
Точные ordinary NA87/MU68 firmware по-прежнему отсутствуют.
Текущая просьба владельца: продолжать до блокера или исчерпания работ до железа.


## Актуальный итог продолжения до блокера

Первым читать `../research/IROK_PRE_HARDWARE_STATUS_2026-09-13.md`.
Он заменяет старый список незакрытых static TODO и содержит оставшиеся
аппаратные критерии, точные границы результатов и известный runtime opcode defect.
Pro: USB descriptors всех10 images, карты87/68/64, исправление padding indices,
serializer emulation всех10, full producer NA871.0.8, full/busy queue losses,
offline parser с200 snapshots. Нельзя объявлять Pro надёжным до USB timing tests.
Ordinary NA87/MU68: точного образа/capture нет; ND75 stream теряет releases.
Новые scripts: sdk_wire_emulation.py,go_watch_emulation.py,
nd75_full_scan_emulation.py,nd75_serializer_coverage.py,pro_riscv_emulation.py,
pro_transport_maps.py,pro_offline_parser.py,verify_irok_offline.py.
Evidence: pro-transport-maps.json,nd75-serializer-coverage.json,
offline-verification-*.json. Никакие vendor EXE не запускались и HID не открывались.


## 2026-09-13 — Новое решение: Pro заморожен, углублённое ревью ND75

Владелец попросил сохранить/заморозить NA87 Pro и копать дальше ND75/Witmod.
См. `IROK_PRO_FROZEN_2026-09-13.md`: проверенный ZIP+SHA256 manifest,71 файл.
Активно ревьюить код, эмуляцию, возможные альтернативные read paths и
обоснованность выводов об ordinary NA87. Не превращать прежний вывод
о блокере в запрет продолжать исследование; не продвигать Pro без нового решения.


## 2026-09-13 — Реальный тестировщик только NA87; один расширенный пакет

Владелец уточнил: тестировщика с ND75 нет, есть с обычной NA87.
Метаданных недостаточно: нужен один пакет, который перебирает доступные
стандартные способы обмена и проводит несколько аналоговых сценариев за один
запуск, без выдачи новой сборки под каждую гипотезу. Создаётся отдельный
инструмент tools/irok_na87_diagnostic; план и статус в его PLAN.md.
ND75 diagnostic не выдавать за NA87 test. Pro остаётся замороженным.
