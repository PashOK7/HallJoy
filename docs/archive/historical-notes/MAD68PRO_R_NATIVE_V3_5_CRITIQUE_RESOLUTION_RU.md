# MAD 68 Pro R: проверка критики A8/A9/A0 и решение HallJoy Native Stable v3.5

## Область проверки

Проверены независимо друг от друга:

- прошивка `35ed6b7a73cfe7e7fd222e6862032e49.bin`;
- SHA-256 `2a7df4ffc491476b79da5333f51179f68fbbb5c5e146e987cfb5df764b79cead`;
- дизассемблирование RV32/WCH с image base `0x5000` и `+xwchc`;
- полный аппаратный журнал `HallJoyMAD68ProR.log`;
- критический документ `MAD68Pro_A8_A9_A0_Approach_Critique_RU.md`;
- исходники HallJoy, UAP/Soup, UI и ViGEm path.

## Исправленный итог

Последовательность `A9 -> A8 -> A9` действительно оставляет Hall telemetry включённой и возвращает normal digital path. Главное противоречие критического документа возникло из-за смешения двух разных RAM-переменных.

### Раздельные состояния

| Назначение | Адрес RAM | Действие A8 | Действие A9 |
|---|---:|---:|---:|
| service/input-suppression state | `0x20002B40` (`gp-0x760`) | `1` | `0` |
| forced sweep count | `0x20002B3F` (`gp-0x761`) | `3` | не меняет |
| telemetry gate | `0x2000580B`, bit `0x08` | устанавливает | не меняет |
| auxiliary 72-byte array | `0x20004BE4` | `0xFF` | `0x00` |

### Raw control flow

`A8` handler:

```text
0x63F8  lbu  a5,-0x760(gp)
0x63FC  bnez a5,0x62F2
0x6400  jal  0x530E
```

`0x530E`:

```text
0x531A  lbu  a4,7(a5)       ; 0x2000580B
0x5322  ori  a4,a4,0x08     ; telemetry gate
0x532E  sb   1,-0x760(gp)   ; service/input state
0x5338  sb   3,-0x761(gp)   ; forced sweep count
0x533C  sb   a4,7(a5)       ; commit telemetry bit
```

`A9` handler:

```text
0x63E0  lui  a0,0x20005
0x63EA  addi a0,a0,-0x41C   ; 0x20004BE4
0x63EE  sb   zero,-0x760(gp); clear service/input state only
0x63F2  jal  memset(...,0,72)
```

`A9` не загружает адрес `0x2000580B` и не очищает bit `0x08`.

`A0` scheduler:

```text
0xA0A8  lui  a5,0x20006
0xA0AC  lbu  a5,-0x7F5(a5) ; 0x2000580B
0xA0B0  andi a5,a5,0x08
0xA0B2  beqz a5,return
```

Следовательно, финальный `A9` восстанавливает normal digital state, но не закрывает telemetry gate.

## Почему аппаратный A0 после A9 не является очередью

Аппаратный журнал содержит:

```text
A8 ACK       06:22:26.751
final A9 TX  06:22:26.751
final A9 ACK 06:22:26.761
```

После этого получены:

- 272 пакета `A0` после final A9 TX;
- 271 пакет после final A9 ACK;
- четыре точных упорядоченных цикла по 68 дескрипторов после final A9 TX;
- три полных цикла после final A9 ACK;
- медианный интервал 15 мс;
- конец четвёртого цикла через 4.236 секунды после ACK A9.

За 7–10 мс между A8 и final A9 прошивка с limiter 15 мс не могла заранее сформировать 272 пакета. Windows HID queue также не может содержать пакеты, которые firmware ещё физически не сформировала. Гипотеза очереди для этого журнала исключена.

## Почему наблюдаются четыре цикла, хотя A8 задаёт три

В forced mode scheduler использует count=3 и последовательно отправляет slots 0..71. После трёх проходов он переходит в steady comparison mode. Mirror/source arrays ещё различаются, поэтому steady branch делает дополнительный полный catch-up цикл по тем же 68 физическим descriptors. После синхронизации mirror поток замолкает до нового изменения.

## Что критический документ установил правильно

### Ограниченная пропускная способность

Limiter равен 15 timer ticks. При tick около 1 мс:

- общий максимум около 66.7 packet/s;
- один пакет содержит одну клавишу;
- forced/catch-up цикл 72 slots занимает около 1.08 с;
- steady scheduler отправляет только первый изменившийся slot за проход;
- поиск каждый раз начинается с slot 0.

Следствие: при одновременно меняющихся клавишах низкий scanner slot может задерживать высокий. Особенно важна карта WASD:

- A: slot 9;
- D: slot 10;
- S: slot 55;
- W: slot 64.

При постоянном изменении A/D пакеты S/W могут задерживаться. Это ограничение stock firmware, а не HallJoy.

### Необходим post-sweep тест

Старый журнал доказал forced/catch-up циклы и настоящие analog values, но не содержит физического нажатия, начатого спустя 5+ секунд после A8. Поэтому hardware steady branch ещё не подтверждена отдельным тестом.

### Нужно разделять ACK и Hall-report

