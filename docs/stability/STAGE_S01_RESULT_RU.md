# Результат S01 — lifecycle contracts and test scaffolding

Дата: 30 июля 2026 г.  
Статус: ACCEPTED для portable gate; Windows/MSVC build не выполнялся.

## 1. Граница пакета

S01 вводит только общие lifecycle-типы, тестируемую state machine и platform seam. Ни один существующий backend, worker, registry callback или runtime start/stop path не использует новые заголовки.

Это намеренное ограничение: пакет создаёт доказуемый контракт перед последующими миграциями, но не смешивает его с изменением поведения.

## 2. Добавленные контракты

### `worker_lifecycle.h`

- `WorkerState`: `Stopped`, `Starting`, `Running`, `StopRequested`, `Joined`, `Faulted`, `Poisoned`;
- сильный монотонный `GenerationId`, где `0` означает отсутствие поколения;
- allocation-free `LifecycleError` с operation, from/requested state, supplied/active generation и native error;
- `StartResult`, `StopResult`, `TransitionResult` с безопасными predicates;
- `WorkerLifecycle` с именованными операциями start/run/stop/join/fault/poison.

### `worker_primitives.h`

Интерфейс `WorkerPrimitives` изолирует:

- monotonic clock;
- create/signal/wait stop primitive;
- thread start;
- bounded join;
- явное закрытие thread/wait token.

Интерфейс не содержит Win32 типов и тестируется fake-реализацией.

## 3. Ключевые инварианты

1. Generation id увеличивается при каждой принятой попытке start и не переиспользуется после start failure.
2. Stale или нулевое поколение не может изменить активное состояние.
3. `Joined` подтверждает завершение конкретного поколения; `StopRequested` этого не подтверждает.
4. `Poisoned` не является `Stopped`, запрещает restart и не маскирует timeout как success.
5. Первый worker fault сохраняется как авторитетная диагностика до следующего поколения.
6. `FailStartBeforeWorker` допустим только когда гарантировано, что thread не создан и живых ресурсов поколения нет.
7. State machine не выполняет OS-операции; их результат поступает через отдельный seam.

## 4. Проверенная матрица переходов

Изменяющие состояние переходы:

```text
Stopped       -> Starting
Starting      -> Stopped | Running | StopRequested | Faulted | Poisoned
Running       -> StopRequested | Faulted | Poisoned
StopRequested -> Joined | Faulted | Poisoned
Joined        -> Starting
Faulted       -> StopRequested | Joined | Poisoned
Poisoned      -> <none>
```

Unit test проверяет все 49 пар `from/to`; idempotent named calls проверяются отдельно.

## 5. Regression evidence

- pre-change GCC static/portable gate: PASS;
- final GCC static/portable gate: PASS;
- final Clang static/portable gate: PASS;
- 9 static audits: PASS;
- 8 portable C++ tests: PASS;
- новые тесты на GCC 14.2 и Clang 17 с `-Werror`, ASan и UBSan: PASS;
- `HallJoy.vcxproj` и `.filters` parse: PASS;
- список `ClCompile`: 58 до и 58 после, без изменений;
- изменённых существующих production `.cpp/.h/.inc`: 0;
- удалённых production файлов: 0;
- новых production object files: 0;
- production translation units, подключающих новые headers: 0.

Подробные логи находятся в `docs/stability/tests/S01_*`.

## 6. Что намеренно не исправлено

- `TerminateThread` и существующие join/timeout paths;
- backend registry stop contract;
- worker exception boundaries;
- Addressed overlapped I/O ownership;
- analog-host generation ownership;
- UAP lifecycle.

Поэтому все 45 production-рисков остаются `Open`.

## 7. Ограничения проверки

В текущей Linux-среде недоступна полноценная MSVC/Windows executable build и hardware fault matrix. Новые headers независимо скомпилированы двумя C++20 compiler families, но их первая Windows runtime-интеграция потребует отдельного Gate W.

## 8. Следующий небольшой пакет

`S02 — exception barriers`, первый подшаг: только realtime/debug/overlay worker entry wrappers. Алгоритмы, scheduling и shutdown в этом подшаге не меняются.
