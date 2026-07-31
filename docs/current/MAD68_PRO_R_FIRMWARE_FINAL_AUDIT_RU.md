# MADLIONS MAD 68 Pro R — финальный критический аудит Hall/analog-протокола

**Исследованный образ:** `35ed6b7a73cfe7e7fd222e6862032e49.bin`  
**SHA-256:** `2a7df4ffc491476b79da5333f51179f68fbbb5c5e146e987cfb5df764b79cead`  
**Размер:** `124656` байт (`0x1E6F0`)  
**База загрузки:** `0x5000`  
**Статус:** этот документ заменяет Stage 1, Stage 2 и предыдущий Deep Audit.

---

## 1. Финальный вывод

Для этого точного бинарного образа live-аналог извлекается так:

1. открыть vendor HID interface 1 устройства `VID 373B / PID 1109`;
2. читать 64-байтовые interrupt-IN отчёты endpoint `0x82`;
3. принимать асинхронные пакеты, у которых `byte[0] == 0xA0`;
4. идентифицировать клавишу по трём байтам `byte[1..3]`;
5. читать основное аналоговое значение:

```text
raw = (packet[4] << 8) | packet[5]       // big-endian u16
raw = clamp(raw, 0, 1600)
normalized = raw / 1600.0
```

**Основное исправление предыдущей документации:** байты `11..13` не являются основным ходом клавиши. Они сериализуют адаптивный per-key коэффициент калибровки, используемый scan-кодом как делитель. Для HallJoy нужно использовать **bytes `4..5`**.

В этой прошивке нет второго host-visible live Hall-пути:

- нет команды чтения Hall-канала по адресу;
- нет команды чтения live Hall RAM;
- нет vendor control transfer через EP0;
- нет HID Feature report;
- нет analog axes в стандартных HID-интерфейсах;
- framing `0x5F` использует ту же command table;
- нет второго producer’а Hall-данных для USB IN;
- прямые обращения к USB-регистрам и все interrupt roots классифицированы.

Таким образом, для этого SHA протокол HallJoy должен строиться вокруг `A0` async stream.

---

## 2. Что было неверно или недостаточно строго раньше

### 2.1 Неверно выбранное поле аналога

Предыдущий Deep Audit называл:

```text
packet[11] + packet[12]/10 + packet[13]/100
```

динамической analog metric. Более глубокий dataflow это опроверг.

Структура по `0x200040EC + 8*slot` содержит float, который scan routine загружает перед `fdiv.s` и использует для масштабирования Hall delta. Позже state machine обновляет тот же float как величину, связанную с полным диапазоном, делённым на `1600.0`. Значение по умолчанию — приблизительно `0.39`.

Следовательно:

- bytes `11..13` = десятичное представление **коэффициента калибровки/масштаба**;
- bytes `4..5` = результат применения этого коэффициента к текущему Hall delta;
- bytes `4..5` ограничиваются прошивкой значением `1600` и являются пригодной live-координатой.

### 2.2 Недостаточно полный аудит USB

Предыдущий документ доказывал отсутствие второго producer’а главным образом через семь прямых вызовов общего sender `0x545A`. Этого недостаточно: теоретически код мог отправлять данные через EP0, напрямую писать USB DMA/endpoint registers или вызываться косвенно.

В финальном проходе дополнительно проверены:

- весь USB ISR `0x558E` и EP0 request dispatch;
- standard USB request table `0xF1D4`;
- descriptor dispatch `0xF204`;
- HID class request table `0xF290`;
- все инструкции, загружающие базу USB peripheral `0x40023xxx`;
- endpoint/DMA initialization;
- endpoint watchdog/recovery;
- interrupt vector table;
- непрямые переходы и literal function pointers;
- все producer’ы EP81/EP82/EP83.

Необъяснённых USB-register roots не осталось.

### 2.3 Небезопасная fire-and-forget активация

Прошивка имеет один RX software buffer и один pending flag, но не очередь vendor-команд. Поэтому отправка `A8` и `A9` подряд без ожидания ответов могла приводить к перезаписи или потере команды.