Parser HallJoy различает:

- control response `AA/AB/5F`;
- asynchronous Hall marker `A0` с известным 3-byte descriptor.

Host opcode `A0` backend не отправляет. Runtime allow-list допускает только `A8` и `A9`.

## Аппаратно подтверждённое analog field

`A0[4..5]`, big-endian u16:

```text
raw = (packet[4] << 8) | packet[5]
normalized = clamp(raw,0,1600) / 1600
```

В журнале:

- A: примерно `5 -> 1565 -> 0`;
- W: `0 -> 1600 -> 0`;
- descriptor изменяется именно для физической клавиши;
- все 68 descriptors совпадают с firmware table;
- checksum/malformed ошибок нет.

## Изменения HallJoy Native Stable v3.5

### Startup snapshot не считается steady proof

- `68/68` включает только `Emergency WASD`.
- Full 67-key ownership разрешается только после нового A0, коррелированного с физическим Raw Input edge после завершения startup window.
- Решающий marker: `STEADY-STATE A0 CONFIRMED`.

### Generation-based edge correlation

Raw Input callback фиксирует analog sample sequence/raw до release-публикации digital sequence. После фронта MAD68 владеет HID usage только если:

- пришёл более новый A0 sequence; либо
- немного опережающий packet уже соответствует новому digital state.

Timestamp сам по себе больше не может сделать stale value авторитетным.

### Per-key fallback

Если конкретная клавиша не получила свежий A0:

- только этот HID usage возвращается UAP/digital fallback;
- остальные живые MAD68 keys продолжают работать;
- A8 не повторяется из-за одного starved high slot;
- новый recovery запускается только при смерти всего A0 transport.

Это предотвращает зависший ViGEm axis и не ухудшает scheduler повторными forced sweeps.

### Emergency WASD

После свежих W/A/S/D startup samples включается аварийный режим. Значения проходят штатный путь HallJoy:

```text
MAD68 A0 -> ReadRaw01Cached -> curves/filter -> g_uiAnalogM
          -> standard blue keyboard bar
          -> remap -> BuildReportForPad -> ViGEm Bus
```

Никакого отдельного тестового progress bar или отдельного ViGEm path нет.

### UAP/Wooting сохранены

- обычные UAP targets не изменены;
- dedicated native UAP исключает только `VID 373B / PID 1109`;
- исключение выполняется до Soup `CreateFileW`, чтобы второй процесс никогда не открывал EP82;
- остальные Wooting/UAP keyboards продолжают работать;
- исходная HallJoy policy `UAP_DISABLE_HOTPLUG=1` сохранена.

### Startup order

Порядок:

1. embedded UAP preparation;
2. Backend/UAP initialization;
3. target-scoped Raw Input registration;
4. MAD68 native worker;
5. A9/A8/A9 activation.

Это закрывает окно, когда A8 мог начаться до регистрации digital edges. Дополнительно `GetAsyncKeyState` блокирует A8, если клавиша удерживалась ещё до запуска HallJoy.

### Crash recovery

- normal shutdown отправляет best-effort primary A9;
- отдельный exit watchdog после завершения HallJoy ещё раз отправляет идемпотентный A9;
- hard allow-list блокирует все opcode кроме A8/A9;
- active commands разрешены только для `bcdDevice 0x0102`.

Telemetry bit после A9 остаётся включённым до config reinitialization/power reset. Это штатное следствие найденной машины состояний. A9 нужен для восстановления digital service state, а не для выключения telemetry.

### Диагностика scheduler

Каждые пять секунд логируются:

- aggregate A0 rate;
- число gaps >50 мс;
- максимальный A0 gap;
- raw/age/ownership/digital state W/A/S/D;
- coverage, ordered cycles, publish mode;
- per-edge latency и semantic match;
- per-key starvation и global-transport death отдельно.

## Что следующая аппаратная проверка должна решить

1. Запустить HallJoy и не нажимать клавиши минимум 5 секунд.
2. Медленно нажать/отпустить W, A, S, D отдельно.
3. Проверить marker `STEADY-STATE A0 CONFIRMED`.
4. Проверить штатные синие depth bars.
5. Назначить WASD на ViGEm stick/trigger.
6. Проверить пары A+D, W+S и затем четыре WASD одновременно.
7. Проанализировать per-key latency, ownership fallback и A0 rate/gaps.

## Честный production verdict

Доказано:

- A9 не выключает telemetry;
- A9/A8/A9 даёт полный startup stream после final A9;
- A0[4..5] является настоящей analog coordinate 0..1600;
- все 68 descriptors известны;
- normal blue UI и ViGEm route интегрированы;
- UAP/Wooting не сломаны;
- stale per-key values не удерживают ViGEm.

Не доказано аппаратно:

- частота и fairness steady event branch после startup catch-up;
- плавность четырёх одновременно движущихся WASD.

Если post-sweep test не даст новых correlated A0, stock path непригоден и потребуется firmware patch. Если даст, но S/W сильно starve при одновременном движении, stock path останется рабочим для одиночных/стабильных состояний, а качественный multi-key analog потребует нового плотного firmware report.
