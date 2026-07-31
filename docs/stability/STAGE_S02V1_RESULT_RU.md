# S02V1 — временная доказательная трассировка изменений

Дата: 30 июля 2026 г.  
База: S02B.3 Addressed exception boundaries.  
Статус: локальная реализация и V0 завершены; Windows/MSVC + SparkLink evidence gate ожидается.

## Причина этапа

Ручного сообщения «всё работает» недостаточно для закрытия runtime-пакета. Нужен ограниченный лог, по которому можно независимо проверить фактическое выполнение сценария, отсутствие fault/timeout/forced termination и корректный shutdown.

S02V1 не исправляет новый runtime-риск и не закрывает пункты risk register. Это временная измерительная инфраструктура для приёмки уже внесённых S02-изменений и последующих малых пакетов.

## Реализация

Добавлены:

- `stability_trace.h/.cpp` — compile-time включаемая трассировка stage `S02V1`;
- `analyze_stability_trace.py` — детерминированный анализ `PASS/WARN/FAIL`;
- `stability_trace_static_audit.py` — проверка bounded/mapped дизайна и synthetic good/bad/incomplete traces;
- `COLLECT_STABILITY_TRACE.cmd` и `collect_stability_trace.ps1` — сбор current/previous logs в один ZIP только после закрытия HallJoy;
- отдельная документация протокола и privacy/performance contract.

Build включает трассировку только через `/p:HallJoyStabilityTrace=true`. Без `HALLJOY_STABILITY_TRACE` вызовы компилируются в no-op.

## Инструментированные области

- main session и final shutdown;
- realtime worker;
- overlay worker;
- backend init/shutdown;
- ViGEm init/reconnect/update failures;
- SparkLink start/stop/fault/hotplug и итоговые счётчики поколения;
- MAD68, Hex80 и Addressed worker boundaries;
- изменения Spark poll mode и row limit.

Поллинг, packet layout, normalization, routing, output scheduling и значения управления не изменены.

## Защита производительности

Первоначально рассмотренная синхронная файловая запись отклонена: `WriteFile` из completion/fault пути мог задержать `join()` при проблемном диске.

Финальный вариант:

- один заранее выделенный 1 МиБ memory-mapped buffer;
- никакого disk I/O/flush на событии;
- нет per-poll или per-key событий;
- единичный final flush после `App_ForceFinalShutdown`;
- bounded overflow с обязательным `FAIL`.

## Дополнительные дефекты, найденные при разработке трассировки

1. Sequence первоначально назначался до writer-lock. Конкурентные потоки могли физически записать `seq=2` до `seq=1` и создать ложный FAIL. Исправлено: sequence и append находятся под одним lock.
2. Первоначальный cap marker не содержал обязательного structured `seq` и мог быть проигнорирован анализатором. Исправлено: cap marker структурирован, reserve гарантирует место, а trace отключается.
3. Простого connection event недостаточно для доказательства теста. Добавлен `worker.stats` и требования к режимам, row limit, input changes, hotplug и ViGEm.

## Локальный regression gate

- все static audits: PASS;
- все portable C++ tests: PASS;
- GCC C++20: PASS;
- Clang C++20: PASS;
- Python syntax: PASS;
- MSVC project/filter XML parse: PASS;
- synthetic analyzer PASS/WARN/FAIL tests: PASS;
- GCC/Clang ASan + UBSan с `-Werror`: PASS; clean-package gate выполняется при финальной упаковке.

## Windows/hardware acceptance

После чистой x64 Release сборки требуется один доказательный SparkLink-сеанс:

1. активный analog input не менее 30 секунд;
2. Safe, Fast Yield, Max Burst;
3. минимум два row-limit значения, включая Unlimited/0;
4. unplug при удерживаемой analog key;
5. reconnect и повторная реакция ViGEm;
6. штатное закрытие HallJoy;
7. сбор `HallJoyStabilityTraceBundle.zip`.

Архив лога анализируется локально помощником. Только `PASS` переводит текущий evidence gate в подтверждённый. При `WARN` тест повторяется; при `FAIL` следующий feature-пакет не начинается.

## План удаления

После успешного evidence gate:

- сохраняется текстовый отчёт анализатора и SHA-256 исходного log bundle;
- удаляются события, относящиеся только к уже подтверждённым S02 boundaries;
- минимальное ядро временной трассировки может быть переиспользовано для следующего пакета с новым stage marker;
- перед финальным релизом temporary trace, analyzer build flag и collector полностью удаляются, если отдельным решением не будет утверждён opt-in support trace.