Правильная схема должна быть сериализованной и подтверждаемой.

---

## 3. Полнота дизассемблирования

Образ дизассемблирован с правильной базой `0x5000` и WCH/QingKe extension `+xwchc`. Это устранило `<unknown>` для нестандартных compressed byte/halfword load/store инструкций.

В критических диапазонах нет ни одной нераспознанной инструкции:

- `A8/A9` и state helper;
- USB init/sender/ISR/EP0;
- vendor parser и все handlers;
- USB scheduler;
- config initialization;
- `A0` builder/service;
- основной Hall scan/state machine.

Это важно: вывод о bytes `4..5` основан не на предположении из структуры пакета, а на полностью декодированной цепочке инструкций.

---

## 4. USB topology

### Device

```text
VID         0x373B
PID         0x1109
bcdDevice   0x0102
Product     MAD 68 Pro R
```

### Interfaces

| IF | Назначение | Endpoints |
|---:|---|---|
| 0 | Boot keyboard | `0x81 IN`, 8 bytes |
| 1 | Vendor HID | `0x82 IN`, `0x04 OUT`, 64 bytes |
| 2 | Composite keyboard/mouse/consumer/system | `0x83 IN` |

Vendor report descriptor объявляет 64-byte Input и 64-byte Output. Feature report отсутствует.

### EP0

EP0 реализует стандартные USB и ограниченные HID class requests. В частности:

- HID `GET_REPORT` приводит к STALL;
- `SET_REPORT` не открывает отдельный data handler;
- vendor request type `0x40` не имеет отдельного dispatcher;
- произвольного чтения памяти или Hall-состояния через control transfer нет.

---

## 5. Vendor framing

64-байтовый OUT request:

| Byte | Назначение |
|---:|---|
| 0 | `0x55` normal request |
| 1 | opcode |
| 2 | XOR key, `0` = disabled |
| 3 | checksum |
| 4 | payload length, max `0x38` |
| 5..6 | offset/index LE |
| 7 | parameter/flags |
| 8.. | payload |

Checksum:

```text
sum(packet[4 .. 7+length]) & 0xFF
```

Ответы:

- `0xAA` — normal response;
- `0xAB` — checksum error;
- неизвестная команда — default handler с `unknw`.

Header `0x5F` проходит к той же jump table `0xF2C0`; второго набора команд за ним нет.

---

## 6. Полный command dispatch

Jump table содержит ровно 243 opcode `00..F2`:

- 30 активных;
- 213 указывают на default handler;
- `A8` → handler `0x63F8`;
- `A9` → handler `0x63E0`.

Все остальные read-команды обращаются к фиксированным config/NVM-окнам либо к отдельным runtime buffers. Ни одна не читает массивы live Hall pipeline (`0x20003CFC..0x200047F4`).

Особенно важно:

- `A0` как **host opcode** — чтение config/NVM `0x20400`, а не live packet request;
- `0xA0` как **первый байт async IN packet** — другой смысл, packet header;
- `DD/DE` используют отдельные RAM-окна `0x200052CC` и `0x2000514C`;
- `0B` является data-flash writer, а не безопасным RGB/live buffer;
- `EE` связан с factory/maintenance path и не должен пробоваться.

Полная таблица приложена в `command_map_final.csv`.

---

## 7. Доказанная Hall dataflow

Основная цепочка:

```text
physical Hall scan / filtered sample
        ↓
raw/current samples       0x20003CFC[72]
baseline/reference        0x20003EAC[72]
filter accumulator        0x20003F3C[72]
per-key scale record      0x200040EC + 8*slot
        ↓
abs(filtered - baseline) / per_key_scale
        ↓ cap and rounding
primary live coordinate   0x2000444C[72], u16, 0..1600
        ↓
A0 builder 0x9F30
        ↓
async buffer gp+0x358
        ↓
USB scheduler 0x7AA4/0x7AB0
        ↓
IF1 endpoint 0x82
```

### Scan equation

