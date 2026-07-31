# Временный протокол доказательной трассировки S02V1

Дата: 30 июля 2026 г.  
Схема: `1`.  
Stage marker: `S02V1`.

## Назначение

`HallJoyStabilityTrace.log` — временный инструмент приёмки конкретного пакета стабилизации. Он не заменяет штатную диагностику и не должен остаться в финальном релизе.

Цикл для каждого следующего изменения:

1. добавить минимальные события только вокруг изменяемого контракта;
2. выполнить заранее заданный Windows/hardware-сценарий;
3. проверить лог машинным анализатором;
4. при `PASS` сохранить отчёт как evidence;
5. удалить события, относящиеся к уже подтверждённому изменению;
6. только затем инструментировать следующую функцию.

`WARN` означает недостаточно выполненный сценарий, а не подтверждение стабильности. `FAIL` означает обнаруженную ошибку, аварийную ветку либо нарушение целостности лога.

## Файлы

Рядом с EXE создаются:

- `HallJoyStabilityTrace.log` — последний запуск;
- `HallJoyStabilityTrace.previous.log` — предыдущий запуск;
- `HallJoyStabilityTraceBundle.zip` — создаётся вручную через `COLLECT_STABILITY_TRACE.cmd` после штатного закрытия HallJoy.

Новый запуск заменяет только пару current/previous. Максимальный размер текущего файла — 1 МиБ.

## Производительность и отказоустойчивость

- Буфер размером 1 МиБ отображается в память заранее через `CreateFileMappingW`/`MapViewOfFile`.
- Событие не выполняет `WriteFile`, `FlushViewOfFile` или `FlushFileBuffers`.
- В worker/hot path нет динамической очереди, фонового writer-потока и периодического flush.
- Запись события — форматирование фиксированных буферов и копирование в отображённую память под одним `SRWLOCK`.
- Flush, truncate и закрытие выполняются один раз после финальной остановки worker’ов.
- При переполнении сохраняется структурированное `trace.capped`, дальнейшая запись отключается, анализ обязан вернуть `FAIL`.
- `seq` назначается под тем же lock, что и append, поэтому порядок строк и монотонная последовательность совпадают даже при конкурентной записи.

## Формат строки

```text
[timestamp][elapsed_ms=N][seq=N][pid=N][tid=N][level=INFO|WARN|ERROR|FATAL][component=name][event=name] key=value ...
```

Обязательные инварианты:

- первая структурированная строка — `main/session.start`, `seq=1`, `schema=1`, `stage=S02V1`;
- `seq` непрерывен и строго возрастает в файловом порядке;
- один `worker.start` соответствует одному `worker.exit` того же component;
- последняя структурированная строка — `main/session.end exit_code=0`;
- `ERROR`, `FATAL`, `worker.fault`, `forced_termination`, `stop.timeout` и `trace.capped` запрещены для успешной приёмки.

## Что фиксируется

Только редкие переходы и итоговые агрегаты:

- start/exit/fault worker’ов;
- start/stop и причины отказа ресурсов;
- neutralization/disconnect/reconnect;
- ViGEm init/reconnect/update failures;
- изменение Spark poll mode и row limit;
- Spark `worker.stats` после завершения поколения: количество route queries, успешных/ошибочных запросов, изменённых строк, realtime notifications и агрегированные интервалы.

## Что принципиально не записывается

- HID-коды и названия нажатых клавиш;
- аналоговые значения отдельных клавиш;
- вводимый пользователем текст;
- пути HID-устройств;
- имя пользователя, домашний каталог и произвольные file paths;
- содержимое настроек;
- per-poll/per-frame/per-key события.

VID/PID и технические размеры HID report могут присутствовать для определения активного backend’а.

## Машинная оценка

```text
python analyze_stability_trace.py HallJoyStabilityTrace.log
```

- `PASS`, exit code `0`: ошибок нет и полный SparkLink/ViGEm сценарий доказан.
- `WARN`, exit code `1`: структурной ошибки нет, но hardware-сценарий выполнен не полностью.
- `FAIL`, exit code `2`: fault, timeout, forced termination, ERROR/FATAL, неполный shutdown, потеря/перестановка событий или повреждение формата.

Для `PASS` S02V1 анализатор требует доказательства:

- успешного ViGEm init;
- SparkLink connection;
- успешных route queries;
- фактических analog changes и realtime notifications;
- всех трёх poll modes `0/1/2`;
- минимум двух row-limit values;
- stale/unplug и успешного reconnect;
- neutralization/disconnect;
- штатного завершения всех worker’ов и процесса.