В двух ветвях полярности код сначала получает модуль отклонения фильтрованного Hall sample от baseline. Затем:

```text
scaled = abs(filtered_sample - baseline) / per_key_scale
```

После этого:

- значение преобразуется в unsigned integer;
- дробная часть может округлять результат вверх при внутреннем пороге `0.6`;
- результат записывается в `0x2000444C[slot]`;
- значение `>=1600.0` принудительно записывается как `0x0640`.

Поэтому bytes `4..5` имеют доказанный internal domain `0..1600`.

Не доказано, что единица равна сотым миллиметра или другой физической величине. Для HallJoy это не требуется: нормализация `raw/1600` корректна относительно внутреннего диапазона firmware.

### Направление

Обе ветви полярности превращаются в положительную величину отклонения от baseline. Поэтому обычное ожидаемое направление:

```text
baseline/rest ≈ 0
maximum deviation → 1600
```

Точная механическая точка полного хода зависит от сохранённой per-key calibration.

---

## 8. Формат async пакета `A0`

Builder `0x9F30` записывает только bytes `0..19`. Bytes `20..63` перед отправкой не очищаются и должны игнорироваться.

| Bytes | Формат | Источник/смысл |
|---:|---|---|
| 0 | `u8` | `0xA0` header |
| 1..3 | 3 bytes | фиксированный descriptor клавиши из table `0xFB74` |
| 4..5 | `u16 BE` | **primary live analog `0..1600`** |
| 6 | `u8` | high byte secondary quantized/gate value |
| 7 | `u8` | условный threshold/secondary helper |
| 8 | `u8` | auxiliary low byte |
| 9 | `u8` | auxiliary state byte |
| 10 | `u8` | state field per-key record |
| 11..13 | decimal bytes | **calibration scale**, не primary analog |
| 14..15 | `u16 BE` | выбранный actuation threshold, тот же internal domain |
| 16..17 | `u16 BE` | auxiliary live word |
| 18..19 | `u16 BE` | baseline/reference Hall sample |
| 20..63 | undefined | stale tail, игнорировать |

### Декодирование для HallJoy

```cpp
if (report.size() == 64 && report[0] == 0xA0) {
    const uint16_t raw =
        (static_cast<uint16_t>(report[4]) << 8) |
         static_cast<uint16_t>(report[5]);

    const float analog =
        std::min<uint16_t>(raw, 1600u) / 1600.0f;
}
```

Поля `11..13` можно сохранять в диагностический лог, но нельзя использовать как значение оси.

---

## 9. Адресация клавиш

Packet builder всегда использует фиксированную descriptor table:

```text
VA 0xFB74
72 entries × 3 bytes
```

Пустые scanner slots:

```text
8, 16, 35, 59
```

Остаётся 68 уникальных дескрипторов. Каждый packet можно связать с клавишей по полным bytes `1..3`; полагаться только на HID usage не следует, потому что modifier/Fn имеют отдельное кодирование.

Начальные forced sweeps проходят все 72 slots. Builder пропускает entries с нулевым первым байтом descriptor, поэтому каждый полный sweep выдаёт 68 физических клавиш.

---

## 10. Как firmware решает, когда отправить пакет

После forced sweeps stream становится change-driven.

Service `0xA0A8` сравнивает:

```text
current trigger/source  0x20004644[slot]
mirror                  0x2000456C[slot]
```

Значения trigger `<=4` нормализуются к нулю. При изменении firmware обновляет mirror и вызывает builder для этого slot. Builder при этом берёт **актуальное высокоразрешённое** значение из `0x2000444C`.

Secondary value формируется из primary coordinate через внутреннюю таблицу/поиск (`0x9E6A`) и используется как событие обновления. Следствие для HallJoy:

- protocol является event stream, а не host polling;
- после initial snapshot нужно хранить последнее значение каждой клавиши;
- частота событий определяется firmware quantization/gating;
- нельзя ожидать 68 значений в каждом USB report.

---

## 11. Активация `A8/A9`

### `A8`

Если suppression flag уже не установлен, `A8`:

1. устанавливает telemetry latch bit 3 в `0x2000580B`;
2. устанавливает input-suppression flag `gp-0x760 = 1`;
3. устанавливает forced-sweep counter `gp-0x761 = 3`;
4. заполняет `0x20004BE4[72]` значением `0xFF`;
5. вызывает baseline/filter initializer `0xA15A`.

### `A9`

`A9`:

1. очищает input-suppression flag;
2. очищает `0x20004BE4[72]`;
3. не очищает telemetry latch;
4. не очищает forced-sweep counter напрямую.

### Важный риск baseline

`A8` вызывает initializer, который копирует текущие Hall samples в baseline и инициализирует фильтр. Если пользователь удерживает клавишу во время `A8`, её текущее положение может стать новой baseline.

Следовательно, будущая HallJoy должна активировать stream только когда стандартный keyboard HID сообщает «все клавиши отпущены». Это не ручной пользовательский тест, а автоматическая предосторожность backend’а.

### Рекомендуемая сериализованная state machine

```text
1. Passive listen for valid A0 packets.

2. Убедиться, что нет нажатых клавиш.

3. Send A9.
   Wait for a valid AA response whose opcode is A9.

4. Send A8.
   Wait for a valid AA response whose opcode is A8.

5. Send A9.
   Wait for a valid AA response whose opcode is A9.

6. Validate A0 stream:
   - header A0;
   - known descriptor;
   - raw <= 1600;
   - initial coverage converges toward 68 unique descriptors.
```

Первые восемь bytes запросов при zero payload:

```text
A8: 55 A8 00 00 00 00 00 00
A9: 55 A9 00 00 00 00 00 00
```

Остальные bytes 64-byte report должны быть нулевыми. При length `0` checksum равен `0`.

### Почему ответ команды не должен смешиваться с stream

Control response и async packet имеют отдельные buffers и flags. USB scheduler проверяет control response flag раньше async flag. Даже если на следующем main-loop iteration service успеет подготовить `A0`, pending `AA/A8` или `AA/A9` имеет приоритет на EP82.

Host всё равно обязан демультиплексировать:

```text
AA/AB → command transaction
A0    → Hall event
```

### Чего нельзя обещать

Нельзя статически гарантировать отсутствие единственного потерянного keyboard event между выполнением `A8` и подтверждённым `A9`: `A8` явно включает suppression. Backend должен минимизировать окно, логировать его и всегда иметь recovery `A9`.

---

## 12. Почему это единственный способ получить live analog

Финальный аудит исключает альтернативы на пяти уровнях.

### 12.1 Полная command table

Все 243 entries прочитаны непосредственно из firmware. Активных handlers вне 30 известных нет. Ни один read handler не адресует live Hall arrays.

### 12.2 EP0 и HID class

Нет vendor EP0 API, Feature report и GET_REPORT path для Hall data.

### 12.3 Все interrupt-IN producers

Общий sender `0x545A` имеет ровно семь call sites:

```text
0x7A10  EP81 boot keyboard
0x7A50  EP83 mouse
0x7A78  EP83 NKRO
0x7A98  EP83 consumer
0x7AB0  EP82 async gp+0x358
0x7ACC  EP83 system
0x7AE4  EP82 control gp+0x12C
```

Для EP82 существует два producer path:

- control `AA/AB`;
- async `A0`, чей единственный builder — `0x9F30`.

### 12.4 Прямые USB-register accesses

Все 24 инструкции `lui ..., 0x40023` находятся внутри классифицированных функций:

- endpoint/DMA init;
- attach/init;
- interrupt-IN sender;
- USB ISR/EP0/EP4 OUT;
- USB reset/pull-up;
- endpoint watchdog/recovery.

Неклассифицированных roots — ноль.

### 12.5 Interrupts и indirect paths

Проверены реальные vector roots:

- `0x558E` USB IRQ;
- `0x7AF4` timer IRQ;
- `0xB01A` scan/peripheral IRQ;
- `0xDEF8` другой peripheral IRQ;
- `0x66EA` main/reset path.

Не-USB IRQ handlers не обращаются к USB peripheral base. В firmware отсутствуют literal pointers на USB sender, scheduler или Hall packet builder; известные indirect dispatch tables классифицированы.

Итог: альтернативный скрытый USB transport, обходящий изученный `A0` builder, для этого image не обнаруживается и противоречил бы полной карте USB register accesses.

---

## 13. Уровни уверенности

### Доказано для точного SHA

- правильная база `0x5000`;
- USB topology и отсутствие Feature report;
- полный dispatcher `00..F2`;
- отсутствие адресного live Hall read opcode;
- `A8/A9` state split;
- `A0` builder/service и endpoint `0x82`;
- descriptor map 72/68;
- primary analog в bytes `4..5`;
- internal range `0..1600`;
- bytes `11..13` являются scale coefficient;
- initial 3 sweeps, затем change-driven stream;
- bytes `20..63` undefined;
- единственность host-visible live Hall path в этом image.

### Высокая уверенность, но требуется интеграционная проверка на устройстве

- реальный USB cadence под Windows/Linux;
- число потерянных keyboard events в кратком suppression window;
- поведение при sleep/resume, unplug/replug и конкуренции с официальным драйвером;
- корректность этого же протокола на других bcdDevice/PID/firmware revisions.

### Не следует утверждать

- что `raw/1600` является физическими миллиметрами;
- что activation полностью бесшовна;
- что другие MAD 68 Pro R revisions имеют идентичный binary protocol;
- что packet tail `20..63` содержит стабильные данные.

---

## 14. Требования к будущему HallJoy backend

Без внешних Python-инструментов backend должен:

1. fingerprint устройство и interface topology;
2. открыть IF1 `EP82/EP04` независимо от keyboard HID;
3. сначала пассивно слушать `A0`;
4. автоматически дождаться состояния all-keys-up;
5. выполнить сериализованное `A9 → A8 → A9`, ожидая соответствующий `AA` после каждой команды;
6. параллельно демультиплексировать `AA/AB/A0`;
7. использовать `BE16(packet[4..5]) / 1600`;
8. хранить state по полному 3-byte descriptor;
9. проверить initial coverage 68 ключей;
10. игнорировать bytes `20..63`;
11. при ошибке/timeout повторно отправить `A9` и не выполнять неизвестные opcode;
12. записать self-diagnostic log: enumeration, descriptors, OUT frames, ACKs, A0 validation, coverage, invalid values, timeouts и recovery.

Это уже не «пробник»: протокольная стратегия и analog field статически определены. Аппаратная интеграция нужна для проверки реализации и ревизии устройства, а не для угадывания, где находится аналог.

---

## 15. Краткая спецификация

```text
Device:          VID 373B, PID 1109, bcdDevice 0102
Interface:       1
OUT endpoint:    04, 64 bytes
IN endpoint:     82, 64 bytes
Arm opcode:      A8
Recovery opcode: A9
Async header:    A0
Key ID:          packet[1..3]
Analog raw:      BE16 packet[4..5]
Internal range:  0..1600
Normalized:      clamp(raw,0,1600)/1600.0
Initial state:   3 sweeps × 72 slots, 68 emitted descriptors per sweep
Steady state:    one changed key per eligible service event
Ignore:          packet[20..63]
Do not use:      packet[11..13] as analog
```

---

## 16. Воспроизводимые материалы

`mad68pr_final_static_audit.py` не подключается к клавиатуре. Он проверяет firmware SHA и непосредственно по binary/disassembly подтверждает:

- jump tables;
- EP0 tables;
- interrupt vectors;
- все USB sender call sites;
- все USB peripheral roots;
- отсутствие relevant unknown instructions;
- `A8/A9` state operations;
- builder byte mapping;
- scan division/cap/store;
- calibration-scale updates;
- scheduler priority;
- descriptor map и thresholds.

Результат аудита должен завершаться `status: ok`. Таблицы и точные листинги критических диапазонов входят в пакет.
